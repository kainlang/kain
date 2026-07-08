//===- lib/MC/MCInstrInfo.cpp - Target Instruction Info -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/mc/MCInstrInfo.h"
#include "core/mc/MCInst.h"
#include "core/mc/MCSubtargetInfo.h"

using namespace llvm;

bool MCInstrInfo::getDeprecatedInfo(MCInst &MI, const MCSubtargetInfo &STI,
                                    std::string &Info) const {
  unsigned Opcode = MI.getOpcode();
  if (ComplexDeprecationInfos && ComplexDeprecationInfos[Opcode])
    return ComplexDeprecationInfos[Opcode](MI, STI, Info);
  if (DeprecatedFeatures && DeprecatedFeatures[Opcode] != uint8_t(-1U) &&
      STI.getFeatureBits()[DeprecatedFeatures[Opcode]]) {
    // FIXME: it would be nice to include the subtarget feature here.
    Info = "deprecated";
    return true;
  }
  return false;
}
