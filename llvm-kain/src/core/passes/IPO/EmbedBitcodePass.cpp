//===- EmbedBitcodePass.cpp - Pass that embeds the bitcode into a global---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/passes/IPO/EmbedBitcodePass.h"
#include "core/ir/BitcodeWriter.h"
#include "core/ir/BitcodeWriterPass.h"
#include "core/ir/PassManager.h"
#include "core/Pass.h"
#include "support/adt/ErrorHandling.h"
#include "support/adt/MemoryBufferRef.h"
#include "support/adt/raw_ostream.h"
#include "support/target/Triple.h"
#include "core/passes/IPO/ThinLTOBitcodeWriter.h"
#include "core/passes/Utils/ModuleUtils.h"

#include <string>

using namespace llvm;

PreservedAnalyses EmbedBitcodePass::run(Module &M, ModuleAnalysisManager &AM) {
  if (M.getGlobalVariable("llvm.embedded.module", /*AllowInternal=*/true))
    reportFatalUsageError("Can only embed the module once");

  Triple T(M.getTargetTriple());
  if (T.getObjectFormat() != Triple::ELF && T.getObjectFormat() != Triple::COFF)
    reportFatalUsageError("EmbedBitcode pass currently only supports COFF and "
                          "ELF object formats");

  std::string Data;
  raw_string_ostream OS(Data);
  if (IsThinLTO)
    ThinLTOBitcodeWriterPass(OS, /*ThinLinkOS=*/nullptr).run(M, AM);
  else
    BitcodeWriterPass(OS, /*ShouldPreserveUseListOrder=*/false, EmitLTOSummary)
        .run(M, AM);

  embedBufferInModule(M, MemoryBufferRef(Data, "ModuleData"), ".llvm.lto");

  return PreservedAnalyses::none();
}
