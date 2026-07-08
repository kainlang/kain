//===- CountVisits.cpp ----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/passes/Utils/CountVisits.h"
#include "support/adt/Statistic.h"
#include "core/ir/Function.h"
#include "core/ir/PassManager.h"

using namespace llvm;

#define DEBUG_TYPE "count-visits"

STATISTIC(MaxVisited, "Max number of times we visited a function");

PreservedAnalyses CountVisitsPass::run(Function &F, FunctionAnalysisManager &) {
  uint32_t Count = Counts[F.getName()] + 1;
  Counts[F.getName()] = Count;
  if (Count > MaxVisited)
    MaxVisited = Count;
  return PreservedAnalyses::all();
}
