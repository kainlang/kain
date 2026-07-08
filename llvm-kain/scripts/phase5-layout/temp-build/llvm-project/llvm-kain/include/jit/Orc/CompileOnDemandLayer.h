//===- CompileOnDemandLayer.h - Compile each function on demand -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// JIT layer for breaking up modules and inserting callbacks to allow
// individual functions to be compiled on demand.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_COMPILEONDEMANDLAYER_H
#define LLVM_EXECUTIONENGINE_ORC_COMPILEONDEMANDLAYER_H

#include "support/adt/APInt.h"
#include "support/adt/STLExtras.h"
#include "support/adt/StringRef.h"
#include "jit/JITSymbol.h"
#include "jit/orc/IndirectionUtils.h"
#include "jit/orc/Layer.h"
#include "jit/orc/LazyReexports.h"
#include "jit/orc/Shared/OrcError.h"
#include "jit/orc/Speculation.h"
#include "jit/RuntimeDyld.h"
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
#include "support/adt/Casting.h"
#include "support/adt/Compiler.h"
#include "support/adt/raw_ostream.h"
#include "core/passes/Utils/ValueMapper.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <utility>

namespace llvm {
namespace orc {

class LLVM_ABI CompileOnDemandLayer : public IRLayer {
public:
  /// Builder for IndirectStubsManagers.
  using IndirectStubsManagerBuilder =
      std::function<std::unique_ptr<IndirectStubsManager>()>;

  /// Construct a CompileOnDemandLayer.
  CompileOnDemandLayer(ExecutionSession &ES, IRLayer &BaseLayer,
                       LazyCallThroughManager &LCTMgr,
                       IndirectStubsManagerBuilder BuildIndirectStubsManager);
  /// Sets the ImplSymbolMap
  void setImplMap(ImplSymbolMap *Imp);

  /// Emits the given module. This should not be called by clients: it will be
  /// called by the JIT when a definition added via the add method is requested.
  void emit(std::unique_ptr<MaterializationResponsibility> R,
            ThreadSafeModule TSM) override;

private:
  struct PerDylibResources {
  public:
    PerDylibResources(JITDylib &ImplD,
                      std::unique_ptr<IndirectStubsManager> ISMgr)
        : ImplD(ImplD), ISMgr(std::move(ISMgr)) {}
    JITDylib &getImplDylib() { return ImplD; }
    IndirectStubsManager &getISManager() { return *ISMgr; }

  private:
    JITDylib &ImplD;
    std::unique_ptr<IndirectStubsManager> ISMgr;
  };

  using PerDylibResourcesMap = std::map<const JITDylib *, PerDylibResources>;

  PerDylibResources &getPerDylibResources(JITDylib &TargetD);

  mutable std::mutex CODLayerMutex;

  IRLayer &BaseLayer;
  LazyCallThroughManager &LCTMgr;
  IndirectStubsManagerBuilder BuildIndirectStubsManager;
  PerDylibResourcesMap DylibResources;
  ImplSymbolMap *AliaseeImpls = nullptr;
};

} // end namespace orc
} // end namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_COMPILEONDEMANDLAYER_H
