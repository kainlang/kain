//===- DWARF.h --------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// llvm-kain stub — DWARF support disabled, empty implementations.

#ifndef LLD_DWARF_H
#define LLD_DWARF_H

#include "lld/Common/LLVM.h"
#include "support/adt/DenseMap.h"
#include "support/adt/StringRef.h"
#include "support/adt/Optional.h"
#include <memory>
#include <string>
#include <vector>

namespace llvm {
struct DILineInfo {
  std::string FileName;
  uint32_t Line;
  uint32_t Column;
  static constexpr const char *BadString = "<invalid>";
  DILineInfo() : Line(0), Column(0) {}
};
} // namespace llvm

namespace lld {

class DWARFCache {
public:
  DWARFCache() = default;
  std::optional<llvm::DILineInfo> getDILineInfo(uint64_t offset,
                                                uint64_t sectionIndex);
  std::optional<std::pair<std::string, unsigned>>
  getVariableLoc(StringRef name);
  void *getContext() { return nullptr; }
};

} // namespace lld

#endif
