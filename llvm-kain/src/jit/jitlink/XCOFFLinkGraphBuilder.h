//===----- XCOFFLinkGraphBuilder.h - XCOFF LinkGraph builder ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Generic XCOFF LinkGraph building code.
//
//===----------------------------------------------------------------------===//

#ifndef LIB_EXECUTIONENGINE_JITLINK_XCOFFLINKGRAPHBUILDER_H
#define LIB_EXECUTIONENGINE_JITLINK_XCOFFLINKGRAPHBUILDER_H

#include "jit/jitlink/JITLink.h"
#include "jit/orc/SymbolStringPool.h"
#include "core/object/ObjectFile.h"
#include "core/object/XCOFFObjectFile.h"
#include "support/target/SubtargetFeature.h"
#include <memory>

namespace llvm {
namespace jitlink {

class XCOFFLinkGraphBuilder {
public:
  virtual ~XCOFFLinkGraphBuilder() = default;
  Expected<std::unique_ptr<LinkGraph>> buildGraph();

public:
  XCOFFLinkGraphBuilder(const object::XCOFFObjectFile &Obj,
                        std::shared_ptr<orc::SymbolStringPool> SSP, Triple TT,
                        SubtargetFeatures Features,
                        LinkGraph::GetEdgeKindNameFunction GetEdgeKindName);
  LinkGraph &getGraph() const { return *G; }
  const object::XCOFFObjectFile &getObject() const { return Obj; }

private:
  Error processSections();
  Error processCsectsAndSymbols();
  Error processRelocations();

private:
  const object::XCOFFObjectFile &Obj;
  std::unique_ptr<LinkGraph> G;

  Section *UndefSection;

  struct SectionEntry {
    jitlink::Section *Section;
    object::SectionRef SectionData;
  };

  DenseMap<uint16_t, SectionEntry> SectionTable;
  DenseMap<uint32_t, Block *> CsectTable;
  DenseMap<uint32_t, Symbol *> SymbolIndexTable;
};

} // namespace jitlink
} // namespace llvm

#endif // LIB_EXECUTIONENGINE_JITLINK_XCOFFLINKGRAPHBUILDER_H
