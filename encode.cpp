// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2024 Yingwei Zheng
// This file is licensed under the Apache-2.0 License.
// See the LICENSE file for more information.

#include "immbits.hpp"
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <string_view>
#include <vector>
#include <z3++.h>

constexpr uint32_t InstructionBits = 32;
constexpr uint32_t RegBits = 5;
constexpr uint32_t BinOpReg = RegBits * 3;
constexpr uint32_t UnOpReg = RegBits * 2;
constexpr uint32_t OpTypeBits = 2; // 8 16 32 64

struct Op {
  std::string_view Mnemonic;
  uint32_t Length;
};

constexpr Op Ops[] = {
    {"LI", RegBits + LargeImmBits},                                     //
    {"LUI", RegBits + LargeImmBits},                                    //
    {"LBITI", RegBits + BitImmBits},                                    //
    {"ADD", BinOpReg + OpTypeBits},                                     //
    {"SUB", BinOpReg + OpTypeBits},                                     //
    {"ADDI", UnOpReg + OpTypeBits + AddSubImmBits},                     //
    {"RSBI", UnOpReg + OpTypeBits + AddSubImmBits},                     //
    {"SLL", BinOpReg + OpTypeBits},                                     //
    {"SRL", BinOpReg + OpTypeBits},                                     //
    {"SRA", BinOpReg + OpTypeBits},                                     //
    {"SLLVI", UnOpReg + ShAmtBits + OpTypeBits},                        //
    {"SRLVI", UnOpReg + ShAmtBits + OpTypeBits},                        //
    {"SRAVI", UnOpReg + ShAmtBits + OpTypeBits},                        //
    {"SLLIV", UnOpReg + ShiftImmBits + OpTypeBits},                     //
    {"SRLIV", UnOpReg + ShiftImmBits + OpTypeBits},                     //
    {"SRAIV", UnOpReg + ShiftImmBits + OpTypeBits},                     //
    {"FSHL", RegBits * 4 + OpTypeBits},                                 //
    {"FSHR", RegBits * 4 + OpTypeBits},                                 //
    {"FSHLI", BinOpReg + ShAmtBits + OpTypeBits},                       //
    {"AND", BinOpReg + NotBit},                                         //
    {"OR", BinOpReg + NotBit},                                          //
    {"XOR", BinOpReg + NotBit},                                         //
    {"ANDI", UnOpReg + NotBit + BitImmBits},                            //
    {"ORI", UnOpReg + NotBit + BitImmBits},                             //
    {"XORI", UnOpReg + NotBit + BitImmBits},                            //
    {"ICMP", BinOpReg + OpTypeBits + 4},                                //
    {"ICMPI", UnOpReg + OpTypeBits + 4 + CmpImmBits},                   //
    {"CTPOP", UnOpReg + OpTypeBits},                                    //
    {"CTLZ", UnOpReg + OpTypeBits},                                     //
    {"CTTZ", UnOpReg + OpTypeBits},                                     //
    {"SELVV", BinOpReg},                                                //
    {"SELVI", UnOpReg + OpTypeBits + SelectImmBits},                    //
    {"SELIV", UnOpReg + OpTypeBits + SelectImmBits},                    //
    {"SELII", RegBits + OpTypeBits + SmallSelectImmBits * 2},           //
    {"SCMPSELI", BinOpReg + OpTypeBits},                                //
    {"UCMPSELI", BinOpReg + OpTypeBits},                                //
    {"MUL", BinOpReg + OpTypeBits},                                     //
    {"MULI", UnOpReg + OpTypeBits + MulDivBits},                        //
    {"MULHU", BinOpReg + OpTypeBits},                                   //
    {"MULHS", BinOpReg + OpTypeBits},                                   //
    {"SDIV", BinOpReg + OpTypeBits},                                    //
    {"SDIVI", UnOpReg + OpTypeBits + MulDivBits},                       //
    {"UDIV", BinOpReg + OpTypeBits},                                    //
    {"UDIVI", UnOpReg + OpTypeBits + MulDivBits},                       //
    {"SREM", BinOpReg + OpTypeBits},                                    //
    {"SREMI", UnOpReg + OpTypeBits + MulDivBits},                       //
    {"UREM", BinOpReg + OpTypeBits},                                    //
    {"UREMI", UnOpReg + OpTypeBits + MulDivBits},                       //
    {"ABS", UnOpReg + OpTypeBits},                                      //
    {"ABSDIFF", BinOpReg + OpTypeBits},                                 //
    {"BSWAP16", UnOpReg},                                               //
    {"BSWAP32", UnOpReg},                                               //
    {"BSWAP64", UnOpReg},                                               //
    {"BREV", UnOpReg + OpTypeBits},                                     //
    {"SMAX", BinOpReg + OpTypeBits},                                    //
    {"SMIN", BinOpReg + OpTypeBits},                                    //
    {"UMAX", BinOpReg + OpTypeBits},                                    //
    {"UMIN", BinOpReg + OpTypeBits},                                    //
    {"SMAXI", UnOpReg + OpTypeBits + MinMaxImmBits},                    //
    {"SMINI", UnOpReg + OpTypeBits + MinMaxImmBits},                    //
    {"UMAXI", UnOpReg + OpTypeBits + MinMaxImmBits},                    //
    {"UMINI", UnOpReg + OpTypeBits + MinMaxImmBits},                    //
    {"SSAT", UnOpReg + OpTypeBits + ShAmtBits},                         //
    {"USAT", UnOpReg + OpTypeBits + ShAmtBits},                         //
    {"FADD", BinOpReg + OpTypeBits},                                    //
    {"FADDI", UnOpReg + OpTypeBits + FPSmallImmBits},                   //
    {"FSUB", BinOpReg + OpTypeBits},                                    //
    {"FRSBI", UnOpReg + OpTypeBits + FPSmallImmBits},                   //
    {"FMUL", BinOpReg + OpTypeBits},                                    //
    {"FMULI", UnOpReg + OpTypeBits + FPSmallImmBits},                   //
    {"FDIV", BinOpReg + OpTypeBits},                                    //
    {"FDIVI", UnOpReg + OpTypeBits + FPSmallImmBits},                   //
    {"FSQRT", UnOpReg + OpTypeBits},                                    //
    {"FABS", UnOpReg + OpTypeBits + NegBit},                            //
    {"FCOPYSIGN", BinOpReg + OpTypeBits + NegBit},                      //
    {"FCOPYSIGNI", UnOpReg + OpTypeBits + NegBit + FPSmallImmBits - 1}, //
    {"FMAX", BinOpReg + OpTypeBits},                                    //
    {"FMIN", BinOpReg + OpTypeBits},                                    //
    {"FMAXNM", BinOpReg + OpTypeBits},                                  //
    {"FMINNM", BinOpReg + OpTypeBits},                                  //
    {"FCLASS", UnOpReg + 10 + OpTypeBits},                              //
    {"FTOSI", UnOpReg + OpTypeBits},                                    //
    {"FTOUI", UnOpReg + OpTypeBits},                                    //
    {"FTOSISAT", UnOpReg + OpTypeBits + ShAmtBits},                     //
    {"FTOUISAT", UnOpReg + OpTypeBits + ShAmtBits},                     //
    {"FTOBI", UnOpReg + OpTypeBits},                                    //
    {"SITOF", UnOpReg + OpTypeBits},                                    //
    {"UITOF", UnOpReg + OpTypeBits},                                    //
    {"BITOF", UnOpReg + OpTypeBits},                                    //
    {"FMA", RegBits * 4 + OpTypeBits},                                  //
    {"FLI", RegBits + OpTypeBits + FPImmBits},                          //
    {"FCMP", BinOpReg + OpTypeBits + 4},                                //
    {"FCMPI", UnOpReg + OpTypeBits + 4 + FPSmallImmBits},               //
    {"J", LinkBit + JumpOffsetImmBits},                                 //
    {"JR", RegBits + LinkBit + JumpOffsetImmBits},                      //
    {"BCMP", RegBits * 2 + OpTypeBits + 4 + BranchOffsetImmBits},       //
    {"BCMPI",
     RegBits + BranchCmpImmBits + OpTypeBits + 4 + BranchOffsetImmBits}, //
    {"SHLIADD", BinOpReg + OpTypeBits + ShAmtBits},                      //
    {"MULIADD", BinOpReg + OpTypeBits + SmallMulBits},                   //
    {"SRLIDIFF", BinOpReg + OpTypeBits + ShAmtBits},                     //
    {"SRAIDIFF", BinOpReg + OpTypeBits + ShAmtBits},                     //
    {"UDIVIDIFF", BinOpReg + OpTypeBits + SmallMulBits},                 //
    {"SDIVIDIFF", BinOpReg + OpTypeBits + SmallMulBits},                 //
};

constexpr bool isUnique() {
  uint32_t Size = std::size(Ops);
  for (uint32_t i = 0; i < Size; ++i) {
    for (uint32_t j = i + 1; j < Size; ++j) {
      if (Ops[i].Mnemonic == Ops[j].Mnemonic)
        return false;
    }
  }

  return true;
}
static_assert(isUnique(), "Redefined operation mnemonic");
constexpr bool isDecodable() {
  for (auto &Op : Ops) {
    if (Op.Length >= InstructionBits)
      return false;
  }
  return true;
}
static_assert(isDecodable(), "Invalid instruction length");

int main() {
  z3::context Ctx;
  z3::solver Solver(Ctx);

  constexpr uint32_t OpSize = std::size(Ops);
  std::vector<z3::expr> Vars;
  for (auto &[Mnemonic, Length] : Ops) {
    uint32_t PrefixLength = InstructionBits - Length;
    z3::expr Var = Ctx.bv_const(Mnemonic.data(), PrefixLength);
    Vars.push_back(Var);
  }

  // Prefix constraints

  auto Trunc = [&](z3::expr &Var, uint32_t Size) {
    if (Var.get_sort().bv_size() != Size)
      Var = Var.extract(Var.get_sort().bv_size() - 1,
                        Var.get_sort().bv_size() - Size);
  };

  for (uint32_t i = 0; i < OpSize; ++i) {
    for (uint32_t j = i + 1; j < OpSize; ++j) {
      auto LHS = Vars[i];
      auto RHS = Vars[j];
      auto Size = std::min(LHS.get_sort().bv_size(), RHS.get_sort().bv_size());
      Trunc(LHS, Size);
      Trunc(RHS, Size);
      Solver.add(LHS != RHS);
    }
  }

  // TODO: add distance constraints
  auto Res = Solver.check();
  if (Res == z3::sat) {
    auto Model = Solver.get_model();
    for (uint32_t i = 0; i < OpSize; ++i) {
      std::string bin;
      Model.eval(Vars[i]).as_binary(bin);
      std::cout << std::setw(12) << std::setfill(' ') << Ops[i].Mnemonic << ' '
                << std::setw(InstructionBits - Ops[i].Length)
                << std::setfill('0') << bin << '\n';
    }
  } else {
    std::cout << "unsat\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
