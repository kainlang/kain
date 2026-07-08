//===- lib/MC/MCValue.cpp - MCValue implementation ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/mc/MCValue.h"
#include "core/config/llvm-config.h"
#include "core/mc/MCExpr.h"
#include "core/mc/MCSymbol.h"
#include "support/adt/Debug.h"
#include "support/adt/raw_ostream.h"

using namespace llvm;

void MCValue::print(raw_ostream &OS) const {
  if (isAbsolute()) {
    OS << getConstant();
    return;
  }

  // FIXME: prints as a number, which isn't ideal. But the meaning will be
  // target-specific anyway.
  if (getSpecifier())
    OS << ':' << getSpecifier() << ':';

  SymA->print(OS, nullptr);

  if (auto *B = getSubSym()) {
    OS << " - ";
    B->print(OS, nullptr);
  }

  if (getConstant())
    OS << " + " << getConstant();
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void MCValue::dump() const {
  print(dbgs());
}
#endif
