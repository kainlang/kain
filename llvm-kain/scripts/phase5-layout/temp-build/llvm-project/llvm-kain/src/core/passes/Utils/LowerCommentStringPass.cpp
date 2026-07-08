//===-- LowerCommentStringPass.cpp - Lower Comment string metadata -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//
// This pass processes copyright comment strings created by Clang for
// #pragma comment(copyright, ...) implementation.
//
// Clang CodeGen creates weak_odr hidden constant string globals marked with
// !loadtime_comment metadata and adds them to llvm.compiler.used. These globals
// are placed in the __loadtime_comment section for better memory layout.
//
// This pass attaches !implicit.ref metadata from every defined function to
// each copyright string global. The PowerPC AIX backend recognizes this
// metadata and emits a .ref directive, creating a relocation that prevents
// the linker from discarding the string as long as the function is kept.
//
// This pass is currently enabled for AIX targets only.
//
// Input IR (created by Clang):
//   @__loadtime_comment_str_HASH = weak_odr hidden unnamed_addr constant
//     [N x i8] c"Copyright\00", section "__loadtime_comment", align 1,
//     !loadtime_comment !0
//   @llvm.compiler.used = appending global [1 x ptr]
//     [ptr @__loadtime_comment_str_HASH], section "llvm.metadata"
//
//  Output IR:
//   @__loadtime_comment_str_HASH = weak_odr hidden unnamed_addr constant
//     [N x i8] c"Copyright\00", section "__loadtime_comment", align 1,
//     !loadtime_comment !0
//   @llvm.compiler.used = appending global [1 x ptr]
//     [ptr @__loadtime_comment_str_HASH], section "llvm.metadata"
//
//     define i32 @func() !implicit.ref !1 { ... }
//     !1 = !{ptr @__loadtime_comment_str_HASH}
//
//===----------------------------------------------------------------------===//

#include "core/passes/Utils/LowerCommentStringPass.h"

#include "support/adt/SmallVector.h"
#include "support/adt/StringRef.h"
#include "core/ir/Attributes.h"
#include "core/ir/DerivedTypes.h"
#include "core/ir/Function.h"
#include "core/ir/GlobalValue.h"
#include "core/ir/GlobalVariable.h"
#include "core/ir/MDBuilder.h"
#include "core/ir/Metadata.h"
#include "core/ir/Module.h"
#include "core/ir/Type.h"
#include "core/ir/Value.h"
#include "support/adt/CommandLine.h"
#include "support/adt/Debug.h"
#include "support/target/Triple.h"
#include "core/passes/Utils/ModuleUtils.h"

#define DEBUG_TYPE "lower-comment-string"

using namespace llvm;

static cl::opt<bool>
    DisableCopyrightMetadata("disable-lower-comment-string", cl::ReallyHidden,
                             cl::desc("Disable LowerCommentString pass."),
                             cl::init(false));

static bool isSupportedTarget(const Module &M) {
  Triple T{M.getTargetTriple()};
  return T.isOSAIX();
}

PreservedAnalyses LowerCommentStringPass::run(Module &M,
                                              ModuleAnalysisManager &AM) {
  if (DisableCopyrightMetadata || !isSupportedTarget(M))
    return PreservedAnalyses::all();

  LLVMContext &Ctx = M.getContext();

  // Collect all globals marked with !loadtime_comment metadata.
  SmallVector<GlobalValue *, 4> LoadTimeCommentGlobals;
  for (GlobalVariable &GV : M.globals()) {
    if (GV.hasMetadata("loadtime_comment"))
      LoadTimeCommentGlobals.push_back(&GV);
  }

  if (LoadTimeCommentGlobals.empty())
    return PreservedAnalyses::all();

  // Add implicit.ref from every function to each loadtime comment global.
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    for (GlobalValue *GV : LoadTimeCommentGlobals) {
      Metadata *Ops[] = {ConstantAsMetadata::get(GV)};
      MDNode *NewMD = MDNode::get(Ctx, Ops);
      F.addMetadata(LLVMContext::MD_implicit_ref, *NewMD);

      LLVM_DEBUG(
          dbgs() << "[loadtime-comment] attached implicit.ref to function: "
                 << F.getName() << " for global: " << GV->getName() << "\n");
    }
  }

  LLVM_DEBUG(dbgs() << "[loadtime-comment] processed "
                    << LoadTimeCommentGlobals.size()
                    << " loadtime comment globals\n");

  return PreservedAnalyses::all();
}
