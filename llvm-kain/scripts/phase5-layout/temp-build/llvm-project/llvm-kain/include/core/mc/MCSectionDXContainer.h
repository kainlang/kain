//===- MCSectionDXContainer.h - DXContainer MC Sections ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the MCSectionDXContainer class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCSECTIONDXCONTAINER_H
#define LLVM_MC_MCSECTIONDXCONTAINER_H

#include "core/mc/MCSection.h"
#include "core/mc/SectionKind.h"

namespace llvm {

class MCSymbol;

class MCSectionDXContainer final : public MCSection {
  friend class MCContext;

  MCSectionDXContainer(StringRef Name, SectionKind K, MCSymbol *Begin)
      : MCSection(Name, K.isText(), /*IsVirtual=*/false, Begin) {}
};

} // end namespace llvm

#endif // LLVM_MC_MCSECTIONDXCONTAINER_H
