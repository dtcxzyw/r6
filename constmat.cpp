// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2024 Yingwei Zheng
// This file is licensed under the Apache-2.0 License.
// See the LICENSE file for more information.

#include <llvm/Support/MathExtras.h>
#include <fstream>
#include <iostream>

using namespace llvm;

static uint32_t getMatCost(int64_t V) {
  if (V == 0 || V == 1)
    return 0;

  if (isInt<12>(V))
    return 1;

  // uint32_t Idx, Len;
  // if (isShiftedMask_64(V, Idx, Len) && Len <= 6)
  //   return 1;

  if (isInt<32>(V))
    return 2;

  // Load from constant pool
  return 4;
}

int main() {
  std::ifstream File("constdist.txt");
  if (!File.is_open())
    return EXIT_FAILURE;

  int64_t k;
  uint32_t v;
  uint64_t sum = 0;
  while (File >> k >> v) {
    sum += getMatCost(k) * v;
  }

  std::cout << "Cost: " << sum << std::endl;

  return EXIT_SUCCESS;
}
