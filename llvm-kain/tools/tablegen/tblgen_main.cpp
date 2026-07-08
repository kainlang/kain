//===- tblgen_main.cpp - llvm-tblgen entry point -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "tools/tablegen/Main.h"
#include "tools/tablegen/Record.h"
#include "support/adt/CommandLine.h"
#include "support/adt/raw_ostream.h"

int main(int argc, char **argv) {
  // Parse all the cl::opt globals (InputFilename, OutputFilename, IncludeDirs, etc.)
  llvm::cl::ParseCommandLineOptions(argc, argv);

  // Pass an empty fallback lambda — ApplyCallback handles the real dispatch
  return llvm::TableGenMain(argv[0], [](llvm::raw_ostream &, const llvm::RecordKeeper &) {
    return false;
  });
}
