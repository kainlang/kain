//===-- DebugSupport.h ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines functions which generate more readable forms of data
//  structures used in the dataflow analyses, for debugging purposes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_FLOWSENSITIVE_DEBUGSUPPORT_H_
#define LLVM_CLANG_ANALYSIS_FLOWSENSITIVE_DEBUGSUPPORT_H_

#include <string>
#include <vector>

#include "analysis/FlowSensitive/Solver.h"
#include "analysis/FlowSensitive/Value.h"
#include "support/adt/StringRef.h"

namespace clang {
namespace dataflow {

/// Returns a string representation of a value kind.
llvm::StringRef debugString(Value::Kind Kind);

/// Returns a string representation of the result status of a SAT check.
llvm::StringRef debugString(Solver::Result::Status Status);

} // namespace dataflow
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_FLOWSENSITIVE_DEBUGSUPPORT_H_
