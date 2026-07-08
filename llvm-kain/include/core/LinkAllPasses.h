//===- llvm/LinkAllPasses.h ------------ Reference All Passes ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This header file pulls in all transformation and analysis passes for tools
// like opt and bugpoint that need this functionality.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LINKALLPASSES_H
#define LLVM_LINKALLPASSES_H

#include "support/adt/Statistic.h"
#include "core/analysis/AliasAnalysisEvaluator.h"
#include "core/analysis/AliasSetTracker.h"
#include "core/analysis/BasicAliasAnalysis.h"
#include "core/analysis/CallPrinter.h"
#include "core/analysis/DXILResource.h"
#include "core/analysis/DomPrinter.h"
#include "core/analysis/GlobalsModRef.h"
#include "core/analysis/Passes.h"
#include "core/analysis/PostDominators.h"
#include "core/analysis/RegionPass.h"
#include "core/analysis/RegionPrinter.h"
#include "core/analysis/ScalarEvolution.h"
#include "core/analysis/ScalarEvolutionAliasAnalysis.h"
#include "core/analysis/ScopedNoAliasAA.h"
#include "core/analysis/TargetLibraryInfo.h"
#include "core/analysis/TypeBasedAliasAnalysis.h"
#include "target/shared/codegen/Passes.h"
#include "core/ir/Function.h"
#include "core/ir/IRPrintingPasses.h"
#include "support/adt/AlwaysTrue.h"
#include "support/adt/Valgrind.h"
#include "core/passes/IPO.h"
#include "core/passes/IPO/AlwaysInliner.h"
#include "core/passes/IPO/GlobalDCE.h"
#include "core/passes/InstCombine/InstCombine.h"
#include "core/passes/ObjCARC.h"
#include "core/passes/Scalar.h"
#include "core/passes/Scalar/GVN.h"
#include "core/passes/Scalar/Scalarizer.h"
#include "core/passes/Utils.h"
#include "core/passes/Utils/SymbolRewriter.h"
#include "core/passes/Vectorize/LoadStoreVectorizer.h"
#include <cstdlib>

namespace llvm {
class Triple;
}

namespace {
struct ForcePassLinking {
  ForcePassLinking() {
    // We must reference the passes in such a way that compilers will not delete
    // it all as dead code, even with whole program optimization, yet is
    // effectively a NO-OP. This is so that globals in the translation units
    // where these functions are defined are forced to be initialized,
    // populating various registries.
    if (llvm::getNonFoldableAlwaysTrue())
      return;

    (void)llvm::createAtomicExpandLegacyPass();
    (void)llvm::createBasicAAWrapperPass();
    (void)llvm::createSCEVAAWrapperPass();
    (void)llvm::createTypeBasedAAWrapperPass();
    (void)llvm::createScopedNoAliasAAWrapperPass();
    (void)llvm::createBreakCriticalEdgesPass();
    (void)llvm::createCallGraphDOTPrinterPass();
    (void)llvm::createCallGraphViewerPass();
    (void)llvm::createCFGSimplificationPass();
    (void)llvm::createStructurizeCFGPass();
    (void)llvm::createDXILResourceWrapperPassPass();
    (void)llvm::createDXILResourceTypeWrapperPassPass();
    (void)llvm::createDeadArgEliminationPass();
    (void)llvm::createDeadCodeEliminationPass();
    (void)llvm::createDeadStoreEliminationPass();
    (void)llvm::createDependenceAnalysisWrapperPass();
    (void)llvm::createDomOnlyPrinterWrapperPassPass();
    (void)llvm::createDomPrinterWrapperPassPass();
    (void)llvm::createDomOnlyViewerWrapperPassPass();
    (void)llvm::createDomViewerWrapperPassPass();
    (void)llvm::createAlwaysInlinerLegacyPass();
    (void)llvm::createGlobalDCEPass();
    (void)llvm::createGlobalMergeFuncPass();
    (void)llvm::createGlobalsAAWrapperPass();
    (void)llvm::createInstSimplifyLegacyPass();
    (void)llvm::createInstructionCombiningPass();
    (void)llvm::createJMCInstrumenterPass();
    (void)llvm::createKCFIPass();
    (void)llvm::createLCSSAPass();
    (void)llvm::createLICMPass();
    (void)llvm::createLazyValueInfoPass();
    (void)llvm::createLoopExtractorPass();
    (void)llvm::createLoopSimplifyPass();
    (void)llvm::createLoopStrengthReducePass();
    (void)llvm::createLoopTermFoldPass();
    (void)llvm::createLoopUnrollPass();
    (void)llvm::createLowerGlobalDtorsLegacyPass();
    (void)llvm::createLowerInvokePass();
    (void)llvm::createLowerSwitchPass();
    (void)llvm::createNaryReassociatePass();
    (void)llvm::createObjCARCContractPass();
    (void)llvm::createPromoteMemoryToRegisterPass();
    (void)llvm::createRegToMemWrapperPass();
    (void)llvm::createPostDomOnlyPrinterWrapperPassPass();
    (void)llvm::createPostDomPrinterWrapperPassPass();
    (void)llvm::createPostDomOnlyViewerWrapperPassPass();
    (void)llvm::createPostDomViewerWrapperPassPass();
    (void)llvm::createReassociatePass();
    (void)llvm::createRegionInfoPass();
    (void)llvm::createRegionOnlyPrinterPass();
    (void)llvm::createRegionOnlyViewerPass();
    (void)llvm::createRegionPrinterPass();
    (void)llvm::createRegionViewerPass();
    (void)llvm::createSafeStackPass();
    (void)llvm::createSROAPass();
    (void)llvm::createSingleLoopExtractorPass();
    (void)llvm::createTailCallEliminationPass();
    (void)llvm::createConstantHoistingPass();
    (void)llvm::createCodeGenPrepareLegacyPass();
    (void)llvm::createPostInlineEntryExitInstrumenterPass();
    (void)llvm::createEarlyCSEPass();
    (void)llvm::createGVNPass();
    (void)llvm::createPostDomTree();
    std::string buf;
    llvm::raw_string_ostream os(buf);
    (void)llvm::createPrintModulePass(os);
    (void)llvm::createPrintFunctionPass(os);
    (void)llvm::createSinkingPass();
    (void)llvm::createLowerAtomicPass();
    (void)llvm::createLoadStoreVectorizerPass();
    (void)llvm::createPartiallyInlineLibCallsPass();
    (void)llvm::createScalarizerPass();
    (void)llvm::createSeparateConstOffsetFromGEPPass();
    (void)llvm::createSpeculativeExecutionPass();
    (void)llvm::createSpeculativeExecutionIfHasBranchDivergencePass();
    (void)llvm::createStraightLineStrengthReducePass();
    (void)llvm::createScalarizeMaskedMemIntrinLegacyPass();
    (void)llvm::createHardwareLoopsLegacyPass();
    (void)llvm::createUnifyLoopExitsPass();
    (void)llvm::createFixIrreduciblePass();
    (void)llvm::createSelectOptimizePass();

    (void)new llvm::ScalarEvolutionWrapperPass();
    llvm::Function::Create(nullptr, llvm::GlobalValue::ExternalLinkage)
        ->viewCFGOnly();
    llvm::RGPassManager RGM;
    llvm::TargetLibraryInfoImpl TLII((llvm::Triple()));
    llvm::TargetLibraryInfo TLI(TLII);
    llvm::AliasAnalysis AA(TLI);
    llvm::BatchAAResults BAA(AA);
    llvm::AliasSetTracker X(BAA);
    (void)llvm::AreStatisticsEnabled();
    (void)llvm::sys::RunningOnValgrind();
  }
} ForcePassLinking; // Force link by creating a global definition.
} // namespace

#endif
