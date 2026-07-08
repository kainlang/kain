//===- DWARF.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// llvm-kain stub — DebugInfo DWARF library deleted Phase 3.
// All methods return empty results.

#include "lld/Common/DWARF.h"

using namespace lld;

std::optional<llvm::DILineInfo> DWARFCache::getDILineInfo(uint64_t offset,
                                                          uint64_t sectionIndex) {
  return std::nullopt;
}

std::optional<std::pair<std::string, unsigned>>
DWARFCache::getVariableLoc(StringRef name) {
  return std::nullopt;
}
