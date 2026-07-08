//===- Parsing and selection of pass pipelines ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file provides the implementation of the PassBuilder based on our
/// static pass registry as well as related functionality. It also provides
/// helpers to aid in analyzing, debugging, and testing passes and pass
/// pipelines.
///
//===----------------------------------------------------------------------===//

#include "core/passes/PassBuilder.h"
#include "support/adt/StringSwitch.h"
#include "support/adt/StringTable.h"
#include "core/analysis/AliasAnalysisEvaluator.h"
#include "core/analysis/AliasSetTracker.h"
#include "core/analysis/AssumptionCache.h"
#include "core/analysis/BasicAliasAnalysis.h"
#include "core/analysis/BlockFrequencyInfo.h"
#include "core/analysis/BranchProbabilityInfo.h"
#include "core/analysis/CFGSCCPrinter.h"
#include "core/analysis/CGSCCPassManager.h"
#include "core/analysis/CallGraph.h"
#include "core/analysis/CallPrinter.h"
#include "core/analysis/CostModel.h"
#include "core/analysis/CtxProfAnalysis.h"
#include "core/analysis/CycleAnalysis.h"
#include "core/analysis/DDG.h"
#include "core/analysis/DDGPrinter.h"
#include "core/analysis/DXILMetadataAnalysis.h"
#include "core/analysis/DXILResource.h"
#include "core/analysis/Delinearization.h"
#include "core/analysis/DemandedBits.h"
#include "core/analysis/DependenceAnalysis.h"
#include "core/analysis/DomPrinter.h"
#include "core/analysis/DominanceFrontier.h"
#include "core/analysis/EphemeralValuesCache.h"
#include "core/analysis/FunctionPropertiesAnalysis.h"
#include "core/analysis/GlobalsModRef.h"
#include "core/analysis/HashRecognize.h"
#include "core/analysis/IR2Vec.h"
#include "core/analysis/IVUsers.h"
#include "core/analysis/InlineAdvisor.h"
#include "core/analysis/InstCount.h"
#include "core/analysis/KernelInfo.h"
#include "core/analysis/LastRunTrackingAnalysis.h"
#include "core/analysis/LazyCallGraph.h"
#include "core/analysis/LazyValueInfo.h"
#include "core/analysis/Lint.h"
#include "core/analysis/LoopAccessAnalysis.h"
#include "core/analysis/LoopCacheAnalysis.h"
#include "core/analysis/LoopInfo.h"
#include "core/analysis/LoopNestAnalysis.h"
#include "core/analysis/MemDerefPrinter.h"
#include "core/analysis/MemoryDependenceAnalysis.h"
#include "core/analysis/MemorySSA.h"
#include "core/analysis/ModuleDebugInfoPrinter.h"
#include "core/analysis/ModuleSummaryAnalysis.h"
#include "core/analysis/MustExecute.h"
#include "core/analysis/ObjCARCAliasAnalysis.h"
#include "core/analysis/PhiValues.h"
#include "core/analysis/PostDominators.h"
#include "core/analysis/ProfileSummaryInfo.h"
#include "core/analysis/RegionInfo.h"
#include "core/analysis/RuntimeLibcallInfo.h"
#include "core/analysis/ScalarEvolution.h"
#include "core/analysis/ScalarEvolutionAliasAnalysis.h"
#include "core/analysis/ScalarEvolutionDivision.h"
#include "core/analysis/ScopedNoAliasAA.h"
#include "core/analysis/StackLifetime.h"
#include "core/analysis/StackSafetyAnalysis.h"
#include "core/analysis/StructuralHash.h"
#include "core/analysis/TargetLibraryInfo.h"
#include "core/analysis/TargetTransformInfo.h"
#include "core/analysis/TypeBasedAliasAnalysis.h"
#include "core/analysis/UniformityAnalysis.h"
#include "target/shared/codegen/AssignmentTrackingAnalysis.h"
#include "target/shared/codegen/AtomicExpand.h"
#include "target/shared/codegen/BasicBlockSectionsProfileReader.h"
#include "target/shared/codegen/BranchFoldingPass.h"
#include "target/shared/codegen/BranchRelaxation.h"
#include "target/shared/codegen/BreakFalseDeps.h"
#include "target/shared/codegen/CFIFixup.h"
#include "target/shared/codegen/CodeGenPrepare.h"
#include "target/shared/codegen/ComplexDeinterleavingPass.h"
#include "target/shared/codegen/DeadMachineInstructionElim.h"
#include "target/shared/codegen/DetectDeadLanes.h"
#include "target/shared/codegen/DwarfEHPrepare.h"
#include "target/shared/codegen/EarlyIfConversion.h"
#include "target/shared/codegen/EdgeBundles.h"
#include "target/shared/codegen/ExpandIRInsts.h"
#include "target/shared/codegen/ExpandPostRAPseudos.h"
#include "target/shared/codegen/ExpandReductions.h"
#include "target/shared/codegen/FEntryInserter.h"
#include "target/shared/codegen/FinalizeISel.h"
#include "target/shared/codegen/FixupStatepointCallerSaved.h"
#include "target/shared/codegen/GCEmptyBasicBlocks.h"
#include "target/shared/codegen/GCMetadata.h"
#include "target/shared/codegen/GlobalISel/CSEInfo.h"
#include "target/shared/codegen/GlobalISel/GISelValueTracking.h"
#include "target/shared/codegen/GlobalMerge.h"
#include "target/shared/codegen/GlobalMergeFunctions.h"
#include "target/shared/codegen/HardwareLoops.h"
#include "target/shared/codegen/IndirectBrExpand.h"
#include "target/shared/codegen/InitUndef.h"
#include "target/shared/codegen/InlineAsmPrepare.h"
#include "target/shared/codegen/InterleavedAccess.h"
#include "target/shared/codegen/InterleavedLoadCombine.h"
#include "target/shared/codegen/JMCInstrumenter.h"
#include "target/shared/codegen/KCFI.h"
#include "target/shared/codegen/LiveDebugValuesPass.h"
#include "target/shared/codegen/LiveDebugVariables.h"
#include "target/shared/codegen/LiveIntervals.h"
#include "target/shared/codegen/LiveRegMatrix.h"
#include "target/shared/codegen/LiveStacks.h"
#include "target/shared/codegen/LiveVariables.h"
#include "target/shared/codegen/LocalStackSlotAllocation.h"
#include "target/shared/codegen/LowerEmuTLS.h"
#include "target/shared/codegen/MIRPrinter.h"
#include "target/shared/codegen/MachineBlockFrequencyInfo.h"
#include "target/shared/codegen/MachineBlockHashInfo.h"
#include "target/shared/codegen/MachineBlockPlacement.h"
#include "target/shared/codegen/MachineBranchProbabilityInfo.h"
#include "target/shared/codegen/MachineCFGPrinter.h"
#include "target/shared/codegen/MachineCSE.h"
#include "target/shared/codegen/MachineCopyPropagation.h"
#include "target/shared/codegen/MachineDebugify.h"
#include "target/shared/codegen/MachineDominanceFrontier.h"
#include "target/shared/codegen/MachineDominators.h"
#include "target/shared/codegen/MachineFunctionAnalysis.h"
#include "target/shared/codegen/MachineInstrBundle.h"
#include "target/shared/codegen/MachineLICM.h"
#include "target/shared/codegen/MachineLateInstrsCleanup.h"
#include "target/shared/codegen/MachinePassManager.h"
#include "target/shared/codegen/MachinePostDominators.h"
#include "target/shared/codegen/MachineRegionInfo.h"
#include "target/shared/codegen/MachineRegisterInfo.h"
#include "target/shared/codegen/MachineScheduler.h"
#include "target/shared/codegen/MachineSink.h"
#include "target/shared/codegen/MachineStripDebug.h"
#include "target/shared/codegen/MachineTraceMetrics.h"
#include "target/shared/codegen/MachineUniformityAnalysis.h"
#include "target/shared/codegen/MachineVerifier.h"
#include "target/shared/codegen/OptimizePHIs.h"
#include "target/shared/codegen/PEI.h"
#include "target/shared/codegen/PHIElimination.h"
#include "target/shared/codegen/PatchableFunction.h"
#include "target/shared/codegen/PeepholeOptimizer.h"
#include "target/shared/codegen/PostRAHazardRecognizer.h"
#include "target/shared/codegen/PostRAMachineSink.h"
#include "target/shared/codegen/PostRASchedulerList.h"
#include "target/shared/codegen/PreISelIntrinsicLowering.h"
#include "target/shared/codegen/ProcessImplicitDefs.h"
#include "target/shared/codegen/ReachingDefAnalysis.h"
#include "target/shared/codegen/RegAllocEvictionAdvisor.h"
#include "target/shared/codegen/RegAllocFast.h"
#include "target/shared/codegen/RegAllocGreedyPass.h"
#include "target/shared/codegen/RegAllocPriorityAdvisor.h"
#include "target/shared/codegen/RegUsageInfoCollector.h"
#include "target/shared/codegen/RegUsageInfoPropagate.h"
#include "target/shared/codegen/RegisterCoalescerPass.h"
#include "target/shared/codegen/RegisterUsageInfo.h"
#include "target/shared/codegen/RemoveLoadsIntoFakeUses.h"
#include "target/shared/codegen/RemoveRedundantDebugValues.h"
#include "target/shared/codegen/RenameIndependentSubregs.h"
#include "target/shared/codegen/ReplaceWithVeclib.h"
#include "target/shared/codegen/SafeStack.h"
#include "target/shared/codegen/SanitizerBinaryMetadata.h"
#include "target/shared/codegen/SelectOptimize.h"
#include "target/shared/codegen/ShadowStackGCLowering.h"
#include "target/shared/codegen/ShrinkWrap.h"
#include "target/shared/codegen/SjLjEHPrepare.h"
#include "target/shared/codegen/SlotIndexes.h"
#include "target/shared/codegen/SpillPlacement.h"
#include "target/shared/codegen/StackColoring.h"
#include "target/shared/codegen/StackFrameLayoutAnalysisPass.h"
#include "target/shared/codegen/StackProtector.h"
#include "target/shared/codegen/StackSlotColoring.h"
#include "target/shared/codegen/TailDuplication.h"
#include "target/shared/codegen/TargetPassConfig.h"
#include "target/shared/codegen/TwoAddressInstructionPass.h"
#include "target/shared/codegen/TypePromotion.h"
#include "target/shared/codegen/UnreachableBlockElim.h"
#include "target/shared/codegen/VirtRegMap.h"
#include "target/shared/codegen/WasmEHPrepare.h"
#include "target/shared/codegen/WinEHPrepare.h"
#include "target/shared/codegen/XRayInstrumentation.h"
#include "core/ir/DebugInfo.h"
#include "core/ir/Dominators.h"
#include "core/ir/PassManager.h"
#include "core/ir/SafepointIRVerifier.h"
#include "core/ir/Verifier.h"
#include "core/ir/IRPrintingPasses.h"
#include "core/passes/OptimizationLevel.h"
#include "support/adt/CodeGen.h"
#include "support/adt/CommandLine.h"
#include "support/adt/Debug.h"
#include "support/adt/Error.h"
#include "support/adt/ErrorHandling.h"
#include "support/adt/FormatAdapters.h"
#include "support/adt/FormatVariadic.h"
#include "support/adt/Regex.h"
#include "target/shared/TargetMachine.h"
#include "core/passes/AggressiveInstCombine/AggressiveInstCombine.h"
#include "core/passes/CFGuard.h"
#include "core/passes/Coroutines/CoroAnnotationElide.h"
#include "core/passes/Coroutines/CoroCleanup.h"
#include "core/passes/Coroutines/CoroConditionalWrapper.h"
#include "core/passes/Coroutines/CoroEarly.h"
#include "core/passes/Coroutines/CoroElide.h"
#include "core/passes/Coroutines/CoroSplit.h"
#include "core/passes/HipStdPar/HipStdPar.h"
#include "core/passes/IPO/AlwaysInliner.h"
#include "core/passes/IPO/Annotation2Metadata.h"
#include "core/passes/IPO/ArgumentPromotion.h"
#include "core/passes/IPO/Attributor.h"
#include "core/passes/IPO/BlockExtractor.h"
#include "core/passes/IPO/CalledValuePropagation.h"
#include "core/passes/IPO/ConstantMerge.h"
#include "core/passes/IPO/CrossDSOCFI.h"
#include "core/passes/IPO/DeadArgumentElimination.h"
#include "core/passes/IPO/ElimAvailExtern.h"
#include "core/passes/IPO/EmbedBitcodePass.h"
#include "core/passes/IPO/ExpandVariadics.h"
#include "core/passes/IPO/FatLTOCleanup.h"
#include "core/passes/IPO/ForceFunctionAttrs.h"
#include "core/passes/IPO/FunctionAttrs.h"
#include "core/passes/IPO/FunctionImport.h"
#include "core/passes/IPO/GlobalDCE.h"
#include "core/passes/IPO/GlobalOpt.h"
#include "core/passes/IPO/GlobalSplit.h"
#include "core/passes/IPO/HotColdSplitting.h"
#include "core/passes/IPO/IROutliner.h"
#include "core/passes/IPO/InferFunctionAttrs.h"
#include "core/passes/IPO/Instrumentor.h"
#include "core/passes/IPO/Internalize.h"
#include "core/passes/IPO/LoopExtractor.h"
#include "core/passes/IPO/LowerTypeTests.h"
#include "core/passes/IPO/MemProfContextDisambiguation.h"
#include "core/passes/IPO/MergeFunctions.h"
#include "core/passes/IPO/OpenMPOpt.h"
#include "core/passes/IPO/PartialInlining.h"
#include "core/passes/IPO/SCCP.h"
#include "core/passes/IPO/SampleProfile.h"
#include "core/passes/IPO/SampleProfileProbe.h"
#include "core/passes/IPO/StripDeadPrototypes.h"
#include "core/passes/IPO/StripSymbols.h"
#include "core/passes/IPO/WholeProgramDevirt.h"
#include "core/passes/InstCombine/InstCombine.h"
#include "core/passes/Instrumentation/AddressSanitizer.h"
#include "core/passes/Instrumentation/AllocToken.h"
#include "core/passes/Instrumentation/BoundsChecking.h"
#include "core/passes/Instrumentation/CGProfile.h"
#include "core/passes/Instrumentation/ControlHeightReduction.h"
#include "core/passes/Instrumentation/DataFlowSanitizer.h"
#include "core/passes/Instrumentation/GCOVProfiler.h"
#include "core/passes/Instrumentation/HWAddressSanitizer.h"
#include "core/passes/Instrumentation/InstrProfiling.h"
#include "core/passes/Instrumentation/KCFI.h"
#include "core/passes/Instrumentation/LowerAllowCheckPass.h"
#include "core/passes/Instrumentation/MemProfInstrumentation.h"
#include "core/passes/Instrumentation/MemProfUse.h"
#include "core/passes/Instrumentation/MemorySanitizer.h"
#include "core/passes/Instrumentation/NumericalStabilitySanitizer.h"
#include "core/passes/Instrumentation/PGOCtxProfFlattening.h"
#include "core/passes/Instrumentation/PGOCtxProfLowering.h"
#include "core/passes/Instrumentation/PGOForceFunctionAttrs.h"
#include "core/passes/Instrumentation/PGOInstrumentation.h"
#include "core/passes/Instrumentation/RealtimeSanitizer.h"
#include "core/passes/Instrumentation/SanitizerBinaryMetadata.h"
#include "core/passes/Instrumentation/SanitizerCoverage.h"
#include "core/passes/Instrumentation/ThreadSanitizer.h"
#include "core/passes/Instrumentation/TypeSanitizer.h"
#include "core/passes/ObjCARC.h"
#include "core/passes/Scalar/ADCE.h"
#include "core/passes/Scalar/AlignmentFromAssumptions.h"
#include "core/passes/Scalar/AnnotationRemarks.h"
#include "core/passes/Scalar/BDCE.h"
#include "core/passes/Scalar/CallSiteSplitting.h"
#include "core/passes/Scalar/ConstantHoisting.h"
#include "core/passes/Scalar/ConstraintElimination.h"
#include "core/passes/Scalar/CorrelatedValuePropagation.h"
#include "core/passes/Scalar/DCE.h"
#include "core/passes/Scalar/DFAJumpThreading.h"
#include "core/passes/Scalar/DeadStoreElimination.h"
#include "core/passes/Scalar/DivRemPairs.h"
#include "core/passes/Scalar/DropUnnecessaryAssumes.h"
#include "core/passes/Scalar/EarlyCSE.h"
#include "core/passes/Scalar/ExpandMemCmp.h"
#include "core/passes/Scalar/FlattenCFG.h"
#include "core/passes/Scalar/Float2Int.h"
#include "core/passes/Scalar/GVN.h"
#include "core/passes/Scalar/GuardWidening.h"
#include "core/passes/Scalar/IVUsersPrinter.h"
#include "core/passes/Scalar/IndVarSimplify.h"
#include "core/passes/Scalar/InductiveRangeCheckElimination.h"
#include "core/passes/Scalar/InferAddressSpaces.h"
#include "core/passes/Scalar/InferAlignment.h"
#include "core/passes/Scalar/InstSimplifyPass.h"
#include "core/passes/Scalar/JumpTableToSwitch.h"
#include "core/passes/Scalar/JumpThreading.h"
#include "core/passes/Scalar/LICM.h"
#include "core/passes/Scalar/LoopAccessAnalysisPrinter.h"
#include "core/passes/Scalar/LoopBoundSplit.h"
#include "core/passes/Scalar/LoopDataPrefetch.h"
#include "core/passes/Scalar/LoopDeletion.h"
#include "core/passes/Scalar/LoopDistribute.h"
#include "core/passes/Scalar/LoopFlatten.h"
#include "core/passes/Scalar/LoopFuse.h"
#include "core/passes/Scalar/LoopIdiomRecognize.h"
#include "core/passes/Scalar/LoopInstSimplify.h"
#include "core/passes/Scalar/LoopInterchange.h"
#include "core/passes/Scalar/LoopLoadElimination.h"
#include "core/passes/Scalar/LoopPassManager.h"
#include "core/passes/Scalar/LoopPredication.h"
#include "core/passes/Scalar/LoopRotation.h"
#include "core/passes/Scalar/LoopSimplifyCFG.h"
#include "core/passes/Scalar/LoopSink.h"
#include "core/passes/Scalar/LoopStrengthReduce.h"
#include "core/passes/Scalar/LoopTermFold.h"
#include "core/passes/Scalar/LoopUnrollAndJamPass.h"
#include "core/passes/Scalar/LoopUnrollPass.h"
#include "core/passes/Scalar/LoopVersioningLICM.h"
#include "core/passes/Scalar/LowerAtomicPass.h"
#include "core/passes/Scalar/LowerConstantIntrinsics.h"
#include "core/passes/Scalar/LowerExpectIntrinsic.h"
#include "core/passes/Scalar/LowerGuardIntrinsic.h"
#include "core/passes/Scalar/LowerMatrixIntrinsics.h"
#include "core/passes/Scalar/LowerWidenableCondition.h"
#include "core/passes/Scalar/MakeGuardsExplicit.h"
#include "core/passes/Scalar/MemCpyOptimizer.h"
#include "core/passes/Scalar/MergeICmps.h"
#include "core/passes/Scalar/MergedLoadStoreMotion.h"
#include "core/passes/Scalar/NaryReassociate.h"
#include "core/passes/Scalar/NewGVN.h"
#include "core/passes/Scalar/PartiallyInlineLibCalls.h"
#include "core/passes/Scalar/PlaceSafepoints.h"
#include "core/passes/Scalar/Reassociate.h"
#include "core/passes/Scalar/Reg2Mem.h"
#include "core/passes/Scalar/RewriteStatepointsForGC.h"
#include "core/passes/Scalar/SCCP.h"
#include "core/passes/Scalar/SROA.h"
#include "core/passes/Scalar/ScalarizeMaskedMemIntrin.h"
#include "core/passes/Scalar/Scalarizer.h"
#include "core/passes/Scalar/SeparateConstOffsetFromGEP.h"
#include "core/passes/Scalar/SimpleLoopUnswitch.h"
#include "core/passes/Scalar/SimplifyCFG.h"
#include "core/passes/Scalar/Sink.h"
#include "core/passes/Scalar/SpeculativeExecution.h"
#include "core/passes/Scalar/StraightLineStrengthReduce.h"
#include "core/passes/Scalar/StructurizeCFG.h"
#include "core/passes/Scalar/TailRecursionElimination.h"
#include "core/passes/Scalar/WarnMissedTransforms.h"
#include "core/passes/Utils/AddDiscriminators.h"
#include "core/passes/Utils/AssumeBundleBuilder.h"
#include "core/passes/Utils/BreakCriticalEdges.h"
#include "core/passes/Utils/CanonicalizeAliases.h"
#include "core/passes/Utils/CanonicalizeFreezeInLoops.h"
#include "core/passes/Utils/CountVisits.h"
#include "core/passes/Utils/DXILUpgrade.h"
#include "core/passes/Utils/Debugify.h"
#include "core/passes/Utils/DeclareRuntimeLibcalls.h"
#include "core/passes/Utils/EntryExitInstrumenter.h"
#include "core/passes/Utils/FixIrreducible.h"
#include "core/passes/Utils/HelloWorld.h"
#include "core/passes/Utils/IRNormalizer.h"
#include "core/passes/Utils/InjectTLIMappings.h"
#include "core/passes/Utils/InstructionNamer.h"
#include "core/passes/Utils/LibCallsShrinkWrap.h"
#include "core/passes/Utils/LoopSimplify.h"
#include "core/passes/Utils/LoopVersioning.h"
#include "core/passes/Utils/LowerCommentStringPass.h"
#include "core/passes/Utils/LowerGlobalDtors.h"
#include "core/passes/Utils/LowerIFunc.h"
#include "core/passes/Utils/LowerInvoke.h"
#include "core/passes/Utils/LowerSwitch.h"
#include "core/passes/Utils/Mem2Reg.h"
#include "core/passes/Utils/MetaRenamer.h"
#include "core/passes/Utils/MoveAutoInit.h"
#include "core/passes/Utils/NameAnonGlobals.h"
#include "core/passes/Utils/PredicateInfo.h"
#include "core/passes/Utils/ProfileVerify.h"
#include "core/passes/Utils/RelLookupTableConverter.h"
#include "core/passes/Utils/StripConvergenceIntrinsics.h"
#include "core/passes/Utils/StripGCRelocates.h"
#include "core/passes/Utils/StripNonLineTableDebugInfo.h"
#include "core/passes/Utils/SymbolRewriter.h"
#include "core/passes/Utils/TriggerCrashPass.h"
#include "core/passes/Utils/UnifyLoopExits.h"
#include "core/passes/Vectorize/LoadStoreVectorizer.h"
#include "core/passes/Vectorize/LoopIdiomVectorize.h"
#include "core/passes/Vectorize/LoopVectorize.h"
#include "core/passes/Vectorize/SLPVectorizer.h"
// llvm-kain: SandboxVectorizer removed (SandboxIR deleted in Phase 3)
// #include "core/passes/Vectorize/SandboxVectorizer/SandboxVectorizer.h"
#include "core/passes/Vectorize/VectorCombine.h"
#include <optional>

using namespace llvm;

cl::opt<std::optional<PrintPipelinePassesFormat>, false,
        PrintPipelinePassesFormatParser>
    llvm::PrintPipelinePasses(
        "print-pipeline-passes", cl::ValueOptional,
        cl::desc(
            "Print string describing the pipeline (best-effort only).\n"
            "  - =text\tPrint a '-passes' compatible string describing the "
            "pipeline.\n"
            "  - =tree\tPrint a tree-like structure describing the pipeline."));

bool PrintPipelinePassesFormatParser::parse(
    cl::Option &O, StringRef ArgName, StringRef Arg,
    std::optional<PrintPipelinePassesFormat> &Val) {
  std::optional<PrintPipelinePassesFormat> Format =
      StringSwitch<std::optional<PrintPipelinePassesFormat>>(Arg)
          .Case("text", PrintPipelinePassesFormat::Text)
          .Case("", PrintPipelinePassesFormat::Text)
          .Case("tree", PrintPipelinePassesFormat::Tree)
          .Default(std::nullopt);

  if (!Format)
    return O.error(formatv(
        "'{0}' value invalid for print-pipeline-passes argument!", Arg));

  Val = Format;
  return false;
}

void llvm::printFormattedPipelinePasses(raw_ostream &OS, StringRef Pipeline,
                                        PrintPipelinePassesFormat Format) {
  switch (Format) {
  case PrintPipelinePassesFormat::Text:
    OS << Pipeline;
    break;
  case PrintPipelinePassesFormat::Tree: {
    int IndentLevel = 0;
    for (char C : Pipeline) {
      switch (C) {
      case '(':
        ++IndentLevel;
        OS << formatv("\n{0}", fmt_repeat("  ", IndentLevel));
        break;
      case ')':
        --IndentLevel;
        assert(IndentLevel >= 0 && "Invalid pipeline string!");
        break;
      case ',':
        OS << formatv("\n{0}", fmt_repeat("  ", IndentLevel));
        break;
      default:
        OS << C;
      }
    }
    break;
  }
  }
}

AnalysisKey NoOpModuleAnalysis::Key;
AnalysisKey NoOpCGSCCAnalysis::Key;
AnalysisKey NoOpFunctionAnalysis::Key;
AnalysisKey NoOpLoopAnalysis::Key;

namespace {

bool applyMIRDebugify(DIBuilder &DIB, Function &F, ModuleAnalysisManager &AM) {
  FunctionAnalysisManager &FAM =
      AM.getResult<FunctionAnalysisManagerModuleProxy>(*F.getParent())
          .getManager();

  return applyDebugifyMetadataToMachineFunction(
      DIB, F, [&](Function &Func) -> MachineFunction * {
        MachineFunctionAnalysis::Result *MFA =
            FAM.getCachedResult<MachineFunctionAnalysis>(Func);
        return MFA ? &MFA->getMF() : nullptr;
      });
}

// A pass for testing message reporting of -verify-each failures.
// DO NOT USE THIS EXCEPT FOR TESTING!
class TriggerVerifierErrorPass
    : public OptionalPassInfoMixin<TriggerVerifierErrorPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    // Intentionally break the Module by creating an alias without setting the
    // aliasee.
    auto *PtrTy = PointerType::getUnqual(M.getContext());
    GlobalAlias::create(PtrTy, PtrTy->getAddressSpace(),
                        GlobalValue::LinkageTypes::InternalLinkage,
                        "__bad_alias", nullptr, &M);
    return PreservedAnalyses::none();
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    // Intentionally break the Function by inserting a terminator
    // instruction in the middle of a basic block.
    BasicBlock &BB = F.getEntryBlock();
    new UnreachableInst(F.getContext(), BB.getTerminator()->getIterator());
    return PreservedAnalyses::none();
  }

  PreservedAnalyses run(MachineFunction &MF, MachineFunctionAnalysisManager &) {
    // Intentionally create a virtual register and set NoVRegs property.
    auto &MRI = MF.getRegInfo();
    MRI.createGenericVirtualRegister(LLT::scalar(8));
    MF.getProperties().setNoVRegs();
    return PreservedAnalyses::all();
  }

  static StringRef name() { return "TriggerVerifierErrorPass"; }
};

// A pass requires all MachineFunctionProperties.
// DO NOT USE THIS EXCEPT FOR TESTING!
class RequireAllMachineFunctionPropertiesPass
    : public OptionalPassInfoMixin<RequireAllMachineFunctionPropertiesPass> {
public:
  PreservedAnalyses run(MachineFunction &MF, MachineFunctionAnalysisManager &) {
    MFPropsModifier _(*this, MF);
    return PreservedAnalyses::none();
  }

  static MachineFunctionProperties getRequiredProperties() {
    return MachineFunctionProperties()
        .setFailedISel()
        .setFailsVerification()
        .setIsSSA()
        .setLegalized()
        .setNoPHIs()
        .setNoVRegs()
        .setRegBankSelected()
        .setSelected()
        .setTiedOpsRewritten()
        .setTracksDebugUserValues()
        .setTracksLiveness();
  }
  static StringRef name() { return "RequireAllMachineFunctionPropertiesPass"; }
};

} // namespace

static std::optional<OptimizationLevel> parseOptLevel(StringRef S) {
  if (S == "Os" || S == "Oz")
    reportFatalUsageError(
        Twine("The optimization level \"") + S +
        "\" is no longer supported. Use O2 in conjunction with the " +
        (S == "Os" ? "optsize" : "minsize") + " attribute instead.");

  return StringSwitch<std::optional<OptimizationLevel>>(S)
      .Case("O0", OptimizationLevel::O0)
      .Case("O1", OptimizationLevel::O1)
      .Case("O2", OptimizationLevel::O2)
      .Case("O3", OptimizationLevel::O3)
      .Default(std::nullopt);
}

static Expected<OptimizationLevel> parseOptLevelParam(StringRef S) {
  std::optional<OptimizationLevel> OptLevel = parseOptLevel(S);
  if (OptLevel)
    return *OptLevel;
  return make_error<StringError>(
      formatv("invalid optimization level '{}'", S).str(),
      inconvertibleErrorCode());
}

PassBuilder::PassBuilder(TargetMachine *TM, PipelineTuningOptions PTO,
                         std::optional<PGOOptions> PGOOpt,
                         PassInstrumentationCallbacks *PIC,
                         IntrusiveRefCntPtr<vfs::FileSystem> FS)
    : TM(TM), PTO(PTO), PGOOpt(PGOOpt), PIC(PIC), FS(std::move(FS)) {
  if (TM)
    TM->registerPassBuilderCallbacks(*this);
  if (PIC) {
    PIC->registerClassToPassNameCallback([this, PIC]() {
      // MSVC requires this to be captured if it's used inside decltype.
      // Other compilers consider it an unused lambda capture.
      (void)this;
#define MODULE_PASS(NAME, CREATE_PASS)                                         \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define MODULE_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)      \
  PIC->addClassToPassName(CLASS, NAME);
#define MODULE_ANALYSIS(NAME, CREATE_PASS)                                     \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define FUNCTION_PASS(NAME, CREATE_PASS)                                       \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)    \
  PIC->addClassToPassName(CLASS, NAME);
#define FUNCTION_ANALYSIS(NAME, CREATE_PASS)                                   \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define LOOPNEST_PASS(NAME, CREATE_PASS)                                       \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define LOOP_PASS(NAME, CREATE_PASS)                                           \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)        \
  PIC->addClassToPassName(CLASS, NAME);
#define LOOP_ANALYSIS(NAME, CREATE_PASS)                                       \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define CGSCC_PASS(NAME, CREATE_PASS)                                          \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define CGSCC_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)       \
  PIC->addClassToPassName(CLASS, NAME);
#define CGSCC_ANALYSIS(NAME, CREATE_PASS)                                      \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#include "PassRegistry.def"

#define MACHINE_FUNCTION_ANALYSIS(NAME, CREATE_PASS)                           \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define MACHINE_FUNCTION_PASS(NAME, CREATE_PASS)                               \
  PIC->addClassToPassName(decltype(CREATE_PASS)::name(), NAME);
#define MACHINE_FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER,    \
                                          PARAMS)                              \
  PIC->addClassToPassName(CLASS, NAME);
#include "core/passes/MachinePassRegistry.def"
    });
  }

  // Module-level callbacks without LTO phase
  registerPipelineParsingCallback(
      [this](StringRef Name, ModulePassManager &PM,
             ArrayRef<PassBuilder::PipelineElement>) {
#define MODULE_CALLBACK(NAME, INVOKE)                                          \
  if (PassBuilder::checkParametrizedPassName(Name, NAME)) {                    \
    auto L = PassBuilder::parsePassParameters(parseOptLevelParam, Name, NAME); \
    if (!L) {                                                                  \
      errs() << NAME ": " << toString(L.takeError()) << '\n';                  \
      return false;                                                            \
    }                                                                          \
    INVOKE(PM, L.get());                                                       \
    return true;                                                               \
  }
#include "PassRegistry.def"
        return false;
      });

  // Module-level callbacks with LTO phase (use Phase::None for string API)
  registerPipelineParsingCallback(
      [this](StringRef Name, ModulePassManager &PM,
             ArrayRef<PassBuilder::PipelineElement>) {
#define MODULE_LTO_CALLBACK(NAME, INVOKE)                                      \
  if (PassBuilder::checkParametrizedPassName(Name, NAME)) {                    \
    auto L = PassBuilder::parsePassParameters(parseOptLevelParam, Name, NAME); \
    if (!L) {                                                                  \
      errs() << NAME ": " << toString(L.takeError()) << '\n';                  \
      return false;                                                            \
    }                                                                          \
    INVOKE(PM, L.get(), ThinOrFullLTOPhase::None);                             \
    return true;                                                               \
  }
#include "PassRegistry.def"
        return false;
      });

  // Function-level callbacks
  registerPipelineParsingCallback(
      [this](StringRef Name, FunctionPassManager &PM,
             ArrayRef<PassBuilder::PipelineElement>) {
#define FUNCTION_CALLBACK(NAME, INVOKE)                                        \
  if (PassBuilder::checkParametrizedPassName(Name, NAME)) {                    \
    auto L = PassBuilder::parsePassParameters(parseOptLevelParam, Name, NAME); \
    if (!L) {                                                                  \
      errs() << NAME ": " << toString(L.takeError()) << '\n';                  \
      return false;                                                            \
    }                                                                          \
    INVOKE(PM, L.get());                                                       \
    return true;                                                               \
  }
#include "PassRegistry.def"
        return false;
      });

  // CGSCC-level callbacks
  registerPipelineParsingCallback(
      [this](StringRef Name, CGSCCPassManager &PM,
             ArrayRef<PassBuilder::PipelineElement>) {
#define CGSCC_CALLBACK(NAME, INVOKE)                                           \
  if (PassBuilder::checkParametrizedPassName(Name, NAME)) {                    \
    auto L = PassBuilder::parsePassParameters(parseOptLevelParam, Name, NAME); \
    if (!L) {                                                                  \
      errs() << NAME ": " << toString(L.takeError()) << '\n';                  \
      return false;                                                            \
    }                                                                          \
    INVOKE(PM, L.get());                                                       \
    return true;                                                               \
  }
#include "PassRegistry.def"
        return false;
      });

  // Loop-level callbacks
  registerPipelineParsingCallback(
      [this](StringRef Name, LoopPassManager &PM,
             ArrayRef<PassBuilder::PipelineElement>) {
#define LOOP_CALLBACK(NAME, INVOKE)                                            \
  if (PassBuilder::checkParametrizedPassName(Name, NAME)) {                    \
    auto L = PassBuilder::parsePassParameters(parseOptLevelParam, Name, NAME); \
    if (!L) {                                                                  \
      errs() << NAME ": " << toString(L.takeError()) << '\n';                  \
      return false;                                                            \
    }                                                                          \
    INVOKE(PM, L.get());                                                       \
    return true;                                                               \
  }
#include "PassRegistry.def"
        return false;
      });
}

void PassBuilder::registerModuleAnalyses(ModuleAnalysisManager &MAM) {
#define MODULE_ANALYSIS(NAME, CREATE_PASS)                                     \
  MAM.registerPass([&] { return CREATE_PASS; });
#include "PassRegistry.def"

  for (auto &C : ModuleAnalysisRegistrationCallbacks)
    C(MAM);
}

void PassBuilder::registerCGSCCAnalyses(CGSCCAnalysisManager &CGAM) {
#define CGSCC_ANALYSIS(NAME, CREATE_PASS)                                      \
  CGAM.registerPass([&] { return CREATE_PASS; });
#include "PassRegistry.def"

  for (auto &C : CGSCCAnalysisRegistrationCallbacks)
    C(CGAM);
}

void PassBuilder::registerFunctionAnalyses(FunctionAnalysisManager &FAM) {
  // We almost always want the default alias analysis pipeline.
  // If a user wants a different one, they can register their own before calling
  // registerFunctionAnalyses().
  FAM.registerPass([&] { return buildDefaultAAPipeline(); });

#define FUNCTION_ANALYSIS(NAME, CREATE_PASS)                                   \
  if constexpr (std::is_constructible_v<                                       \
                    std::remove_reference_t<decltype(CREATE_PASS)>,            \
                    const TargetMachine &>) {                                  \
    if (TM)                                                                    \
      FAM.registerPass([&] { return CREATE_PASS; });                           \
  } else {                                                                     \
    FAM.registerPass([&] { return CREATE_PASS; });                             \
  }
#include "PassRegistry.def"

  for (auto &C : FunctionAnalysisRegistrationCallbacks)
    C(FAM);
}

void PassBuilder::registerMachineFunctionAnalyses(
    MachineFunctionAnalysisManager &MFAM) {

#define MACHINE_FUNCTION_ANALYSIS(NAME, CREATE_PASS)                           \
  MFAM.registerPass([&] { return CREATE_PASS; });
#include "core/passes/MachinePassRegistry.def"

  for (auto &C : MachineFunctionAnalysisRegistrationCallbacks)
    C(MFAM);
}

void PassBuilder::registerLoopAnalyses(LoopAnalysisManager &LAM) {
#define LOOP_ANALYSIS(NAME, CREATE_PASS)                                       \
  LAM.registerPass([&] { return CREATE_PASS; });
#include "PassRegistry.def"

  for (auto &C : LoopAnalysisRegistrationCallbacks)
    C(LAM);
}

static std::optional<std::pair<bool, bool>>
parseFunctionPipelineName(StringRef Name) {
  std::pair<bool, bool> Params;
  if (!Name.consume_front("function"))
    return std::nullopt;
  if (Name.empty())
    return Params;
  if (!Name.consume_front("<") || !Name.consume_back(">"))
    return std::nullopt;
  while (!Name.empty()) {
    auto [Front, Back] = Name.split(';');
    Name = Back;
    if (Front == "eager-inv")
      Params.first = true;
    else if (Front == "no-rerun")
      Params.second = true;
    else
      return std::nullopt;
  }
  return Params;
}

static std::optional<int> parseDevirtPassName(StringRef Name) {
  if (!Name.consume_front("devirt<") || !Name.consume_back(">"))
    return std::nullopt;
  int Count;
  if (Name.getAsInteger(0, Count) || Count < 0)
    return std::nullopt;
  return Count;
}

Expected<bool> PassBuilder::parseSinglePassOption(StringRef Params,
                                                  StringRef OptionName,
                                                  StringRef PassName) {
  bool Result = false;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == OptionName) {
      Result = true;
    } else {
      return make_error<StringError>(
          formatv("invalid {} pass parameter '{}'", PassName, ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

namespace {

/// Parser of parameters for HardwareLoops  pass.
Expected<HardwareLoopOptions> parseHardwareLoopOptions(StringRef Params) {
  HardwareLoopOptions HardwareLoopOpts;

  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');
    if (ParamName.consume_front("hardware-loop-decrement=")) {
      int Count;
      if (ParamName.getAsInteger(0, Count))
        return make_error<StringError>(
            formatv("invalid HardwareLoopPass parameter '{}'", ParamName).str(),
            inconvertibleErrorCode());
      HardwareLoopOpts.setDecrement(Count);
      continue;
    }
    if (ParamName.consume_front("hardware-loop-counter-bitwidth=")) {
      int Count;
      if (ParamName.getAsInteger(0, Count))
        return make_error<StringError>(
            formatv("invalid HardwareLoopPass parameter '{}'", ParamName).str(),
            inconvertibleErrorCode());
      HardwareLoopOpts.setCounterBitwidth(Count);
      continue;
    }
    if (ParamName == "force-hardware-loops") {
      HardwareLoopOpts.setForce(true);
    } else if (ParamName == "force-hardware-loop-phi") {
      HardwareLoopOpts.setForcePhi(true);
    } else if (ParamName == "force-nested-hardware-loop") {
      HardwareLoopOpts.setForceNested(true);
    } else if (ParamName == "force-hardware-loop-guard") {
      HardwareLoopOpts.setForceGuard(true);
    } else {
      return make_error<StringError>(
          formatv("invalid HardwarePass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return HardwareLoopOpts;
}

/// Parser of parameters for Lint pass.
Expected<bool> parseLintOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "abort-on-error",
                                            "LintPass");
}

/// Parser of parameters for FunctionPropertiesStatistics pass.
Expected<bool> parseFunctionPropertiesStatisticsOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "pre-opt",
                                            "FunctionPropertiesStatisticsPass");
}

/// Parser of parameters for InstCount pass.
Expected<bool> parseInstCountOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "pre-opt", "InstCountPass");
}

/// Parser of parameters for LoopUnroll pass.
Expected<LoopUnrollOptions> parseLoopUnrollOptions(StringRef Params) {
  LoopUnrollOptions UnrollOpts;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');
    std::optional<OptimizationLevel> OptLevel = parseOptLevel(ParamName);
    if (OptLevel) {
      UnrollOpts.setOptLevel(OptLevel->getSpeedupLevel());
      continue;
    }
    if (ParamName.consume_front("full-unroll-max=")) {
      int Count;
      if (ParamName.getAsInteger(0, Count))
        return make_error<StringError>(
            formatv("invalid LoopUnrollPass parameter '{}'", ParamName).str(),
            inconvertibleErrorCode());
      UnrollOpts.setFullUnrollMaxCount(Count);
      continue;
    }

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "partial") {
      UnrollOpts.setPartial(Enable);
    } else if (ParamName == "peeling") {
      UnrollOpts.setPeeling(Enable);
    } else if (ParamName == "profile-peeling") {
      UnrollOpts.setProfileBasedPeeling(Enable);
    } else if (ParamName == "runtime") {
      UnrollOpts.setRuntime(Enable);
    } else if (ParamName == "upperbound") {
      UnrollOpts.setUpperBound(Enable);
    } else {
      return make_error<StringError>(
          formatv("invalid LoopUnrollPass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return UnrollOpts;
}

Expected<bool> parseGlobalDCEPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(
      Params, "vfe-linkage-unit-visibility", "GlobalDCE");
}

Expected<bool> parseCGProfilePassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "in-lto-post-link",
                                            "CGProfile");
}

Expected<bool> parseInlinerPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "only-mandatory",
                                            "InlinerPass");
}

Expected<bool> parseCoroSplitPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "reuse-storage",
                                            "CoroSplitPass");
}

Expected<bool> parsePostOrderFunctionAttrsPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(
      Params, "skip-non-recursive-function-attrs", "PostOrderFunctionAttrs");
}

Expected<bool> parseEarlyCSEPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "memssa", "EarlyCSE");
}

Expected<bool> parseEntryExitInstrumenterPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "post-inline",
                                            "EntryExitInstrumenter");
}

Expected<bool> parseDropUnnecessaryAssumesPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "drop-deref",
                                            "DropUnnecessaryAssumes");
}

Expected<bool> parseLoopExtractorPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "single", "LoopExtractor");
}

Expected<bool> parseLowerMatrixIntrinsicsPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "minimal",
                                            "LowerMatrixIntrinsics");
}

Expected<IRNormalizerOptions> parseIRNormalizerPassOptions(StringRef Params) {
  IRNormalizerOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "preserve-order")
      Result.PreserveOrder = Enable;
    else if (ParamName == "rename-all")
      Result.RenameAll = Enable;
    else if (ParamName == "fold-all") // FIXME: Name mismatch
      Result.FoldPreOutputs = Enable;
    else if (ParamName == "reorder-operands")
      Result.ReorderOperands = Enable;
    else {
      return make_error<StringError>(
          formatv("invalid normalize pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }

  return Result;
}

Expected<AddressSanitizerOptions> parseASanPassOptions(StringRef Params) {
  AddressSanitizerOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "kernel") {
      Result.CompileKernel = true;
    } else if (ParamName == "use-after-scope") {
      Result.UseAfterScope = true;
    } else {
      return make_error<StringError>(
          formatv("invalid AddressSanitizer pass parameter '{}'", ParamName)
              .str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<HWAddressSanitizerOptions> parseHWASanPassOptions(StringRef Params) {
  HWAddressSanitizerOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "recover") {
      Result.Recover = true;
    } else if (ParamName == "kernel") {
      Result.CompileKernel = true;
    } else {
      return make_error<StringError>(
          formatv("invalid HWAddressSanitizer pass parameter '{}'", ParamName)
              .str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<lowertypetests::DropTestKind>
parseDropTypeTestsPassOptions(StringRef Params) {
  lowertypetests::DropTestKind Result = lowertypetests::DropTestKind::Assume;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "all") {
      Result = lowertypetests::DropTestKind::All;
    } else if (ParamName == "assume") {
      Result = lowertypetests::DropTestKind::Assume;
    } else {
      return make_error<StringError>(
          formatv("invalid DropTypeTestsPass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<EmbedBitcodeOptions> parseEmbedBitcodePassOptions(StringRef Params) {
  EmbedBitcodeOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "thinlto") {
      Result.IsThinLTO = true;
    } else if (ParamName == "emit-summary") {
      Result.EmitLTOSummary = true;
    } else {
      return make_error<StringError>(
          formatv("invalid EmbedBitcode pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<LowerAllowCheckPass::Options>
parseLowerAllowCheckPassOptions(StringRef Params) {
  LowerAllowCheckPass::Options Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    // Format is <cutoffs[1,2,3]=70000;cutoffs[5,6,8]=90000>
    //
    // Parsing allows duplicate indices (last one takes precedence).
    // It would technically be in spec to specify
    //   cutoffs[0]=70000,cutoffs[1]=90000,cutoffs[0]=80000,...
    if (ParamName.starts_with("cutoffs[")) {
      StringRef IndicesStr;
      StringRef CutoffStr;

      std::tie(IndicesStr, CutoffStr) = ParamName.split("]=");
      //       cutoffs[1,2,3
      //                   70000

      int cutoff;
      if (CutoffStr.getAsInteger(0, cutoff))
        return make_error<StringError>(
            formatv("invalid LowerAllowCheck pass cutoffs parameter '{}' ({})",
                    CutoffStr, Params)
                .str(),
            inconvertibleErrorCode());

      if (!IndicesStr.consume_front("cutoffs[") || IndicesStr == "")
        return make_error<StringError>(
            formatv("invalid LowerAllowCheck pass index parameter '{}' ({})",
                    IndicesStr, CutoffStr)
                .str(),
            inconvertibleErrorCode());

      while (IndicesStr != "") {
        StringRef firstIndexStr;
        std::tie(firstIndexStr, IndicesStr) = IndicesStr.split('|');

        unsigned int index;
        if (firstIndexStr.getAsInteger(0, index))
          return make_error<StringError>(
              formatv(
                  "invalid LowerAllowCheck pass index parameter '{}' ({}) {}",
                  firstIndexStr, IndicesStr)
                  .str(),
              inconvertibleErrorCode());

        // In the common case (sequentially increasing indices), we will issue
        // O(n) resize requests. We assume the underlying data structure has
        // O(1) runtime for each added element.
        if (index >= Result.cutoffs.size())
          Result.cutoffs.resize(index + 1, 0);

        Result.cutoffs[index] = cutoff;
      }
    } else if (ParamName.starts_with("runtime_check")) {
      StringRef ValueString;
      std::tie(std::ignore, ValueString) = ParamName.split("=");
      int runtime_check;
      if (ValueString.getAsInteger(0, runtime_check)) {
        return make_error<StringError>(
            formatv("invalid LowerAllowCheck pass runtime_check parameter '{}' "
                    "({})",
                    ValueString, Params)
                .str(),
            inconvertibleErrorCode());
      }
      Result.runtime_check = runtime_check;
    } else {
      return make_error<StringError>(
          formatv("invalid LowerAllowCheck pass parameter '{}'", ParamName)
              .str(),
          inconvertibleErrorCode());
    }
  }

  return Result;
}

Expected<MemorySanitizerOptions> parseMSanPassOptions(StringRef Params) {
  MemorySanitizerOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "recover") {
      Result.Recover = true;
    } else if (ParamName == "kernel") {
      Result.Kernel = true;
    } else if (ParamName.consume_front("track-origins=")) {
      if (ParamName.getAsInteger(0, Result.TrackOrigins))
        return make_error<StringError>(
            formatv("invalid argument to MemorySanitizer pass track-origins "
                    "parameter: '{}'",
                    ParamName)
                .str(),
            inconvertibleErrorCode());
    } else if (ParamName == "eager-checks") {
      Result.EagerChecks = true;
    } else {
      return make_error<StringError>(
          formatv("invalid MemorySanitizer pass parameter '{}'", ParamName)
              .str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<AllocTokenOptions> parseAllocTokenPassOptions(StringRef Params) {
  AllocTokenOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName.consume_front("mode=")) {
      if (auto Mode = getAllocTokenModeFromString(ParamName))
        Result.Mode = *Mode;
      else
        return make_error<StringError>(
            formatv("invalid argument to AllocToken pass mode "
                    "parameter: '{}'",
                    ParamName)
                .str(),
            inconvertibleErrorCode());
    } else {
      return make_error<StringError>(
          formatv("invalid AllocToken pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

/// Parser of parameters for SimplifyCFG pass.
Expected<SimplifyCFGOptions> parseSimplifyCFGOptions(StringRef Params) {
  SimplifyCFGOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "speculate-blocks") {
      Result.speculateBlocks(Enable);
    } else if (ParamName == "simplify-cond-branch") {
      Result.setSimplifyCondBranch(Enable);
    } else if (ParamName == "forward-switch-cond") {
      Result.forwardSwitchCondToPhi(Enable);
    } else if (ParamName == "switch-range-to-icmp") {
      Result.convertSwitchRangeToICmp(Enable);
    } else if (ParamName == "switch-to-arithmetic") {
      Result.convertSwitchToArithmetic(Enable);
    } else if (ParamName == "switch-to-lookup") {
      Result.convertSwitchToLookupTable(Enable);
    } else if (ParamName == "keep-loops") {
      Result.needCanonicalLoops(Enable);
    } else if (ParamName == "hoist-common-insts") {
      Result.hoistCommonInsts(Enable);
    } else if (ParamName == "hoist-loads-stores-with-cond-faulting") {
      Result.hoistLoadsStoresWithCondFaulting(Enable);
    } else if (ParamName == "sink-common-insts") {
      Result.sinkCommonInsts(Enable);
    } else if (ParamName == "speculate-unpredictables") {
      Result.speculateUnpredictables(Enable);
    } else if (Enable && ParamName.consume_front("bonus-inst-threshold=")) {
      APInt BonusInstThreshold;
      if (ParamName.getAsInteger(0, BonusInstThreshold))
        return make_error<StringError>(
            formatv("invalid argument to SimplifyCFG pass bonus-threshold "
                    "parameter: '{}'",
                    ParamName)
                .str(),
            inconvertibleErrorCode());
      Result.bonusInstThreshold(BonusInstThreshold.getSExtValue());
    } else {
      return make_error<StringError>(
          formatv("invalid SimplifyCFG pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<InstCombineOptions> parseInstCombineOptions(StringRef Params) {
  InstCombineOptions Result;
  // When specifying "instcombine" in -passes enable fix-point verification by
  // default, as this is what most tests should use.
  Result.setVerifyFixpoint(true);
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "verify-fixpoint") {
      Result.setVerifyFixpoint(Enable);
    } else if (Enable && ParamName.consume_front("max-iterations=")) {
      APInt MaxIterations;
      if (ParamName.getAsInteger(0, MaxIterations))
        return make_error<StringError>(
            formatv("invalid argument to InstCombine pass max-iterations "
                    "parameter: '{}'",
                    ParamName)
                .str(),
            inconvertibleErrorCode());
      Result.setMaxIterations((unsigned)MaxIterations.getZExtValue());
    } else {
      return make_error<StringError>(
          formatv("invalid InstCombine pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

/// Parser of parameters for LoopVectorize pass.
Expected<LoopVectorizeOptions> parseLoopVectorizeOptions(StringRef Params) {
  LoopVectorizeOptions Opts;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "interleave-forced-only") {
      Opts.setInterleaveOnlyWhenForced(Enable);
    } else if (ParamName == "vectorize-forced-only") {
      Opts.setVectorizeOnlyWhenForced(Enable);
    } else {
      return make_error<StringError>(
          formatv("invalid LoopVectorize parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Opts;
}

Expected<std::pair<bool, bool>> parseLoopUnswitchOptions(StringRef Params) {
  std::pair<bool, bool> Result = {false, true};
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "nontrivial") {
      Result.first = Enable;
    } else if (ParamName == "trivial") {
      Result.second = Enable;
    } else {
      return make_error<StringError>(
          formatv("invalid LoopUnswitch pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<LICMOptions> parseLICMOptions(StringRef Params) {
  LICMOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "allowspeculation") {
      Result.AllowSpeculation = Enable;
    } else {
      return make_error<StringError>(
          formatv("invalid LICM pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

struct LoopRotateOptions {
  bool EnableHeaderDuplication = true;
  bool PrepareForLTO = false;
  bool CheckExitCount = false;
};

Expected<LoopRotateOptions> parseLoopRotateOptions(StringRef Params) {
  LoopRotateOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "header-duplication") {
      Result.EnableHeaderDuplication = Enable;
    } else if (ParamName == "prepare-for-lto") {
      Result.PrepareForLTO = Enable;
    } else if (ParamName == "check-exit-count") {
      Result.CheckExitCount = Enable;
    } else {
      return make_error<StringError>(
          formatv("invalid LoopRotate pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<bool> parseMergedLoadStoreMotionOptions(StringRef Params) {
  bool Result = false;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "split-footer-bb") {
      Result = Enable;
    } else {
      return make_error<StringError>(
          formatv("invalid MergedLoadStoreMotion pass parameter '{}'",
                  ParamName)
              .str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<GVNOptions> parseGVNOptions(StringRef Params) {
  GVNOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "scalar-pre") {
      Result.setScalarPRE(Enable);
    } else if (ParamName == "load-pre") {
      Result.setLoadPRE(Enable);
    } else if (ParamName == "split-backedge-load-pre") {
      Result.setLoadPRESplitBackedge(Enable);
    } else if (ParamName == "memdep") {
      // MemDep and MemorySSA are mutually exclusive.
      Result.setMemDep(Enable);
      Result.setMemorySSA(!Enable);
    } else if (ParamName == "memoryssa") {
      // MemDep and MemorySSA are mutually exclusive.
      Result.setMemorySSA(Enable);
      Result.setMemDep(!Enable);
    } else {
      return make_error<StringError>(
          formatv("invalid GVN pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<IPSCCPOptions> parseIPSCCPOptions(StringRef Params) {
  IPSCCPOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "func-spec")
      Result.setFuncSpec(Enable);
    else
      return make_error<StringError>(
          formatv("invalid IPSCCP pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
  }
  return Result;
}

Expected<ScalarizerPassOptions> parseScalarizerOptions(StringRef Params) {
  ScalarizerPassOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName.consume_front("min-bits=")) {
      if (ParamName.getAsInteger(0, Result.ScalarizeMinBits)) {
        return make_error<StringError>(
            formatv("invalid argument to Scalarizer pass min-bits "
                    "parameter: '{}'",
                    ParamName)
                .str(),
            inconvertibleErrorCode());
      }

      continue;
    }

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "load-store")
      Result.ScalarizeLoadStore = Enable;
    else if (ParamName == "variable-insert-extract")
      Result.ScalarizeVariableInsertExtract = Enable;
    else {
      return make_error<StringError>(
          formatv("invalid Scalarizer pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }

  return Result;
}

Expected<SROAOptions> parseSROAOptions(StringRef Params) {
  SROAOptions Result(SROAOptions::ModifyCFG);
  bool SawCFGOption = false;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "modify-cfg") {
      if (SawCFGOption)
        return make_error<StringError>("multiple SROA CFG options specified",
                                       inconvertibleErrorCode());
      Result.CFG = SROAOptions::ModifyCFG;
      SawCFGOption = true;
    } else if (ParamName == "preserve-cfg") {
      if (SawCFGOption)
        return make_error<StringError>("multiple SROA CFG options specified",
                                       inconvertibleErrorCode());
      Result.CFG = SROAOptions::PreserveCFG;
      SawCFGOption = true;
    } else if (ParamName == "aggregate-to-vector") {
      Result.AggregateToVector = true;
    } else {
      return make_error<StringError>(
          formatv("invalid SROA pass parameter '{}' (expected preserve-cfg, "
                  "modify-cfg, or aggregate-to-vector)",
                  ParamName)
              .str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<StackLifetime::LivenessType>
parseStackLifetimeOptions(StringRef Params) {
  StackLifetime::LivenessType Result = StackLifetime::LivenessType::May;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "may") {
      Result = StackLifetime::LivenessType::May;
    } else if (ParamName == "must") {
      Result = StackLifetime::LivenessType::Must;
    } else {
      return make_error<StringError>(
          formatv("invalid StackLifetime parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<bool> parseDependenceAnalysisPrinterOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "normalized-results",
                                            "DependenceAnalysisPrinter");
}

Expected<bool> parseSeparateConstOffsetFromGEPPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "lower-gep",
                                            "SeparateConstOffsetFromGEP");
}

Expected<bool> parseStructurizeCFGPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "skip-uniform-regions",
                                            "StructurizeCFG");
}

Expected<OptimizationLevel>
parseFunctionSimplificationPipelineOptions(StringRef Params) {
  std::optional<OptimizationLevel> L = parseOptLevel(Params);
  if (!L || *L == OptimizationLevel::O0) {
    return make_error<StringError>(
        formatv("invalid function-simplification parameter '{}'", Params).str(),
        inconvertibleErrorCode());
  };
  return *L;
}

Expected<bool> parseMemorySSAPrinterPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "no-ensure-optimized-uses",
                                            "MemorySSAPrinterPass");
}

Expected<bool> parseSpeculativeExecutionPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "only-if-divergent-target",
                                            "SpeculativeExecutionPass");
}

Expected<std::string> parseMemProfUsePassOptions(StringRef Params) {
  std::string Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName.consume_front("profile-filename=")) {
      Result = ParamName.str();
    } else {
      return make_error<StringError>(
          formatv("invalid MemProfUse pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<StructuralHashOptions>
parseStructuralHashPrinterPassOptions(StringRef Params) {
  if (Params.empty())
    return StructuralHashOptions::None;
  if (Params == "detailed")
    return StructuralHashOptions::Detailed;
  if (Params == "call-target-ignored")
    return StructuralHashOptions::CallTargetIgnored;
  return make_error<StringError>(
      formatv("invalid structural hash printer parameter '{}'", Params).str(),
      inconvertibleErrorCode());
}

Expected<bool> parseWinEHPrepareOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "demote-catchswitch-only",
                                            "WinEHPreparePass");
}

Expected<GlobalMergeOptions> parseGlobalMergeOptions(StringRef Params) {
  GlobalMergeOptions Result;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    bool Enable = !ParamName.consume_front("no-");
    if (ParamName == "group-by-use")
      Result.GroupByUse = Enable;
    else if (ParamName == "ignore-single-use")
      Result.IgnoreSingleUse = Enable;
    else if (ParamName == "merge-const")
      Result.MergeConstantGlobals = Enable;
    else if (ParamName == "merge-const-aggressive")
      Result.MergeConstAggressive = Enable;
    else if (ParamName == "merge-external")
      Result.MergeExternal = Enable;
    else if (ParamName.consume_front("max-offset=")) {
      if (ParamName.getAsInteger(0, Result.MaxOffset))
        return make_error<StringError>(
            formatv("invalid GlobalMergePass parameter '{}'", ParamName).str(),
            inconvertibleErrorCode());
    } else {
      return make_error<StringError>(
          formatv("invalid global-merge pass parameter '{}'", Params).str(),
          inconvertibleErrorCode());
    }
  }
  return Result;
}

Expected<SmallVector<std::string, 0>> parseInternalizeGVs(StringRef Params) {
  SmallVector<std::string, 1> PreservedGVs;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName.consume_front("preserve-gv=")) {
      PreservedGVs.push_back(ParamName.str());
    } else {
      return make_error<StringError>(
          formatv("invalid Internalize pass parameter '{}'", ParamName).str(),
          inconvertibleErrorCode());
    }
  }

  return Expected<SmallVector<std::string, 0>>(std::move(PreservedGVs));
}

Expected<RegAllocFastPass::Options>
parseRegAllocFastPassOptions(PassBuilder &PB, StringRef Params) {
  RegAllocFastPass::Options Opts;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName.consume_front("filter=")) {
      std::optional<RegAllocFilterFunc> Filter =
          PB.parseRegAllocFilter(ParamName);
      if (!Filter) {
        return make_error<StringError>(
            formatv("invalid regallocfast register filter '{}'", ParamName)
                .str(),
            inconvertibleErrorCode());
      }
      Opts.Filter = *Filter;
      Opts.FilterName = ParamName;
      continue;
    }

    if (ParamName == "no-clear-vregs") {
      Opts.ClearVRegs = false;
      continue;
    }

    return make_error<StringError>(
        formatv("invalid regallocfast pass parameter '{}'", ParamName).str(),
        inconvertibleErrorCode());
  }
  return Opts;
}

Expected<BoundsCheckingPass::Options>
parseBoundsCheckingOptions(StringRef Params) {
  BoundsCheckingPass::Options Options;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');
    if (ParamName == "trap") {
      Options.Rt = std::nullopt;
    } else if (ParamName == "rt") {
      Options.Rt = {
          /*MinRuntime=*/false,
          /*MayReturn=*/true,
          /*HandlerPreserveAllRegs=*/false,
      };
    } else if (ParamName == "rt-abort") {
      Options.Rt = {
          /*MinRuntime=*/false,
          /*MayReturn=*/false,
          /*HandlerPreserveAllRegs=*/false,
      };
    } else if (ParamName == "min-rt") {
      Options.Rt = {
          /*MinRuntime=*/true,
          /*MayReturn=*/true,
          /*HandlerPreserveAllRegs=*/false,
      };
    } else if (ParamName == "min-rt-abort") {
      Options.Rt = {
          /*MinRuntime=*/true,
          /*MayReturn=*/false,
          /*HandlerPreserveAllRegs=*/false,
      };
    } else if (ParamName == "merge") {
      Options.Merge = true;
    } else if (ParamName == "handler-preserve-all-regs") {
      if (Options.Rt)
        Options.Rt->HandlerPreserveAllRegs = true;
    } else {
      StringRef ParamEQ;
      StringRef Val;
      std::tie(ParamEQ, Val) = ParamName.split('=');
      int8_t Id;
      if (ParamEQ == "guard" && !Val.getAsInteger(0, Id)) {
        Options.GuardKind = Id;
      } else {
        return make_error<StringError>(
            formatv("invalid BoundsChecking pass parameter '{}'", ParamName)
                .str(),
            inconvertibleErrorCode());
      }
    }
  }
  return Options;
}

Expected<CodeGenOptLevel> parseExpandIRInstsOptions(StringRef Param) {
  if (Param.empty())
    return CodeGenOptLevel::None;

  // Parse a CodeGenOptLevel, e.g. "O1", "O2", "O3".
  auto [Prefix, Digit] = Param.split('O');

  uint8_t N;
  if (!Prefix.empty() || Digit.getAsInteger(10, N))
    return createStringError("invalid expand-ir-insts pass parameter '%s'",
                             Param.str().c_str());

  std::optional<CodeGenOptLevel> Level = CodeGenOpt::getLevel(N);
  if (!Level.has_value())
    return createStringError(
        "invalid optimization level for expand-ir-insts pass: %s",
        Digit.str().c_str());

  return *Level;
}

Expected<RAGreedyPass::Options>
parseRegAllocGreedyFilterFunc(PassBuilder &PB, StringRef Params) {
  if (Params.empty() || Params == "all")
    return RAGreedyPass::Options();

  std::optional<RegAllocFilterFunc> Filter = PB.parseRegAllocFilter(Params);
  if (Filter)
    return RAGreedyPass::Options{*Filter, Params};

  return make_error<StringError>(
      formatv("invalid regallocgreedy register filter '{}'", Params).str(),
      inconvertibleErrorCode());
}

Expected<bool> parseMachineSinkingPassOptions(StringRef Params) {
  return PassBuilder::parseSinglePassOption(Params, "enable-sink-fold",
                                            "MachineSinkingPass");
}

Expected<bool> parseMachineBlockPlacementPassOptions(StringRef Params) {
  bool AllowTailMerge = true;
  if (!Params.empty()) {
    AllowTailMerge = !Params.consume_front("no-");
    if (Params != "tail-merge")
      return make_error<StringError>(
          formatv("invalid MachineBlockPlacementPass parameter '{}'", Params)
              .str(),
          inconvertibleErrorCode());
  }
  return AllowTailMerge;
}

Expected<bool> parseVirtRegRewriterPassOptions(StringRef Params) {
  bool ClearVirtRegs = true;
  if (!Params.empty()) {
    ClearVirtRegs = !Params.consume_front("no-");
    if (Params != "clear-vregs")
      return make_error<StringError>(
          formatv("invalid VirtRegRewriter pass parameter '{}'", Params).str(),
          inconvertibleErrorCode());
  }
  return ClearVirtRegs;
}

struct FatLTOOptions {
  OptimizationLevel OptLevel;
  bool ThinLTO = false;
  bool EmitSummary = false;
};

Expected<FatLTOOptions> parseFatLTOOptions(StringRef Params) {
  FatLTOOptions Result;
  bool HaveOptLevel = false;
  while (!Params.empty()) {
    StringRef ParamName;
    std::tie(ParamName, Params) = Params.split(';');

    if (ParamName == "thinlto") {
      Result.ThinLTO = true;
    } else if (ParamName == "emit-summary") {
      Result.EmitSummary = true;
    } else if (std::optional<OptimizationLevel> OptLevel =
                   parseOptLevel(ParamName)) {
      Result.OptLevel = *OptLevel;
      HaveOptLevel = true;
    } else {
      return make_error<StringError>(
          formatv("invalid fatlto-pre-link pass parameter '{}'", ParamName)
              .str(),
          inconvertibleErrorCode());
    }
  }
  if (!HaveOptLevel)
    return make_error<StringError>(
        "missing optimization level for fatlto-pre-link pipeline",
        inconvertibleErrorCode());
  return Result;
}

} // namespace

/// Tests whether registered callbacks will accept a given pass name.
///
/// When parsing a pipeline text, the type of the outermost pipeline may be
/// omitted, in which case the type is automatically determined from the first
/// pass name in the text. This may be a name that is handled through one of the
/// callbacks. We check this through the oridinary parsing callbacks by setting
/// up a dummy PassManager in order to not force the client to also handle this
/// type of query.
template <typename PassManagerT, typename CallbacksT>
static bool callbacksAcceptPassName(StringRef Name, CallbacksT &Callbacks) {
  if (!Callbacks.empty()) {
    PassManagerT DummyPM;
    for (auto &CB : Callbacks)
      if (CB(Name, DummyPM, {}))
        return true;
  }
  return false;
}

template <typename CallbacksT>
static bool isModulePassName(StringRef Name, CallbacksT &Callbacks) {
  StringRef NameNoBracket = Name.take_until([](char C) { return C == '<'; });

  // Explicitly handle pass manager names.
  if (Name == "module")
    return true;
  if (Name == "cgscc")
    return true;
  if (NameNoBracket == "function")
    return true;
  if (Name == "coro-cond")
    return true;

#define MODULE_PASS(NAME, CREATE_PASS)                                         \
  if (Name == NAME)                                                            \
    return true;
#define MODULE_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)      \
  if (PassBuilder::checkParametrizedPassName(Name, NAME))                      \
    return true;
#define MODULE_ANALYSIS(NAME, CREATE_PASS)                                     \
  if (Name == "require<" NAME ">" || Name == "invalidate<" NAME ">")           \
    return true;
#include "PassRegistry.def"

  return callbacksAcceptPassName<ModulePassManager>(Name, Callbacks);
}

template <typename CallbacksT>
static bool isCGSCCPassName(StringRef Name, CallbacksT &Callbacks) {
  // Explicitly handle pass manager names.
  StringRef NameNoBracket = Name.take_until([](char C) { return C == '<'; });
  if (Name == "cgscc")
    return true;
  if (NameNoBracket == "function")
    return true;

  // Explicitly handle custom-parsed pass names.
  if (parseDevirtPassName(Name))
    return true;

#define CGSCC_PASS(NAME, CREATE_PASS)                                          \
  if (Name == NAME)                                                            \
    return true;
#define CGSCC_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)       \
  if (PassBuilder::checkParametrizedPassName(Name, NAME))                      \
    return true;
#define CGSCC_ANALYSIS(NAME, CREATE_PASS)                                      \
  if (Name == "require<" NAME ">" || Name == "invalidate<" NAME ">")           \
    return true;
#include "PassRegistry.def"

  return callbacksAcceptPassName<CGSCCPassManager>(Name, Callbacks);
}

template <typename CallbacksT>
static bool isFunctionPassName(StringRef Name, CallbacksT &Callbacks) {
  // Explicitly handle pass manager names.
  StringRef NameNoBracket = Name.take_until([](char C) { return C == '<'; });
  if (NameNoBracket == "function")
    return true;
  if (Name == "loop" || Name == "loop-mssa" || Name == "machine-function")
    return true;

#define FUNCTION_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME)                                                            \
    return true;
#define FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)    \
  if (PassBuilder::checkParametrizedPassName(Name, NAME))                      \
    return true;
#define FUNCTION_ANALYSIS(NAME, CREATE_PASS)                                   \
  if (Name == "require<" NAME ">" || Name == "invalidate<" NAME ">")           \
    return true;
#include "PassRegistry.def"

  return callbacksAcceptPassName<FunctionPassManager>(Name, Callbacks);
}

template <typename CallbacksT>
static bool isMachineFunctionPassName(StringRef Name, CallbacksT &Callbacks) {
  // Explicitly handle pass manager names.
  if (Name == "machine-function")
    return true;

#define MACHINE_FUNCTION_PASS(NAME, CREATE_PASS)                               \
  if (Name == NAME)                                                            \
    return true;
#define MACHINE_FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER,    \
                                          PARAMS)                              \
  if (PassBuilder::checkParametrizedPassName(Name, NAME))                      \
    return true;

#define MACHINE_FUNCTION_ANALYSIS(NAME, CREATE_PASS)                           \
  if (Name == "require<" NAME ">" || Name == "invalidate<" NAME ">")           \
    return true;

#include "core/passes/MachinePassRegistry.def"

  return callbacksAcceptPassName<MachineFunctionPassManager>(Name, Callbacks);
}

template <typename CallbacksT>
static bool isLoopNestPassName(StringRef Name, CallbacksT &Callbacks,
                               bool &UseMemorySSA) {
  UseMemorySSA = false;

  if (PassBuilder::checkParametrizedPassName(Name, "lnicm")) {
    UseMemorySSA = true;
    return true;
  }

#define LOOPNEST_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME)                                                            \
    return true;
#include "PassRegistry.def"

  return callbacksAcceptPassName<LoopPassManager>(Name, Callbacks);
}

template <typename CallbacksT>
static bool isLoopPassName(StringRef Name, CallbacksT &Callbacks,
                           bool &UseMemorySSA) {
  UseMemorySSA = false;

  if (PassBuilder::checkParametrizedPassName(Name, "licm")) {
    UseMemorySSA = true;
    return true;
  }

#define LOOP_PASS(NAME, CREATE_PASS)                                           \
  if (Name == NAME)                                                            \
    return true;
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)        \
  if (PassBuilder::checkParametrizedPassName(Name, NAME))                      \
    return true;
#define LOOP_ANALYSIS(NAME, CREATE_PASS)                                       \
  if (Name == "require<" NAME ">" || Name == "invalidate<" NAME ">")           \
    return true;
#include "PassRegistry.def"

  return callbacksAcceptPassName<LoopPassManager>(Name, Callbacks);
}

std::optional<std::vector<PassBuilder::PipelineElement>>
PassBuilder::parsePipelineText(StringRef Text) {
  std::vector<PipelineElement> ResultPipeline;

  SmallVector<std::vector<PipelineElement> *, 4> PipelineStack = {
      &ResultPipeline};
  for (;;) {
    std::vector<PipelineElement> &Pipeline = *PipelineStack.back();
    size_t Pos = Text.find_first_of(",()");
    Pipeline.push_back({Text.substr(0, Pos), {}});

    // If we have a single terminating name, we're done.
    if (Pos == Text.npos)
      break;

    char Sep = Text[Pos];
    Text = Text.substr(Pos + 1);
    if (Sep == ',')
      // Just a name ending in a comma, continue.
      continue;

    if (Sep == '(') {
      // Push the inner pipeline onto the stack to continue processing.
      PipelineStack.push_back(&Pipeline.back().InnerPipeline);
      continue;
    }

    assert(Sep == ')' && "Bogus separator!");
    // When handling the close parenthesis, we greedily consume them to avoid
    // empty strings in the pipeline.
    do {
      // If we try to pop the outer pipeline we have unbalanced parentheses.
      if (PipelineStack.size() == 1)
        return std::nullopt;

      PipelineStack.pop_back();
    } while (Text.consume_front(")"));

    // Check if we've finished parsing.
    if (Text.empty())
      break;

    // Otherwise, the end of an inner pipeline always has to be followed by
    // a comma, and then we can continue.
    if (!Text.consume_front(","))
      return std::nullopt;
  }

  if (PipelineStack.size() > 1)
    // Unbalanced paretheses.
    return std::nullopt;

  assert(PipelineStack.back() == &ResultPipeline &&
         "Wrong pipeline at the bottom of the stack!");
  return {std::move(ResultPipeline)};
}

static void setupOptionsForPipelineAlias(PipelineTuningOptions &PTO,
                                         OptimizationLevel L) {
  PTO.LoopVectorization = L.getSpeedupLevel() > 1;
  PTO.SLPVectorization = L.getSpeedupLevel() > 1;
}

Error PassBuilder::parseModulePass(ModulePassManager &MPM,
                                   const PipelineElement &E) {
  auto &Name = E.Name;
  auto &InnerPipeline = E.InnerPipeline;

  // First handle complex passes like the pass managers which carry pipelines.
  if (!InnerPipeline.empty()) {
    if (Name == "module") {
      ModulePassManager NestedMPM;
      if (auto Err = parseModulePassPipeline(NestedMPM, InnerPipeline))
        return Err;
      MPM.addPass(std::move(NestedMPM));
      return Error::success();
    }
    if (Name == "coro-cond") {
      ModulePassManager NestedMPM;
      if (auto Err = parseModulePassPipeline(NestedMPM, InnerPipeline))
        return Err;
      MPM.addPass(CoroConditionalWrapper(std::move(NestedMPM)));
      return Error::success();
    }
    if (Name == "cgscc") {
      CGSCCPassManager CGPM;
      if (auto Err = parseCGSCCPassPipeline(CGPM, InnerPipeline))
        return Err;
      MPM.addPass(createModuleToPostOrderCGSCCPassAdaptor(std::move(CGPM)));
      return Error::success();
    }
    if (auto Params = parseFunctionPipelineName(Name)) {
      if (Params->second)
        return make_error<StringError>(
            "cannot have a no-rerun module to function adaptor",
            inconvertibleErrorCode());
      FunctionPassManager FPM;
      if (auto Err = parseFunctionPassPipeline(FPM, InnerPipeline))
        return Err;
      MPM.addPass(
          createModuleToFunctionPassAdaptor(std::move(FPM), Params->first));
      return Error::success();
    }

    for (auto &C : ModulePipelineParsingCallbacks)
      if (C(Name, MPM, InnerPipeline))
        return Error::success();

    // Normal passes can't have pipelines.
    return make_error<StringError>(
        formatv("invalid use of '{}' pass as module pipeline", Name).str(),
        inconvertibleErrorCode());
    ;
  }

  // Finally expand the basic registered passes from the .inc file.
#define MODULE_PASS(NAME, CREATE_PASS)                                         \
  if (Name == NAME) {                                                          \
    MPM.addPass(CREATE_PASS);                                                  \
    return Error::success();                                                   \
  }
#define MODULE_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)      \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    MPM.addPass(CREATE_PASS(Params.get()));                                    \
    return Error::success();                                                   \
  }
#define MODULE_ANALYSIS(NAME, CREATE_PASS)                                     \
  if (Name == "require<" NAME ">") {                                           \
    MPM.addPass(                                                               \
        RequireAnalysisPass<                                                   \
            std::remove_reference_t<decltype(CREATE_PASS)>, Module>());        \
    return Error::success();                                                   \
  }                                                                            \
  if (Name == "invalidate<" NAME ">") {                                        \
    MPM.addPass(InvalidateAnalysisPass<                                        \
                std::remove_reference_t<decltype(CREATE_PASS)>>());            \
    return Error::success();                                                   \
  }
#define CGSCC_PASS(NAME, CREATE_PASS)                                          \
  if (Name == NAME) {                                                          \
    MPM.addPass(createModuleToPostOrderCGSCCPassAdaptor(CREATE_PASS));         \
    return Error::success();                                                   \
  }
#define CGSCC_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)       \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    MPM.addPass(                                                               \
        createModuleToPostOrderCGSCCPassAdaptor(CREATE_PASS(Params.get())));   \
    return Error::success();                                                   \
  }
#define FUNCTION_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME) {                                                          \
    if constexpr (std::is_constructible_v<                                     \
                      std::remove_reference_t<decltype(CREATE_PASS)>,          \
                      const TargetMachine &>) {                                \
      if (!TM)                                                                 \
        return make_error<StringError>(                                        \
            formatv("pass '{0}' requires TargetMachine", Name).str(),          \
            inconvertibleErrorCode());                                         \
    }                                                                          \
    MPM.addPass(createModuleToFunctionPassAdaptor(CREATE_PASS));               \
    return Error::success();                                                   \
  }
#define FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)    \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    auto CreatePass = CREATE_PASS;                                             \
    if constexpr (std::is_constructible_v<                                     \
                      std::remove_reference_t<decltype(CreatePass(             \
                          Params.get()))>,                                     \
                      const TargetMachine &,                                   \
                      std::remove_reference_t<decltype(Params.get())>>) {      \
      if (!TM) {                                                               \
        return make_error<StringError>(                                        \
            formatv("pass '{0}' requires TargetMachine", Name).str(),          \
            inconvertibleErrorCode());                                         \
      }                                                                        \
    }                                                                          \
    MPM.addPass(createModuleToFunctionPassAdaptor(CREATE_PASS(Params.get()))); \
    return Error::success();                                                   \
  }
#define LOOPNEST_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME) {                                                          \
    MPM.addPass(createModuleToFunctionPassAdaptor(                             \
        createFunctionToLoopPassAdaptor(CREATE_PASS, false)));                 \
    return Error::success();                                                   \
  }
#define LOOP_PASS(NAME, CREATE_PASS)                                           \
  if (Name == NAME) {                                                          \
    MPM.addPass(createModuleToFunctionPassAdaptor(                             \
        createFunctionToLoopPassAdaptor(CREATE_PASS, false)));                 \
    return Error::success();                                                   \
  }
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)        \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    MPM.addPass(createModuleToFunctionPassAdaptor(                             \
        createFunctionToLoopPassAdaptor(CREATE_PASS(Params.get()), false)));   \
    return Error::success();                                                   \
  }
#include "PassRegistry.def"

  for (auto &C : ModulePipelineParsingCallbacks)
    if (C(Name, MPM, InnerPipeline))
      return Error::success();
  return make_error<StringError>(
      formatv("unknown module pass '{}'", Name).str(),
      inconvertibleErrorCode());
}

Error PassBuilder::parseCGSCCPass(CGSCCPassManager &CGPM,
                                  const PipelineElement &E) {
  auto &Name = E.Name;
  auto &InnerPipeline = E.InnerPipeline;

  // First handle complex passes like the pass managers which carry pipelines.
  if (!InnerPipeline.empty()) {
    if (Name == "cgscc") {
      CGSCCPassManager NestedCGPM;
      if (auto Err = parseCGSCCPassPipeline(NestedCGPM, InnerPipeline))
        return Err;
      // Add the nested pass manager with the appropriate adaptor.
      CGPM.addPass(std::move(NestedCGPM));
      return Error::success();
    }
    if (auto Params = parseFunctionPipelineName(Name)) {
      FunctionPassManager FPM;
      if (auto Err = parseFunctionPassPipeline(FPM, InnerPipeline))
        return Err;
      // Add the nested pass manager with the appropriate adaptor.
      CGPM.addPass(createCGSCCToFunctionPassAdaptor(
          std::move(FPM), Params->first, Params->second));
      return Error::success();
    }
    if (auto MaxRepetitions = parseDevirtPassName(Name)) {
      CGSCCPassManager NestedCGPM;
      if (auto Err = parseCGSCCPassPipeline(NestedCGPM, InnerPipeline))
        return Err;
      CGPM.addPass(
          createDevirtSCCRepeatedPass(std::move(NestedCGPM), *MaxRepetitions));
      return Error::success();
    }

    for (auto &C : CGSCCPipelineParsingCallbacks)
      if (C(Name, CGPM, InnerPipeline))
        return Error::success();

    // Normal passes can't have pipelines.
    return make_error<StringError>(
        formatv("invalid use of '{}' pass as cgscc pipeline", Name).str(),
        inconvertibleErrorCode());
  }

// Now expand the basic registered passes from the .inc file.
#define CGSCC_PASS(NAME, CREATE_PASS)                                          \
  if (Name == NAME) {                                                          \
    CGPM.addPass(CREATE_PASS);                                                 \
    return Error::success();                                                   \
  }
#define CGSCC_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)       \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    CGPM.addPass(CREATE_PASS(Params.get()));                                   \
    return Error::success();                                                   \
  }
#define CGSCC_ANALYSIS(NAME, CREATE_PASS)                                      \
  if (Name == "require<" NAME ">") {                                           \
    CGPM.addPass(RequireAnalysisPass<                                          \
                 std::remove_reference_t<decltype(CREATE_PASS)>,               \
                 LazyCallGraph::SCC, CGSCCAnalysisManager, LazyCallGraph &,    \
                 CGSCCUpdateResult &>());                                      \
    return Error::success();                                                   \
  }                                                                            \
  if (Name == "invalidate<" NAME ">") {                                        \
    CGPM.addPass(InvalidateAnalysisPass<                                       \
                 std::remove_reference_t<decltype(CREATE_PASS)>>());           \
    return Error::success();                                                   \
  }
#define FUNCTION_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME) {                                                          \
    if constexpr (std::is_constructible_v<                                     \
                      std::remove_reference_t<decltype(CREATE_PASS)>,          \
                      const TargetMachine &>) {                                \
      if (!TM)                                                                 \
        return make_error<StringError>(                                        \
            formatv("pass '{0}' requires TargetMachine", Name).str(),          \
            inconvertibleErrorCode());                                         \
    }                                                                          \
    CGPM.addPass(createCGSCCToFunctionPassAdaptor(CREATE_PASS));               \
    return Error::success();                                                   \
  }
#define FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)    \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    auto CreatePass = CREATE_PASS;                                             \
    if constexpr (std::is_constructible_v<                                     \
                      std::remove_reference_t<decltype(CreatePass(             \
                          Params.get()))>,                                     \
                      const TargetMachine &,                                   \
                      std::remove_reference_t<decltype(Params.get())>>) {      \
      if (!TM) {                                                               \
        return make_error<StringError>(                                        \
            formatv("pass '{0}' requires TargetMachine", Name).str(),          \
            inconvertibleErrorCode());                                         \
      }                                                                        \
    }                                                                          \
    CGPM.addPass(createCGSCCToFunctionPassAdaptor(CREATE_PASS(Params.get()))); \
    return Error::success();                                                   \
  }
#define LOOPNEST_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME) {                                                          \
    CGPM.addPass(createCGSCCToFunctionPassAdaptor(                             \
        createFunctionToLoopPassAdaptor(CREATE_PASS, false)));                 \
    return Error::success();                                                   \
  }
#define LOOP_PASS(NAME, CREATE_PASS)                                           \
  if (Name == NAME) {                                                          \
    CGPM.addPass(createCGSCCToFunctionPassAdaptor(                             \
        createFunctionToLoopPassAdaptor(CREATE_PASS, false)));                 \
    return Error::success();                                                   \
  }
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)        \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    CGPM.addPass(createCGSCCToFunctionPassAdaptor(                             \
        createFunctionToLoopPassAdaptor(CREATE_PASS(Params.get()), false)));   \
    return Error::success();                                                   \
  }
#include "PassRegistry.def"

  for (auto &C : CGSCCPipelineParsingCallbacks)
    if (C(Name, CGPM, InnerPipeline))
      return Error::success();
  return make_error<StringError>(formatv("unknown cgscc pass '{}'", Name).str(),
                                 inconvertibleErrorCode());
}

Error PassBuilder::parseFunctionPass(FunctionPassManager &FPM,
                                     const PipelineElement &E) {
  auto &Name = E.Name;
  auto &InnerPipeline = E.InnerPipeline;

  // First handle complex passes like the pass managers which carry pipelines.
  if (!InnerPipeline.empty()) {
    if (Name == "function") {
      FunctionPassManager NestedFPM;
      if (auto Err = parseFunctionPassPipeline(NestedFPM, InnerPipeline))
        return Err;
      // Add the nested pass manager with the appropriate adaptor.
      FPM.addPass(std::move(NestedFPM));
      return Error::success();
    }
    if (Name == "loop" || Name == "loop-mssa") {
      LoopPassManager LPM;
      if (auto Err = parseLoopPassPipeline(LPM, InnerPipeline))
        return Err;
      // Add the nested pass manager with the appropriate adaptor.
      bool UseMemorySSA = (Name == "loop-mssa");
      FPM.addPass(
          createFunctionToLoopPassAdaptor(std::move(LPM), UseMemorySSA));
      return Error::success();
    }
    if (Name == "machine-function") {
      MachineFunctionPassManager MFPM;
      if (auto Err = parseMachinePassPipeline(MFPM, InnerPipeline))
        return Err;
      FPM.addPass(createFunctionToMachineFunctionPassAdaptor(std::move(MFPM)));
      return Error::success();
    }

    for (auto &C : FunctionPipelineParsingCallbacks)
      if (C(Name, FPM, InnerPipeline))
        return Error::success();

    // Normal passes can't have pipelines.
    return make_error<StringError>(
        formatv("invalid use of '{}' pass as function pipeline", Name).str(),
        inconvertibleErrorCode());
  }

// Now expand the basic registered passes from the .inc file.
#define FUNCTION_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME) {                                                          \
    if constexpr (std::is_constructible_v<                                     \
                      std::remove_reference_t<decltype(CREATE_PASS)>,          \
                      const TargetMachine &>) {                                \
      if (!TM)                                                                 \
        return make_error<StringError>(                                        \
            formatv("pass '{0}' requires TargetMachine", Name).str(),          \
            inconvertibleErrorCode());                                         \
    }                                                                          \
    FPM.addPass(CREATE_PASS);                                                  \
    return Error::success();                                                   \
  }
#define FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)    \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    auto CreatePass = CREATE_PASS;                                             \
    if constexpr (std::is_constructible_v<                                     \
                      std::remove_reference_t<decltype(CreatePass(             \
                          Params.get()))>,                                     \
                      const TargetMachine &,                                   \
                      std::remove_reference_t<decltype(Params.get())>>) {      \
      if (!TM) {                                                               \
        return make_error<StringError>(                                        \
            formatv("pass '{0}' requires TargetMachine", Name).str(),          \
            inconvertibleErrorCode());                                         \
      }                                                                        \
    }                                                                          \
    FPM.addPass(CREATE_PASS(Params.get()));                                    \
    return Error::success();                                                   \
  }
#define FUNCTION_ANALYSIS(NAME, CREATE_PASS)                                   \
  if (Name == "require<" NAME ">") {                                           \
    if constexpr (std::is_constructible_v<                                     \
                      std::remove_reference_t<decltype(CREATE_PASS)>,          \
                      const TargetMachine &>) {                                \
      if (!TM)                                                                 \
        return make_error<StringError>(                                        \
            formatv("pass '{0}' requires TargetMachine", Name).str(),          \
            inconvertibleErrorCode());                                         \
    }                                                                          \
    FPM.addPass(                                                               \
        RequireAnalysisPass<std::remove_reference_t<decltype(CREATE_PASS)>,    \
                            Function>());                                      \
    return Error::success();                                                   \
  }                                                                            \
  if (Name == "invalidate<" NAME ">") {                                        \
    FPM.addPass(InvalidateAnalysisPass<                                        \
                std::remove_reference_t<decltype(CREATE_PASS)>>());            \
    return Error::success();                                                   \
  }
// FIXME: UseMemorySSA is set to false. Maybe we could do things like:
//        bool UseMemorySSA = !("canon-freeze" || "loop-predication" ||
//                              "guard-widening");
//        The risk is that it may become obsolete if we're not careful.
#define LOOPNEST_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME) {                                                          \
    FPM.addPass(createFunctionToLoopPassAdaptor(CREATE_PASS, false));          \
    return Error::success();                                                   \
  }
#define LOOP_PASS(NAME, CREATE_PASS)                                           \
  if (Name == NAME) {                                                          \
    FPM.addPass(createFunctionToLoopPassAdaptor(CREATE_PASS, false));          \
    return Error::success();                                                   \
  }
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)        \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    FPM.addPass(                                                               \
        createFunctionToLoopPassAdaptor(CREATE_PASS(Params.get()), false));    \
    return Error::success();                                                   \
  }
#include "PassRegistry.def"

  for (auto &C : FunctionPipelineParsingCallbacks)
    if (C(Name, FPM, InnerPipeline))
      return Error::success();
  return make_error<StringError>(
      formatv("unknown function pass '{}'", Name).str(),
      inconvertibleErrorCode());
}

Error PassBuilder::parseLoopPass(LoopPassManager &LPM,
                                 const PipelineElement &E) {
  StringRef Name = E.Name;
  auto &InnerPipeline = E.InnerPipeline;

  // First handle complex passes like the pass managers which carry pipelines.
  if (!InnerPipeline.empty()) {
    if (Name == "loop") {
      LoopPassManager NestedLPM;
      if (auto Err = parseLoopPassPipeline(NestedLPM, InnerPipeline))
        return Err;
      // Add the nested pass manager with the appropriate adaptor.
      LPM.addPass(std::move(NestedLPM));
      return Error::success();
    }

    for (auto &C : LoopPipelineParsingCallbacks)
      if (C(Name, LPM, InnerPipeline))
        return Error::success();

    // Normal passes can't have pipelines.
    return make_error<StringError>(
        formatv("invalid use of '{}' pass as loop pipeline", Name).str(),
        inconvertibleErrorCode());
  }

// Now expand the basic registered passes from the .inc file.
#define LOOPNEST_PASS(NAME, CREATE_PASS)                                       \
  if (Name == NAME) {                                                          \
    LPM.addPass(CREATE_PASS);                                                  \
    return Error::success();                                                   \
  }
#define LOOP_PASS(NAME, CREATE_PASS)                                           \
  if (Name == NAME) {                                                          \
    LPM.addPass(CREATE_PASS);                                                  \
    return Error::success();                                                   \
  }
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)        \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    LPM.addPass(CREATE_PASS(Params.get()));                                    \
    return Error::success();                                                   \
  }
#define LOOP_ANALYSIS(NAME, CREATE_PASS)                                       \
  if (Name == "require<" NAME ">") {                                           \
    LPM.addPass(RequireAnalysisPass<                                           \
                std::remove_reference_t<decltype(CREATE_PASS)>, Loop,          \
                LoopAnalysisManager, LoopStandardAnalysisResults &,            \
                LPMUpdater &>());                                              \
    return Error::success();                                                   \
  }                                                                            \
  if (Name == "invalidate<" NAME ">") {                                        \
    LPM.addPass(InvalidateAnalysisPass<                                        \
                std::remove_reference_t<decltype(CREATE_PASS)>>());            \
    return Error::success();                                                   \
  }
#include "PassRegistry.def"

  for (auto &C : LoopPipelineParsingCallbacks)
    if (C(Name, LPM, InnerPipeline))
      return Error::success();
  return make_error<StringError>(formatv("unknown loop pass '{}'", Name).str(),
                                 inconvertibleErrorCode());
}

Error PassBuilder::parseMachinePass(MachineFunctionPassManager &MFPM,
                                    const PipelineElement &E) {
  StringRef Name = E.Name;
  // Handle any nested pass managers.
  if (!E.InnerPipeline.empty()) {
    if (E.Name == "machine-function") {
      MachineFunctionPassManager NestedPM;
      if (auto Err = parseMachinePassPipeline(NestedPM, E.InnerPipeline))
        return Err;
      MFPM.addPass(std::move(NestedPM));
      return Error::success();
    }
    return make_error<StringError>("invalid pipeline",
                                   inconvertibleErrorCode());
  }

#define MACHINE_MODULE_PASS(NAME, CREATE_PASS)                                 \
  if (Name == NAME) {                                                          \
    MFPM.addPass(CREATE_PASS);                                                 \
    return Error::success();                                                   \
  }
#define MACHINE_FUNCTION_PASS(NAME, CREATE_PASS)                               \
  if (Name == NAME) {                                                          \
    MFPM.addPass(CREATE_PASS);                                                 \
    return Error::success();                                                   \
  }
#define MACHINE_FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER,    \
                                          PARAMS)                              \
  if (checkParametrizedPassName(Name, NAME)) {                                 \
    auto Params = parsePassParameters(PARSER, Name, NAME);                     \
    if (!Params)                                                               \
      return Params.takeError();                                               \
    MFPM.addPass(CREATE_PASS(Params.get()));                                   \
    return Error::success();                                                   \
  }
#define MACHINE_FUNCTION_ANALYSIS(NAME, CREATE_PASS)                           \
  if (Name == "require<" NAME ">") {                                           \
    MFPM.addPass(                                                              \
        RequireAnalysisPass<std::remove_reference_t<decltype(CREATE_PASS)>,    \
                            MachineFunction>());                               \
    return Error::success();                                                   \
  }                                                                            \
  if (Name == "invalidate<" NAME ">") {                                        \
    MFPM.addPass(InvalidateAnalysisPass<                                       \
                 std::remove_reference_t<decltype(CREATE_PASS)>>());           \
    return Error::success();                                                   \
  }
#include "core/passes/MachinePassRegistry.def"

  for (auto &C : MachineFunctionPipelineParsingCallbacks)
    if (C(Name, MFPM, E.InnerPipeline))
      return Error::success();
  return make_error<StringError>(
      formatv("unknown machine pass '{}'", Name).str(),
      inconvertibleErrorCode());
}

bool PassBuilder::parseAAPassName(AAManager &AA, StringRef Name) {
#define MODULE_ALIAS_ANALYSIS(NAME, CREATE_PASS)                               \
  if (Name == NAME) {                                                          \
    AA.registerModuleAnalysis<                                                 \
        std::remove_reference_t<decltype(CREATE_PASS)>>();                     \
    return true;                                                               \
  }
#define FUNCTION_ALIAS_ANALYSIS(NAME, CREATE_PASS)                             \
  if (Name == NAME) {                                                          \
    AA.registerFunctionAnalysis<                                               \
        std::remove_reference_t<decltype(CREATE_PASS)>>();                     \
    return true;                                                               \
  }
#include "PassRegistry.def"

  for (auto &C : AAParsingCallbacks)
    if (C(Name, AA))
      return true;
  return false;
}

Error PassBuilder::parseMachinePassPipeline(
    MachineFunctionPassManager &MFPM, ArrayRef<PipelineElement> Pipeline) {
  for (const auto &Element : Pipeline) {
    if (auto Err = parseMachinePass(MFPM, Element))
      return Err;
  }
  return Error::success();
}

Error PassBuilder::parseLoopPassPipeline(LoopPassManager &LPM,
                                         ArrayRef<PipelineElement> Pipeline) {
  for (const auto &Element : Pipeline) {
    if (auto Err = parseLoopPass(LPM, Element))
      return Err;
  }
  return Error::success();
}

Error PassBuilder::parseFunctionPassPipeline(
    FunctionPassManager &FPM, ArrayRef<PipelineElement> Pipeline) {
  for (const auto &Element : Pipeline) {
    if (auto Err = parseFunctionPass(FPM, Element))
      return Err;
  }
  return Error::success();
}

Error PassBuilder::parseCGSCCPassPipeline(CGSCCPassManager &CGPM,
                                          ArrayRef<PipelineElement> Pipeline) {
  for (const auto &Element : Pipeline) {
    if (auto Err = parseCGSCCPass(CGPM, Element))
      return Err;
  }
  return Error::success();
}

void PassBuilder::crossRegisterProxies(LoopAnalysisManager &LAM,
                                       FunctionAnalysisManager &FAM,
                                       CGSCCAnalysisManager &CGAM,
                                       ModuleAnalysisManager &MAM,
                                       MachineFunctionAnalysisManager *MFAM) {
  MAM.registerPass([&] { return FunctionAnalysisManagerModuleProxy(FAM); });
  MAM.registerPass([&] { return CGSCCAnalysisManagerModuleProxy(CGAM); });
  CGAM.registerPass([&] { return ModuleAnalysisManagerCGSCCProxy(MAM); });
  FAM.registerPass([&] { return CGSCCAnalysisManagerFunctionProxy(CGAM); });
  FAM.registerPass([&] { return ModuleAnalysisManagerFunctionProxy(MAM); });
  FAM.registerPass([&] { return LoopAnalysisManagerFunctionProxy(LAM); });
  LAM.registerPass([&] { return FunctionAnalysisManagerLoopProxy(FAM); });
  if (MFAM) {
    MAM.registerPass(
        [&] { return MachineFunctionAnalysisManagerModuleProxy(*MFAM); });
    FAM.registerPass(
        [&] { return MachineFunctionAnalysisManagerFunctionProxy(*MFAM); });
    MFAM->registerPass(
        [&] { return ModuleAnalysisManagerMachineFunctionProxy(MAM); });
    MFAM->registerPass(
        [&] { return FunctionAnalysisManagerMachineFunctionProxy(FAM); });
  }
}

Error PassBuilder::parseModulePassPipeline(ModulePassManager &MPM,
                                           ArrayRef<PipelineElement> Pipeline) {
  for (const auto &Element : Pipeline) {
    if (auto Err = parseModulePass(MPM, Element))
      return Err;
  }
  return Error::success();
}

// Primary pass pipeline description parsing routine for a \c ModulePassManager
// FIXME: Should this routine accept a TargetMachine or require the caller to
// pre-populate the analysis managers with target-specific stuff?
Error PassBuilder::parsePassPipeline(ModulePassManager &MPM,
                                     StringRef PipelineText) {
  auto Pipeline = parsePipelineText(PipelineText);
  if (!Pipeline || Pipeline->empty())
    return make_error<StringError>(
        formatv("invalid pipeline '{}'", PipelineText).str(),
        inconvertibleErrorCode());

  // If the first name isn't at the module layer, wrap the pipeline up
  // automatically.
  StringRef FirstName = Pipeline->front().Name;

  if (!isModulePassName(FirstName, ModulePipelineParsingCallbacks)) {
    bool UseMemorySSA;
    if (isCGSCCPassName(FirstName, CGSCCPipelineParsingCallbacks)) {
      Pipeline = {{"cgscc", std::move(*Pipeline)}};
    } else if (isFunctionPassName(FirstName,
                                  FunctionPipelineParsingCallbacks)) {
      Pipeline = {{"function", std::move(*Pipeline)}};
    } else if (isLoopNestPassName(FirstName, LoopPipelineParsingCallbacks,
                                  UseMemorySSA)) {
      Pipeline = {{"function", {{UseMemorySSA ? "loop-mssa" : "loop",
                                 std::move(*Pipeline)}}}};
    } else if (isLoopPassName(FirstName, LoopPipelineParsingCallbacks,
                              UseMemorySSA)) {
      Pipeline = {{"function", {{UseMemorySSA ? "loop-mssa" : "loop",
                                 std::move(*Pipeline)}}}};
    } else if (isMachineFunctionPassName(
                   FirstName, MachineFunctionPipelineParsingCallbacks)) {
      Pipeline = {{"function", {{"machine-function", std::move(*Pipeline)}}}};
    } else {
      for (auto &C : TopLevelPipelineParsingCallbacks)
        if (C(MPM, *Pipeline))
          return Error::success();

      // Unknown pass or pipeline name!
      auto &InnerPipeline = Pipeline->front().InnerPipeline;
      return make_error<StringError>(
          formatv("unknown {} name '{}'",
                  (InnerPipeline.empty() ? "pass" : "pipeline"), FirstName)
              .str(),
          inconvertibleErrorCode());
    }
  }

  if (auto Err = parseModulePassPipeline(MPM, *Pipeline))
    return Err;
  return Error::success();
}

// Primary pass pipeline description parsing routine for a \c CGSCCPassManager
Error PassBuilder::parsePassPipeline(CGSCCPassManager &CGPM,
                                     StringRef PipelineText) {
  auto Pipeline = parsePipelineText(PipelineText);
  if (!Pipeline || Pipeline->empty())
    return make_error<StringError>(
        formatv("invalid pipeline '{}'", PipelineText).str(),
        inconvertibleErrorCode());

  StringRef FirstName = Pipeline->front().Name;
  if (!isCGSCCPassName(FirstName, CGSCCPipelineParsingCallbacks))
    return make_error<StringError>(
        formatv("unknown cgscc pass '{}' in pipeline '{}'", FirstName,
                PipelineText)
            .str(),
        inconvertibleErrorCode());

  if (auto Err = parseCGSCCPassPipeline(CGPM, *Pipeline))
    return Err;
  return Error::success();
}

// Primary pass pipeline description parsing routine for a \c
// FunctionPassManager
Error PassBuilder::parsePassPipeline(FunctionPassManager &FPM,
                                     StringRef PipelineText) {
  auto Pipeline = parsePipelineText(PipelineText);
  if (!Pipeline || Pipeline->empty())
    return make_error<StringError>(
        formatv("invalid pipeline '{}'", PipelineText).str(),
        inconvertibleErrorCode());

  StringRef FirstName = Pipeline->front().Name;
  if (!isFunctionPassName(FirstName, FunctionPipelineParsingCallbacks))
    return make_error<StringError>(
        formatv("unknown function pass '{}' in pipeline '{}'", FirstName,
                PipelineText)
            .str(),
        inconvertibleErrorCode());

  if (auto Err = parseFunctionPassPipeline(FPM, *Pipeline))
    return Err;
  return Error::success();
}

// Primary pass pipeline description parsing routine for a \c LoopPassManager
Error PassBuilder::parsePassPipeline(LoopPassManager &CGPM,
                                     StringRef PipelineText) {
  auto Pipeline = parsePipelineText(PipelineText);
  if (!Pipeline || Pipeline->empty())
    return make_error<StringError>(
        formatv("invalid pipeline '{}'", PipelineText).str(),
        inconvertibleErrorCode());

  if (auto Err = parseLoopPassPipeline(CGPM, *Pipeline))
    return Err;

  return Error::success();
}

Error PassBuilder::parsePassPipeline(MachineFunctionPassManager &MFPM,
                                     StringRef PipelineText) {
  auto Pipeline = parsePipelineText(PipelineText);
  if (!Pipeline || Pipeline->empty())
    return make_error<StringError>(
        formatv("invalid machine pass pipeline '{}'", PipelineText).str(),
        inconvertibleErrorCode());

  if (auto Err = parseMachinePassPipeline(MFPM, *Pipeline))
    return Err;

  return Error::success();
}

Error PassBuilder::parseAAPipeline(AAManager &AA, StringRef PipelineText) {
  // If the pipeline just consists of the word 'default' just replace the AA
  // manager with our default one.
  if (PipelineText == "default") {
    AA = buildDefaultAAPipeline();
    return Error::success();
  }

  while (!PipelineText.empty()) {
    StringRef Name;
    std::tie(Name, PipelineText) = PipelineText.split(',');
    if (!parseAAPassName(AA, Name))
      return make_error<StringError>(
          formatv("unknown alias analysis name '{}'", Name).str(),
          inconvertibleErrorCode());
  }

  return Error::success();
}

std::optional<RegAllocFilterFunc>
PassBuilder::parseRegAllocFilter(StringRef FilterName) {
  if (FilterName == "all")
    return nullptr;
  for (auto &C : RegClassFilterParsingCallbacks)
    if (auto F = C(FilterName))
      return F;
  return std::nullopt;
}

LLVM_ATTRIBUTE_NOINLINE static void printPassNameList(StringTable PassNames,
                                                      raw_ostream &OS) {
  for (StringRef PassName : drop_begin(PassNames))
    OS << "  " << PassName << '\n';
}

LLVM_ATTRIBUTE_NOINLINE static void
printPassNameListWithParams(StringTable PassNames, raw_ostream &OS) {
  auto I = PassNames.begin();
  auto End = PassNames.end();
  ++I;
  while (I != End) {
    StringRef Name = *I;
    ++I;
    assert(I != End);
    StringRef Params = *I;
    ++I;
    OS << "  " << Name << '<' << Params << ">\n";
  }
}

void PassBuilder::printPassNames(raw_ostream &OS) {
  // TODO: print pass descriptions when they are available

  OS << "Module passes:\n";
  static constexpr char ModulePassNames[] = {"\0"
#define MODULE_PASS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(ModulePassNames), OS);

  OS << "Module passes with params:\n";
  static constexpr char ModulePassNamesWithParams[] = {"\0"
#define MODULE_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)      \
  NAME "\0" PARAMS "\0"
#include "PassRegistry.def"
  };
  printPassNameListWithParams(StringTable(ModulePassNamesWithParams), OS);

  OS << "Module analyses:\n";
  static constexpr char ModuleAnalysisNames[] = {"\0"
#define MODULE_ANALYSIS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(ModuleAnalysisNames), OS);

  OS << "Module alias analyses:\n";
  static constexpr char ModuleAliasAnalysisNames[] = {"\0"
#define MODULE_ALIAS_ANALYSIS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(ModuleAliasAnalysisNames), OS);

  OS << "CGSCC passes:\n";
  static constexpr char CGSCCPassNames[] = {"\0"
#define CGSCC_PASS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(CGSCCPassNames), OS);

  OS << "CGSCC passes with params:\n";
  static constexpr char CGSCCPassNamesWithParams[] = {"\0"
#define CGSCC_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)       \
  NAME "\0" PARAMS "\0"
#include "PassRegistry.def"
  };
  printPassNameListWithParams(StringTable(CGSCCPassNamesWithParams), OS);

  OS << "CGSCC analyses:\n";
  static constexpr char CGSCCAnalysisNames[] = {"\0"
#define CGSCC_ANALYSIS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(CGSCCAnalysisNames), OS);

  OS << "Function passes:\n";
  static constexpr char FunctionPassNames[] = {"\0"
#define FUNCTION_PASS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(FunctionPassNames), OS);

  OS << "Function passes with params:\n";
  static constexpr char FunctionPassNamesWithParams[] = {"\0"
#define FUNCTION_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)    \
  NAME "\0" PARAMS "\0"
#include "PassRegistry.def"
  };
  printPassNameListWithParams(StringTable(FunctionPassNamesWithParams), OS);

  OS << "Function analyses:\n";
  static constexpr char FunctionAnalysisNames[] = {"\0"
#define FUNCTION_ANALYSIS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(FunctionAnalysisNames), OS);

  OS << "Function alias analyses:\n";
  static constexpr char FunctionAliasAnalysisNames[] = {"\0"
#define FUNCTION_ALIAS_ANALYSIS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(FunctionAliasAnalysisNames), OS);

  OS << "LoopNest passes:\n";
  static constexpr char LoopNestPassNames[] = {"\0"
#define LOOPNEST_PASS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(LoopNestPassNames), OS);

  OS << "Loop passes:\n";
  static constexpr char LoopPassNames[] = {"\0"
#define LOOP_PASS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(LoopPassNames), OS);

  OS << "Loop passes with params:\n";
  static constexpr char LoopPassNamesWithParams[] = {"\0"
#define LOOP_PASS_WITH_PARAMS(NAME, CLASS, CREATE_PASS, PARSER, PARAMS)        \
  NAME "\0" PARAMS "\0"
#include "PassRegistry.def"
  };
  printPassNameListWithParams(StringTable(LoopPassNamesWithParams), OS);

  OS << "Loop analyses:\n";
  static constexpr char LoopAnalysisNames[] = {"\0"
#define LOOP_ANALYSIS(NAME, CREATE_PASS) NAME "\0"
#include "PassRegistry.def"
  };
  printPassNameList(StringTable(LoopAnalysisNames), OS);

  OS << "Machine module passes (WIP):\n";
  static constexpr char MachineModulePassNames[] = {"\0"
#define MACHINE_MODULE_PASS(NAME, CREATE_PASS) NAME "\0"
#include "core/passes/MachinePassRegistry.def"
  };
  printPassNameList(StringTable(MachineModulePassNames), OS);

  OS << "Machine function passes (WIP):\n";
  static constexpr char MachineFunctionPassNames[] = {"\0"
#define MACHINE_FUNCTION_PASS(NAME, CREATE_PASS) NAME "\0"
#include "core/passes/MachinePassRegistry.def"
  };
  printPassNameList(StringTable(MachineFunctionPassNames), OS);

  OS << "Machine function analyses (WIP):\n";
  static constexpr char MachineFunctionAnalysisNames[] = {"\0"
#define MACHINE_FUNCTION_ANALYSIS(NAME, CREATE_PASS) NAME "\0"
#include "core/passes/MachinePassRegistry.def"
  };
  printPassNameList(StringTable(MachineFunctionAnalysisNames), OS);
}

void PassBuilder::registerParseTopLevelPipelineCallback(
    const std::function<bool(ModulePassManager &, ArrayRef<PipelineElement>)>
        &C) {
  TopLevelPipelineParsingCallbacks.push_back(C);
}
