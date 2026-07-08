//===- DominanceFrontier.cpp - Dominance Frontier Calculation -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/analysis/DominanceFrontier.h"
#include "core/analysis/DominanceFrontierImpl.h"
#include "core/config/llvm-config.h"
#include "core/ir/Dominators.h"
#include "core/ir/Function.h"
#include "core/ir/PassManager.h"
#include "core/InitializePasses.h"
#include "core/Pass.h"
#include "support/adt/Compiler.h"
#include "support/adt/raw_ostream.h"

using namespace llvm;

namespace llvm {

template class LLVM_TEMPLATE_ABI DominanceFrontierBase<BasicBlock, false>;
template class LLVM_TEMPLATE_ABI DominanceFrontierBase<BasicBlock, true>;

} // end namespace llvm

char DominanceFrontierWrapperPass::ID = 0;

INITIALIZE_PASS_BEGIN(DominanceFrontierWrapperPass, "domfrontier",
                "Dominance Frontier Construction", true, true)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(DominanceFrontierWrapperPass, "domfrontier",
                "Dominance Frontier Construction", true, true)

DominanceFrontierWrapperPass::DominanceFrontierWrapperPass()
    : FunctionPass(ID) {}

void DominanceFrontierWrapperPass::releaseMemory() {
  DF.releaseMemory();
}

bool DominanceFrontierWrapperPass::runOnFunction(Function &) {
  releaseMemory();
  DF.analyze(getAnalysis<DominatorTreeWrapperPass>().getDomTree());
  return false;
}

void DominanceFrontierWrapperPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<DominatorTreeWrapperPass>();
}

void DominanceFrontierWrapperPass::print(raw_ostream &OS, const Module *) const {
  DF.print(OS);
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void DominanceFrontierWrapperPass::dump() const {
  print(dbgs());
}
#endif

/// Handle invalidation explicitly.
bool DominanceFrontier::invalidate(Function &F, const PreservedAnalyses &PA,
                                   FunctionAnalysisManager::Invalidator &) {
  // Check whether the analysis, all analyses on functions, or the function's
  // CFG have been preserved.
  auto PAC = PA.getChecker<DominanceFrontierAnalysis>();
  return !(PAC.preserved() || PAC.preservedSet<AllAnalysesOn<Function>>() ||
           PAC.preservedSet<CFGAnalyses>());
}

AnalysisKey DominanceFrontierAnalysis::Key;

DominanceFrontier DominanceFrontierAnalysis::run(Function &F,
                                                 FunctionAnalysisManager &AM) {
  DominanceFrontier DF;
  DF.analyze(AM.getResult<DominatorTreeAnalysis>(F));
  return DF;
}

DominanceFrontierPrinterPass::DominanceFrontierPrinterPass(raw_ostream &OS)
  : OS(OS) {}

PreservedAnalyses
DominanceFrontierPrinterPass::run(Function &F, FunctionAnalysisManager &AM) {
  OS << "DominanceFrontier for function: " << F.getName() << "\n";
  AM.getResult<DominanceFrontierAnalysis>(F).print(OS);

  return PreservedAnalyses::all();
}
