//===- IRPartitionLayer.h - Partition IR module on lookup -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// JIT layer for breaking up modules into smaller submodules that only contains
// looked up symbols.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_IRPARTITIONLAYER_H
#define LLVM_EXECUTIONENGINE_ORC_IRPARTITIONLAYER_H

#include "jit/orc/IndirectionUtils.h"
#include "jit/orc/Layer.h"
#include "core/ir/Attributes.h"
#include "core/ir/Constant.h"
#include "core/ir/Constants.h"
#include "core/ir/DataLayout.h"
#include "core/ir/Function.h"
#include "core/ir/GlobalAlias.h"
#include "core/ir/GlobalValue.h"
#include "core/ir/GlobalVariable.h"
#include "core/ir/Instruction.h"
#include "core/ir/Mangler.h"
#include "core/ir/Module.h"
#include "core/ir/Type.h"
#include "support/adt/Compiler.h"

namespace llvm {
namespace orc {

/// A layer that breaks up IR modules into smaller submodules that only contains
/// looked up symbols.
class LLVM_ABI IRPartitionLayer : public IRLayer {
  friend class PartitioningIRMaterializationUnit;

public:
  using GlobalValueSet = std::set<const GlobalValue *>;

  /// Partitioning function.
  using PartitionFunction =
      std::function<std::optional<GlobalValueSet>(GlobalValueSet Requested)>;

  /// Construct a IRPartitionLayer.
  IRPartitionLayer(ExecutionSession &ES, IRLayer &BaseLayer);

  /// Off-the-shelf partitioning which compiles all requested symbols (usually
  /// a single function at a time).
  static std::optional<GlobalValueSet>
  compileRequested(GlobalValueSet Requested);

  /// Off-the-shelf partitioning which compiles whole modules whenever any
  /// symbol in them is requested.
  static std::optional<GlobalValueSet>
  compileWholeModule(GlobalValueSet Requested);

  /// Sets the partition function.
  void setPartitionFunction(PartitionFunction Partition);

  /// Emits the given module. This should not be called by clients: it will be
  /// called by the JIT when a definition added via the add method is requested.
  void emit(std::unique_ptr<MaterializationResponsibility> R,
            ThreadSafeModule TSM) override;

private:
  void cleanUpModule(Module &M);

  void expandPartition(GlobalValueSet &Partition);

  void emitPartition(std::unique_ptr<MaterializationResponsibility> R,
                     ThreadSafeModule TSM,
                     IRMaterializationUnit::SymbolNameToDefinitionMap Defs);

  IRLayer &BaseLayer;
  PartitionFunction Partition = compileRequested;
  SymbolLinkagePromoter PromoteSymbols;
};

} // namespace orc
} // namespace llvm

#endif
