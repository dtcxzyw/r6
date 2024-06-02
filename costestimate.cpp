// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2024 Yingwei Zheng
// This file is licensed under the Apache-2.0 License.
// See the LICENSE file for more information.

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/FloatingPointMode.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/Analysis/DomConditionCache.h>
#include <llvm/Analysis/SimplifyQuery.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/PatternMatch.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRPrinter/IRPrintingPasses.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Local.h>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <unordered_map>

using namespace llvm;
namespace fs = std::filesystem;

static cl::opt<std::string>
    InputDir(cl::Positional, cl::desc("<directory for input LLVM IR files>"),
             cl::Required, cl::value_desc("inputdir"));

constexpr uint64_t Cost = 0;
constexpr uint64_t LoadStoreCost = 4;
constexpr uint64_t JumpCost = 1;
constexpr uint64_t MulCost = 3;
constexpr uint64_t DivCost = 12;
constexpr uint64_t FDivCost = 30;
constexpr uint64_t FMulCost = 5;
constexpr uint64_t FCheapOpCost = 3;
constexpr uint64_t GlobalCost = 2;
constexpr uint64_t BitCountCost = 3;
constexpr uint64_t UnsupportedCost = 1000;

class CostEstimator final : public InstVisitor<CostEstimator> {
private:
  uint64_t Cost = 0;
  Module &Mod;
  SimplifyQuery SQ;
  SmallPtrSet<Value *, 16> RequestedValues;
  void addOperands(Instruction &I, uint64_t K) {
    addCost(K);
    for (Value *V : I.operands())
      RequestedValues.insert(V);
  }
  void addCost(uint64_t K = 1) { Cost += K; }

public:
  explicit CostEstimator(Module &M) : Mod{M}, SQ(Mod.getDataLayout()) {}

  void visitUnaryOperator(UnaryInstruction &I) {
    assert(I.getOpcode() == Instruction::FNeg);
    addOperands(I, 1);
  }
  void countMul(Value *LHS, Value *RHS) {
    RequestedValues.insert(LHS);
    RequestedValues.insert(RHS);
    addCost(MulCost);
  }
  void countAdd(Value *LHS, Value *RHS) {
    RequestedValues.insert(LHS);
    RequestedValues.insert(RHS);
    addCost();
  }
  void visitBinaryOperator(BinaryOperator &I) {
    switch (I.getOpcode()) {
    case Instruction::Add:
      countAdd(I.getOperand(0), I.getOperand(1));
      break;
    case Instruction::Mul:
      countMul(I.getOperand(0), I.getOperand(1));
      break;
    case Instruction::UDiv:
    case Instruction::SDiv:
    case Instruction::URem:
    case Instruction::SRem:
      addOperands(I, DivCost);
      break;
    case Instruction::FRem:
      addOperands(I, GlobalCost + JumpCost);
      break;
    case Instruction::FDiv:
      addOperands(I, FDivCost);
      break;
    case Instruction::FMul:
      addOperands(I, FMulCost);
      break;
    case Instruction::FAdd:
    case Instruction::FSub:
      addOperands(I, FCheapOpCost);
      break;
    default:
      addOperands(I, 1);
    }
  }
  void visitCastInst(CastInst &I) {
    addOperands(I, I.getSrcTy()->isFPOrFPVectorTy() ||
                           I.getDestTy()->isFPOrFPVectorTy()
                       ? FCheapOpCost
                       : 1);
  }
  void visitCmp(CmpInst::Predicate Pred, Value *LHS, Value *RHS) {
    RequestedValues.insert(LHS);
    RequestedValues.insert(RHS);
    addCost(LHS->getType()->isFPOrFPVectorTy() ? FCheapOpCost : 1);
  }
  void visitCmpInst(CmpInst &I) {
    visitCmp(I.getPredicate(), I.getOperand(0), I.getOperand(1));
  }
  void visitCallBase(CallBase &I) {
    addCost(GlobalCost + JumpCost);
    for (Value *V : I.args())
      RequestedValues.insert(V);
  }
  void visitIntrinsicInst(IntrinsicInst &I) {
    Intrinsic::ID IID = I.getIntrinsicID();
    switch (IID) {
    default: {
      addCost(UnsupportedCost);
      visitCallBase(I);
      break;
    }
    case Intrinsic::ctlz:
    case Intrinsic::cttz:
    case Intrinsic::ctpop: {
      addCost(BitCountCost);
      RequestedValues.insert(I.getArgOperand(0));
      break;
    }
    case Intrinsic::abs: {
      addCost();
      RequestedValues.insert(I.getArgOperand(0));
      break;
    }
    case Intrinsic::smax:
    case Intrinsic::smin:
    case Intrinsic::umax:
    case Intrinsic::umin: {
      addCost();
      RequestedValues.insert(I.getArgOperand(0));
      RequestedValues.insert(I.getArgOperand(1));
      break;
    }
    case Intrinsic::fabs:
    case Intrinsic::is_fpclass:
    case Intrinsic::copysign:
    case Intrinsic::minnum:
    case Intrinsic::maxnum:
    case Intrinsic::minimum:
    case Intrinsic::maximum: {
      addCost(FCheapOpCost);
      RequestedValues.insert(I.getArgOperand(0));
      break;
    }
    case Intrinsic::sqrt: {
      addCost(FDivCost);
      RequestedValues.insert(I.getArgOperand(0));
      break;
    }
    case Intrinsic::assume:
      break;
    }
  }
  void visitSelectInst(SelectInst &I) { addOperands(I, 2 + JumpCost); }
  void visitFreezeInst(FreezeInst &I) {
    RequestedValues.insert(I.getOperand(0));
  }
  void visitReturnInst(ReturnInst &I) { addCost(JumpCost); }
  void visitLoadInst(LoadInst &I) { addOperands(I, LoadStoreCost); }
  void visitStoreInst(StoreInst &I) { addOperands(I, LoadStoreCost); }
  void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) {
    addOperands(I, LoadStoreCost);
  }
  void visitAtomicRMWInst(AtomicRMWInst &I) { addOperands(I, LoadStoreCost); }
  void visitFenceInst(FenceInst &I) {}
  void visitUnreachableInst(UnreachableInst &I) {}
  void visitBranchInst(BranchInst &I) { addOperands(I, JumpCost); }
  void visitSwitchInst(SwitchInst &I) {
    // Expand to icmp + br
    addOperands(I, JumpCost * (I.getNumCases() - I.defaultDestUndefined()));
    for (auto &Case : I.cases())
      visitCmp(ICmpInst::ICMP_EQ, I.getCondition(), Case.getCaseValue());
  }
  void visitPHINode(PHINode &PHI) { addCost(PHI.getNumIncomingValues()); }
  void visitIndirectBrInst(IndirectBrInst &I) { addOperands(I, JumpCost); }
  void visitExtractValueInst(ExtractValueInst &I) {
    addOperands(I, UnsupportedCost);
  }
  void visitInsertValueInst(InsertValueInst &I) {
    addOperands(I, UnsupportedCost);
  }
  void visitExtractElementInst(ExtractElementInst &I) {
    addOperands(I, UnsupportedCost);
  }
  void visitInsertElementInst(InsertElementInst &I) {
    addOperands(I, UnsupportedCost);
  }
  void visitAllocaInst(AllocaInst &I) { addOperands(I, 0); }
  void visitGetElementPtrInst(GetElementPtrInst &I) {
    auto &DL = Mod.getDataLayout();
    MapVector<Value *, APInt> VariableOffsets;
    APInt ConstantOffset = APInt::getZero(DL.getPointerSizeInBits());
    I.collectOffset(DL, 64, VariableOffsets, ConstantOffset);

    for (auto &[V, Scale] : VariableOffsets) {
      if (Scale != 1) {
        countMul(V, ConstantInt::get(I.getContext(), Scale));
      } else
        RequestedValues.insert(V);
      addCost();
    }
    countAdd(I.getPointerOperand(),
             ConstantInt::get(I.getContext(), ConstantOffset));
  }
  void visitShuffleVectorInst(ShuffleVectorInst &I) {
    addOperands(I, UnsupportedCost);
  }
  void visitTerminatorInst(Instruction &I) { addOperands(I, UnsupportedCost); }
  void visitResumeInst(ResumeInst &I) { addOperands(I, UnsupportedCost); }
  void visitLandingPadInst(LandingPadInst &I) {
    addOperands(I, UnsupportedCost);
  }
  void visitVAArgInst(VAArgInst &I) { addOperands(I, UnsupportedCost); }
  void visitCatchPadInst(CatchPadInst &I) { addOperands(I, UnsupportedCost); }
  void visitCleanupPadInst(CleanupPadInst &I) {
    addOperands(I, UnsupportedCost);
  }
  void visitFuncletPadInst(FuncletPadInst &I) {
    addOperands(I, UnsupportedCost);
  }

  void visitInstruction(Instruction &I) {
    errs() << I << '\n';
    llvm_unreachable("Unhandled instruction type");
  }

  uint64_t run(Function &F) {
    AssumptionCache AC(F);
    DominatorTree DT(F);
    DomConditionCache DC;
    TargetLibraryInfoImpl TLI(Triple(Mod.getTargetTriple()));
    TargetLibraryInfo TLIWrapper(TLI);

    SQ.AC = &AC;
    SQ.DT = &DT;
    SQ.TLI = &TLIWrapper;
    SQ.DC = &DC;

    for (auto &BB : F) {
      if (!DT.isReachableFromEntry(&BB))
        continue;
      if (auto *BI = dyn_cast<BranchInst>(BB.getTerminator());
          BI && BI->isConditional())
        DC.registerBranch(BI);
      for (auto &PHI : BB.phis())
        for (auto &V : PHI.incoming_values())
          RequestedValues.insert(V);
    }

    for (auto BB : post_order(&F)) {
      if (!DT.isReachableFromEntry(BB))
        continue;
      for (auto &I : reverse(*BB)) {
        if (RequestedValues.contains(&I) ||
            !wouldInstructionBeTriviallyDead(&I))
          visit(I);
      }
    }

    for (auto V : RequestedValues) {
      if (isa<ConstantExpr>(V))
        addCost(GlobalCost);
      if (isa<ConstantInt>(V)) {
        addCost(LoadStoreCost);
      }
      if (isa<ConstantFP>(V)) {
        addCost(LoadStoreCost);
      }
    }
    return Cost;
  }
};

static uint64_t estimateCost(Module &M) {
  uint64_t Cost = 0;

  for (auto &F : M) {
    if (F.empty())
      continue;
    CostEstimator Estimator{M};
    Cost += Estimator.run(F);
  }
  return Cost;
}

int main(int argc, char **argv) {
  InitLLVM Init{argc, argv};
  cl::ParseCommandLineOptions(argc, argv, "scanner\n");

  std::vector<fs::path> InputFiles;
  for (auto &Entry : fs::recursive_directory_iterator(std::string(InputDir))) {
    if (Entry.is_regular_file()) {
      auto &Path = Entry.path();
      if (Path.extension() == ".ll" &&
          Path.string().find("/optimized/") != std::string::npos)
        InputFiles.push_back(Path);
    }
  }
  errs() << "Input files: " << InputFiles.size() << '\n';
  LLVMContext Context;
  std::map<std::string, uint64_t> CostTable;
  uint32_t Count = 0;

  using namespace PatternMatch;
  auto Base = fs::absolute(std::string(InputDir));
  std::string_view Pattern = "/optimized/";

  for (auto &Path : InputFiles) {
    SMDiagnostic Err;
    auto M = parseIRFile(Path.string(), Err, Context);
    if (!M)
      continue;
    auto Name = fs::relative(Path, Base).string();
    Name.replace(Name.find(Pattern), Pattern.size(), "/");
    CostTable[Name] = estimateCost(*M);

    errs() << "\rProgress: " << ++Count;
  }
  errs() << '\n';

  std::ofstream ResultFile("cost.txt");
  if (!ResultFile.is_open())
    return EXIT_FAILURE;

  uint64_t Sum = 0;
  for (auto &[K, V] : CostTable) {
    ResultFile << K << ' ' << V << '\n';
    Sum += V;
  }
  ResultFile << "Total " << Sum << '\n';

  return EXIT_SUCCESS;
}
