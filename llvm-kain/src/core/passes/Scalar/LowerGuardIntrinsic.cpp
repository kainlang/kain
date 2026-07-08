//===- LowerGuardIntrinsic.cpp - Lower the guard intrinsic ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass lowers the llvm.experimental.guard intrinsic to a conditional call
// to @llvm.experimental.deoptimize.  Once this happens, the guard can no longer
// be widened.
//
//===----------------------------------------------------------------------===//

#include "core/passes/Scalar/LowerGuardIntrinsic.h"
#include "support/adt/SmallVector.h"
#include "core/ir/Function.h"
#include "core/ir/Instructions.h"
#include "core/ir/Intrinsics.h"
#include "core/passes/Utils/GuardUtils.h"

using namespace llvm;

static bool lowerGuardIntrinsic(Function &F) {
  // Check if we can cheaply rule out the possibility of not having any work to
  // do.
  auto *GuardDecl = Intrinsic::getDeclarationIfExists(
      F.getParent(), Intrinsic::experimental_guard);
  if (!GuardDecl || GuardDecl->use_empty())
    return false;

  SmallVector<CallInst *, 8> ToLower;
  // Traverse through the users of GuardDecl.
  // This is presumably cheaper than traversing all instructions in the
  // function.
  for (auto *U : GuardDecl->users())
    if (auto *CI = dyn_cast<CallInst>(U))
      if (CI->getFunction() == &F)
        ToLower.push_back(CI);

  if (ToLower.empty())
    return false;

  auto *DeoptIntrinsic = Intrinsic::getOrInsertDeclaration(
      F.getParent(), Intrinsic::experimental_deoptimize, {F.getReturnType()});
  DeoptIntrinsic->setCallingConv(GuardDecl->getCallingConv());

  for (auto *CI : ToLower) {
    makeGuardControlFlowExplicit(DeoptIntrinsic, CI, false);
    CI->eraseFromParent();
  }

  return true;
}

PreservedAnalyses LowerGuardIntrinsicPass::run(Function &F,
                                               FunctionAnalysisManager &AM) {
  if (lowerGuardIntrinsic(F))
    return PreservedAnalyses::none();

  return PreservedAnalyses::all();
}
