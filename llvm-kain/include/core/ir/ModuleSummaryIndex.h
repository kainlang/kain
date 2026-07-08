//===- ModuleSummaryIndex.h - Module Summary Index (minimal stub) --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// @file
/// This is a MINIMAL stub for the stripped LTO/ThinLTO subsystem.
/// Only provides forward declarations needed by StackSafetyAnalysis.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_MODULESUMMARYINDEX_H
#define LLVM_IR_MODULESUMMARYINDEX_H

#include "support/adt/ArrayRef.h"
#include "support/adt/StringRef.h"
#include <cstdint>
#include <vector>

namespace llvm {

class Function;
class GlobalValue;
class Module;

/// Minimal stub for FunctionSummary::ParamAccess needed by StackSafetyAnalysis.
struct FunctionSummary {
  struct ParamAccess {
    struct Range {
      int64_t Start = 0;
      int64_t End = 0;
    };
    unsigned ParamNo = 0;
    Range CallRange = {};
  };
};

/// Minimal GlobalValueSummary stub for AsmWriter getSummaryKindName/getImportTypeName.
class GlobalValueSummary {
public:
  enum SummaryKind { AliasKind, FunctionKind, GlobalVarKind };
  enum ImportKind { Definition, Declaration };
};

/// Forward declaration of ModuleSummaryIndex — full class is in the
/// _llvm_bak for reference but not needed in the stripped build.
class ModuleSummaryIndex {
public:
  ModuleSummaryIndex() = default;
  ~ModuleSummaryIndex() = default;

  // Provide enough interface to satisfy StackSafetyAnalysis.h references.
  // Full definition is 2092 lines — not needed without LTO.
  bool hasExportedFunctions() const { return false; }
  unsigned getModuleCount() const { return 0; }
  unsigned getBlockCount() const { return 0; }
};

} // end namespace llvm

#endif // LLVM_IR_MODULESUMMARYINDEX_H
