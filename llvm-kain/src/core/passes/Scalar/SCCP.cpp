//===- SCCP.cpp - Sparse Conditional Constant Propagation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements sparse conditional constant propagation and merging:
//
// Specifically, this:
//   * Assumes values are constant unless proven otherwise
//   * Assumes BasicBlocks are dead unless proven otherwise
//   * Proves values to be constant, and replaces them with constants
//   * Proves conditional branches to be unconditional
//
//===----------------------------------------------------------------------===//

#include "core/passes/Scalar/SCCP.h"
#include "support/adt/SmallPtrSet.h"
#include "support/adt/SmallVector.h"
#include "support/adt/Statistic.h"
#include "core/analysis/AssumptionCache.h"
#include "core/analysis/DomTreeUpdater.h"
#include "core/analysis/GlobalsModRef.h"
#include "core/analysis/TargetLibraryInfo.h"
#include "core/analysis/ValueLatticeUtils.h"
#include "core/analysis/ValueTracking.h"
#include "core/ir/BasicBlock.h"
#include "core/ir/DerivedTypes.h"
#include "core/ir/Function.h"
#include "core/ir/InstrTypes.h"
#include "core/ir/Instruction.h"
#include "core/ir/Instructions.h"
#include "core/ir/IntrinsicInst.h"
#include "core/ir/PassManager.h"
#include "core/ir/Type.h"
#include "core/ir/Value.h"
#include "core/Pass.h"
#include "support/adt/Debug.h"
#include "support/adt/raw_ostream.h"
#include "core/passes/Scalar.h"
#include "core/passes/Utils/Local.h"
#include "core/passes/Utils/SCCPSolver.h"

using namespace llvm;

#define DEBUG_TYPE "sccp"

STATISTIC(NumInstRemoved, "Number of instructions removed");
STATISTIC(NumDeadBlocks , "Number of basic blocks unreachable");
STATISTIC(NumInstReplaced,
          "Number of instructions replaced with (simpler) instruction");

// runSCCP() - Run the Sparse Conditional Constant Propagation algorithm,
// and return true if the function was modified.
static bool runSCCP(Function &F, const DataLayout &DL,
                    const TargetLibraryInfo *TLI, DominatorTree &DT,
                    AssumptionCache &AC) {
  LLVM_DEBUG(dbgs() << "SCCP on function '" << F.getName() << "'\n");
  SCCPSolver Solver(
      DL, [TLI](Function &F) -> const TargetLibraryInfo & { return *TLI; },
      F.getContext());

  Solver.addPredicateInfo(F, DT, AC);

  // While we don't do any actual inter-procedural analysis, still track
  // return values so we can infer attributes.
  if (canTrackReturnsInterprocedurally(&F))
    Solver.addTrackedFunction(&F);

  // Mark the first block of the function as being executable.
  Solver.markBlockExecutable(&F.front());

  // Initialize arguments based on attributes.
  for (Argument &AI : F.args())
    Solver.trackValueOfArgument(&AI);

  // Solve for constants.
  bool ResolvedUndefs = true;
  while (ResolvedUndefs) {
    Solver.solve();
    LLVM_DEBUG(dbgs() << "RESOLVING UNDEFs\n");
    ResolvedUndefs = Solver.resolvedUndefsIn(F);
  }

  bool MadeChanges = false;

  // If we decided that there are basic blocks that are dead in this function,
  // delete their contents now.  Note that we cannot actually delete the blocks,
  // as we cannot modify the CFG of the function.

  SmallPtrSet<Value *, 32> InsertedValues;
  SmallVector<BasicBlock *, 8> BlocksToErase;
  for (BasicBlock &BB : F) {
    if (!Solver.isBlockExecutable(&BB)) {
      LLVM_DEBUG(dbgs() << "  BasicBlock Dead:" << BB);
      ++NumDeadBlocks;
      BlocksToErase.push_back(&BB);
      MadeChanges = true;
      continue;
    }

    MadeChanges |= Solver.simplifyInstsInBlock(BB, InsertedValues,
                                               NumInstRemoved, NumInstReplaced);
  }

  // Remove unreachable blocks and non-feasible edges.
  DomTreeUpdater DTU(DT, DomTreeUpdater::UpdateStrategy::Lazy);
  for (BasicBlock *DeadBB : BlocksToErase)
    NumInstRemoved += changeToUnreachable(&*DeadBB->getFirstNonPHIIt(),
                                          /*PreserveLCSSA=*/false, &DTU);

  BasicBlock *NewUnreachableBB = nullptr;
  for (BasicBlock &BB : F)
    MadeChanges |= Solver.removeNonFeasibleEdges(&BB, DTU, NewUnreachableBB);

  for (BasicBlock *DeadBB : BlocksToErase)
    if (!DeadBB->hasAddressTaken())
      DTU.deleteBB(DeadBB);

  Solver.removeSSACopies(F);

  Solver.inferReturnAttributes();

  return MadeChanges;
}

PreservedAnalyses SCCPPass::run(Function &F, FunctionAnalysisManager &AM) {
  const DataLayout &DL = F.getDataLayout();
  auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  auto &AC = AM.getResult<AssumptionAnalysis>(F);
  if (!runSCCP(F, DL, &TLI, DT, AC))
    return PreservedAnalyses::all();

  auto PA = PreservedAnalyses();
  PA.preserve<DominatorTreeAnalysis>();
  return PA;
}
