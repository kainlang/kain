//===- LoopAccessAnalysisPrinter.cpp - Loop Access Analysis Printer --------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/passes/Scalar/LoopAccessAnalysisPrinter.h"
#include "support/adt/PriorityWorklist.h"
#include "core/analysis/LoopAccessAnalysis.h"
#include "core/analysis/LoopInfo.h"
#include "core/passes/Utils/LoopUtils.h"

using namespace llvm;

#define DEBUG_TYPE "loop-accesses"

PreservedAnalyses LoopAccessInfoPrinterPass::run(Function &F,
                                                 FunctionAnalysisManager &AM) {
  auto &LAIs = AM.getResult<LoopAccessAnalysis>(F);
  auto &LI = AM.getResult<LoopAnalysis>(F);
  OS << "Printing analysis 'Loop Access Analysis' for function '" << F.getName()
     << "':\n";

  SmallPriorityWorklist<Loop *, 4> Worklist;
  appendLoopsToWorklist(LI, Worklist);
  while (!Worklist.empty()) {
    Loop *L = Worklist.pop_back_val();
    OS.indent(2) << L->getHeader()->getName() << ":\n";
    LAIs.getInfo(*L, AllowPartial).print(OS, 4);
  }
  return PreservedAnalyses::all();
}
