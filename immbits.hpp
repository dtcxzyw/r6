// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2024 Yingwei Zheng
// This file is licensed under the Apache-2.0 License.
// See the LICENSE file for more information.

#pragma once
#include <cstdint>

constexpr uint32_t ShAmtBits = 6;
constexpr uint32_t AddSubImmBits = 12;
constexpr uint32_t BitImmBits = 15;
// 0: Int<8> << ShAmt
// 100: splat Int<8>
// 110: mask 2^k - 1
// 111: ~(mask 2^k - 1)
constexpr uint32_t SelectImmBits = 12;
constexpr uint32_t SmallSelectImmBits = 8;
constexpr uint32_t LargeImmBits = 20;
constexpr uint32_t ShiftImmBits = 12;
constexpr uint32_t MinMaxImmBits = 12;
constexpr uint32_t MulDivBits = 10;
constexpr uint32_t FPImmBits = 16;     // S_IEEEHalf
constexpr uint32_t FPSmallImmBits = 8; // S_Float8E4M3FN
constexpr uint32_t NotBit = 1;
constexpr uint32_t NegBit = 1;
constexpr uint32_t LinkBit = 1;
constexpr uint32_t CmpImmBits = 12;
constexpr uint32_t JumpOffsetImmBits = 16;
constexpr uint32_t BranchOffsetImmBits = 10;
constexpr uint32_t BranchCmpImmBits = 6;
