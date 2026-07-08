//==- MemProfContextDisambiguation.h - Context Disambiguation (stub) ------*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// llvm-kain stub: MemProf requires ProfileData which is stripped.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_IPO_MEMPROF_CONTEXT_DISAMBIGUATION_H
#define LLVM_TRANSFORMS_IPO_MEMPROF_CONTEXT_DISAMBIGUATION_H

#include "core/ir/PassManager.h"

namespace llvm {

class MemProfContextDisambiguation
    : public OptionalPassInfoMixin<MemProfContextDisambiguation> {
public:
  // Minimal stub — no-op pass
};

} // end namespace llvm

#endif // LLVM_TRANSFORMS_IPO_MEMPROF_CONTEXT_DISAMBIGUATION_H
