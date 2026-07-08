//===- lib/MC/MCLabel.cpp - MCLabel implementation ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/mc/MCLabel.h"
#include "core/config/llvm-config.h"
#include "support/adt/Compiler.h"
#include "support/adt/Debug.h"
#include "support/adt/raw_ostream.h"

using namespace llvm;

void MCLabel::print(raw_ostream &OS) const {
  OS << '"' << getInstance() << '"';
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void MCLabel::dump() const {
  print(dbgs());
}
#endif
