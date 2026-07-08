//===-- BitstreamRemarkContainer.h - Container for remarks - STUB ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// STUBBED for llvm-kain (Bitcode stripped).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_REMARKS_BITSTREAMREMARKCONTAINER_H
#define LLVM_REMARKS_BITSTREAMREMARKCONTAINER_H

#include <cstdint>

namespace llvm {
namespace remarks {

enum class BitstreamRemarkContainerType {
  ContainerRemarks,
  ContainerRemarkVersionDummy,
};

constexpr uint64_t CurrentContainerVersion = 1;
constexpr unsigned META_BLOCK_ID = 1;
constexpr auto ContainerMagic = "RMRK";

} // namespace remarks
} // namespace llvm

#endif
