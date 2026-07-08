//===--- AllDiagnostics.h - Aggregate Diagnostic headers --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Includes all the separate Diagnostic headers & some related helpers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_ALLDIAGNOSTICS_H
#define LLVM_CLANG_BASIC_ALLDIAGNOSTICS_H

#include "basic/DiagnosticAST.h"
#include "basic/DiagnosticAnalysis.h"
#include "basic/DiagnosticComment.h"
#include "basic/DiagnosticCrossTU.h"
#include "basic/DiagnosticDriver.h"
#include "basic/DiagnosticFrontend.h"
#include "basic/DiagnosticInstallAPI.h"
#include "basic/DiagnosticLex.h"
#include "basic/DiagnosticParse.h"
#include "basic/DiagnosticRefactoring.h"
#include "basic/DiagnosticSema.h"
#include "basic/DiagnosticSerialization.h"
#include "basic/DiagnosticTrap.h"

namespace clang {
template <size_t SizeOfStr, typename FieldType> class StringSizerHelper {
  static_assert(SizeOfStr <= FieldType(~0U), "Field too small!");

public:
  enum { Size = SizeOfStr };
};
} // end namespace clang

#define STR_SIZE(str, fieldTy)                                                 \
  clang::StringSizerHelper<sizeof(str) - 1, fieldTy>::Size

#endif
