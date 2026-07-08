//===-- DiagnosticCommonInterface.inc - Common Diagnostics ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
//
//===----------------------------------------------------------------------===//
//
// This file defines common diagnostic enums in the clang::diag namespace.
//
//===----------------------------------------------------------------------===//

// Define DIAG macro to produce enum entries in clang::diag namespace.
// In this context, DIAG(name, severity, desc) produces just the name.
#define DIAG(ENUM, CLASS, DESC) ENUM

// Generate enum entries from DiagnosticCommonKinds.inc.
#define GET_DIAG_ENUMS
#include "basic/DiagnosticCommonKinds.inc"
#undef GET_DIAG_ENUMS
#undef DIAG
