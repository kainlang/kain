//===- Checker.h - C++ Lifetime Safety Analysis -*----------- C++-*-=========//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines and enforces the lifetime safety policy. It detects
// use-after-free errors by examining loan expiration points and checking if
// any live origins hold the expired loans.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_CHECKER_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_CHECKER_H

#include "analysis/Analyses/LifetimeSafety/Facts.h"
#include "analysis/Analyses/LifetimeSafety/LifetimeSafety.h"
#include "analysis/Analyses/LifetimeSafety/LiveOrigins.h"
#include "analysis/Analyses/LifetimeSafety/LoanPropagation.h"

namespace clang::lifetimes::internal {

/// Runs the lifetime checker, which detects use-after-free errors by
/// examining loan expiration points and checking if any live origins hold
/// the expired loan.
void runLifetimeChecker(const LoanPropagationAnalysis &LoanPropagation,
                        const MovedLoansAnalysis &MovedLoans,
                        const LiveOriginsAnalysis &LiveOrigins,
                        FactManager &FactMgr, AnalysisDeclContext &ADC,
                        LifetimeSafetySemaHelper *SemaHelper,
                        const LifetimeSafetyOpts &LSOpts);

} // namespace clang::lifetimes::internal

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_CHECKER_H
