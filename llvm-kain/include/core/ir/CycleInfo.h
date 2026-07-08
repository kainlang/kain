//===- CycleInfo.h - Cycle Info for LLVM IR -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file declares the LLVM IR specialization of the GenericCycle
/// templates.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_CYCLEINFO_H
#define LLVM_IR_CYCLEINFO_H

#include "support/adt/GenericCycleInfo.h"
#include "core/ir/SSAContext.h"

namespace llvm {

// Use class instead of using to allow forward declarations.
class CycleInfo : public GenericCycleInfo<SSAContext> {};

using Cycle = CycleInfo::CycleT;

} // namespace llvm

#endif // LLVM_IR_CYCLEINFO_H
