//===- NewPMDriver.cpp - Stub: falls back to legacy PM ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Minimal stub implementation that returns an error to fall back to legacy PM.
//
//===----------------------------------------------------------------------===//

#include "NewPMDriver.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Error.h"
#include "llvm/CodeGen/MIRParser/MIRParser.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

bool LLCDiagnosticHandler::handleDiagnostics(const DiagnosticInfo &DI) {
  return false;
}

int llvm::compileModuleWithNewPM(
    StringRef Arg0, std::unique_ptr<Module> M,
    std::unique_ptr<MIRParser> MIR,
    std::unique_ptr<TargetMachine> Target,
    std::unique_ptr<ToolOutputFile> Out,
    std::unique_ptr<ToolOutputFile> DwoOut, LLVMContext &Context,
    const TargetLibraryInfoImpl &TLII, VerifierKind VK,
    StringRef PassPipeline, CodeGenFileType FileType) {
  errs() << "NewPM not available in this build, use legacy PM\n";
  return 1;
}
