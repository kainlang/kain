//===- CycleInfo.cpp - IR Cycle Info ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/ir/CycleInfo.h"
#include "support/adt/GenericCycleImpl.h"
#include "core/ir/CFG.h"

using namespace llvm;

template class llvm::GenericCycleInfo<SSAContext>;
template class llvm::GenericCycle<SSAContext>;
