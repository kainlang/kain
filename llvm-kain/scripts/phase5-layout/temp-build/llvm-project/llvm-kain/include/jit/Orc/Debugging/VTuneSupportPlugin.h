//===--- VTuneSupportPlugin.h -- Support for VTune profiler ---*- C++ -*---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Handles support for registering code with VIntel Tune's Amplifier JIT API.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_DEBUGGING_VTUNESUPPORT_H
#define LLVM_EXECUTIONENGINE_ORC_DEBUGGING_VTUNESUPPORT_H

#include "support/adt/DenseMap.h"
#include "support/adt/SmallVector.h"
#include "jit/orc/Core.h"
#include "jit/orc/ObjectLinkingLayer.h"
#include "jit/orc/Shared/SimplePackedSerialization.h"
#include "jit/orc/Shared/VTuneSharedStructs.h"
#include "support/adt/Compiler.h"

namespace llvm {

namespace orc {

class LLVM_ABI VTuneSupportPlugin : public ObjectLinkingLayer::Plugin {
public:
  VTuneSupportPlugin(ExecutorProcessControl &EPC, ExecutorAddr RegisterImplAddr,
                     ExecutorAddr UnregisterImplAddr, bool EmitDebugInfo)
      : EPC(EPC), RegisterVTuneImplAddr(RegisterImplAddr),
        UnregisterVTuneImplAddr(UnregisterImplAddr),
        EmitDebugInfo(EmitDebugInfo) {}

  void modifyPassConfig(MaterializationResponsibility &MR,
                        jitlink::LinkGraph &G,
                        jitlink::PassConfiguration &Config) override;

  Error notifyEmitted(MaterializationResponsibility &MR) override;
  Error notifyFailed(MaterializationResponsibility &MR) override;
  Error notifyRemovingResources(JITDylib &JD, ResourceKey K) override;
  void notifyTransferringResources(JITDylib &JD, ResourceKey DstKey,
                                   ResourceKey SrcKey) override;

  static Expected<std::unique_ptr<VTuneSupportPlugin>>
  Create(ExecutorProcessControl &EPC, JITDylib &JD, bool EmitDebugInfo,
         bool TestMode = false);

private:
  ExecutorProcessControl &EPC;
  ExecutorAddr RegisterVTuneImplAddr;
  ExecutorAddr UnregisterVTuneImplAddr;
  std::mutex PluginMutex;
  uint64_t NextMethodID = 0;
  DenseMap<MaterializationResponsibility *, std::pair<uint64_t, uint64_t>>
      PendingMethodIDs;
  DenseMap<ResourceKey, SmallVector<std::pair<uint64_t, uint64_t>>>
      LoadedMethodIDs;
  bool EmitDebugInfo;
};

} // end namespace orc

} // end namespace llvm

#endif
