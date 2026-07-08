//===--- CodeGenPassBuilder.cpp --------------------------------------- ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines interfaces to access the target independent code
// generation passes provided by the LLVM backend.
//
//===---------------------------------------------------------------------===//

#include "core/passes/CodeGenPassBuilder.h"

using namespace llvm;

namespace llvm {
#define DUMMY_MACHINE_FUNCTION_ANALYSIS(NAME, CREATE_PASS)                     \
  AnalysisKey PASS_NAME::Key;
#include "core/passes/MachinePassRegistry.def"
} // namespace llvm
