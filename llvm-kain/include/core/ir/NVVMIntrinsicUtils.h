//===--- NVVMIntrinsicUtils.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Stubbed for llvm-kain stripped build (NVVM target dropped).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_NVVM_INTRINSIC_UTILS_H
#define LLVM_IR_NVVM_INTRINSIC_UTILS_H

#include "core/ir/Intrinsics.h"
#include "support/adt/FloatingPointMode.h"

namespace llvm {
namespace nvvm {

inline RoundingMode GetFPToIntegerRoundingMode(Intrinsic::ID) {
  return RoundingMode::NearestTiesToEven;
}

inline RoundingMode GetFPMinMaxRoundingMode(Intrinsic::ID) {
  return RoundingMode::NearestTiesToEven;
}

} // namespace nvvm
} // namespace llvm

#endif
