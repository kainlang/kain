//===- RegionsFromMetadata.cpp - A helper to test RegionPasses -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/passes/Vectorize/SandboxVectorizer/Passes/RegionsFromMetadata.h"

#include "core/passes/Vectorize/SandboxVectorizer/RegionWithScore.h"
#include "core/passes/Vectorize/SandboxVectorizer/SandboxVectorizerPassBuilder.h"

namespace llvm::sandboxir {

RegionsFromMetadata::RegionsFromMetadata(StringRef Pipeline, StringRef AuxArg)
    : FunctionPass("regions-from-metadata"),
      RPM("rpm", Pipeline, SandboxVectorizerPassBuilder::createRegionPass) {}

bool RegionsFromMetadata::runOnFunction(Function &F, const Analyses &A) {
  SmallVector<std::unique_ptr<sandboxir::RegionWithScore>> Regions =
      sandboxir::RegionWithScore::createRegionsFromMD(F, A.getTTI());
  bool Change = false;
  for (auto &R : Regions) {
    Change |= RPM.runOnRegion(*R, A);
  }
  return Change;
}

} // namespace llvm::sandboxir
