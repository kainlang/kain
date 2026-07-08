//===- MCSPIRVStreamer.h - MCStreamer SPIR-V Object File Interface -*- C++ ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Overrides MCObjectStreamer to disable all unnecessary features with stubs.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCSPIRVSTREAMER_H
#define LLVM_MC_MCSPIRVSTREAMER_H

#include "core/mc/MCAsmBackend.h"
#include "core/mc/MCCodeEmitter.h"
#include "core/mc/MCObjectStreamer.h"
#include "core/mc/MCObjectWriter.h"

namespace llvm {
class MCInst;
class raw_ostream;

class MCSPIRVStreamer : public MCObjectStreamer {
public:
  MCSPIRVStreamer(MCContext &Context, std::unique_ptr<MCAsmBackend> TAB,
                  std::unique_ptr<MCObjectWriter> OW,
                  std::unique_ptr<MCCodeEmitter> Emitter)
      : MCObjectStreamer(Context, std::move(TAB), std::move(OW),
                         std::move(Emitter)) {}

  bool emitSymbolAttribute(MCSymbol *Symbol, MCSymbolAttr Attribute) override {
    return false;
  }
  void emitCommonSymbol(MCSymbol *Symbol, uint64_t Size,
                        Align ByteAlignment) override {}
};

} // end namespace llvm

#endif
