/*===------- llvm/Config/abi-breaking.h - llvm configuration -------*- C -*-===*/
/*                                                                            */
/* Part of the LLVM Project, under the Apache License v2.0 with LLVM          */
/* Exceptions.                                                                */
/* See https://llvm.org/LICENSE.txt for license information.                  */
/* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception                    */
/*                                                                            */
/*===----------------------------------------------------------------------===*/

/* This file controls the C++ ABI break introduced in LLVM public header. */

#ifndef LLVM_ABI_BREAKING_CHECKS_H
#define LLVM_ABI_BREAKING_CHECKS_H

#include "llvm/Config/llvm-config.h"

/* Define to enable checks that alter the LLVM C++ ABI */
#define LLVM_ENABLE_ABI_BREAKING_CHECKS 0

/* Define to enable reverse iteration of unordered llvm containers */
#define LLVM_ENABLE_REVERSE_ITERATION 0

#if !defined(__has_attribute)
#define __has_attribute(attribute) 0
#endif

#if defined(LLVM_BUILD_STATIC) || !defined(LLVM_ENABLE_LLVM_EXPORT_ANNOTATIONS)
#define ABI_BREAKING_EXPORT_ABI
#else
#if defined(_WIN32)
#if defined(LLVM_EXPORTS)
#define ABI_BREAKING_EXPORT_ABI __declspec(dllexport)
#else
#define ABI_BREAKING_EXPORT_ABI __declspec(dllimport)
#endif
#else
#if __has_attribute(visibility)
#define ABI_BREAKING_EXPORT_ABI __attribute__((__visibility__("default")))
#else
#define ABI_BREAKING_EXPORT_ABI
#endif
#endif
#endif

#if !defined(LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING) || !LLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING

#if defined(_MSC_VER)
#define LLVM_XSTR(s) LLVM_STR(s)
#define LLVM_STR(s) #s
#pragma detect_mismatch("LLVM_ENABLE_ABI_BREAKING_CHECKS", LLVM_XSTR(LLVM_ENABLE_ABI_BREAKING_CHECKS))
#undef LLVM_XSTR
#undef LLVM_STR
#elif defined(_WIN32) || defined(__CYGWIN__)
#elif defined(__cplusplus)
#if !(defined(_AIX) && defined(__GNUC__) && !defined(__clang__))
#define LLVM_HIDDEN_VISIBILITY __attribute__ ((visibility("hidden")))
#else
#define LLVM_HIDDEN_VISIBILITY
#endif
namespace llvm {
#if LLVM_ENABLE_ABI_BREAKING_CHECKS
ABI_BREAKING_EXPORT_ABI extern int EnableABIBreakingChecks;
LLVM_HIDDEN_VISIBILITY
__attribute__((weak)) int *VerifyEnableABIBreakingChecks =
    &EnableABIBreakingChecks;
#else
ABI_BREAKING_EXPORT_ABI extern int DisableABIBreakingChecks;
LLVM_HIDDEN_VISIBILITY
__attribute__((weak)) int *VerifyDisableABIBreakingChecks =
    &DisableABIBreakingChecks;
#endif
}
#undef LLVM_HIDDEN_VISIBILITY
#endif

#endif

#endif
