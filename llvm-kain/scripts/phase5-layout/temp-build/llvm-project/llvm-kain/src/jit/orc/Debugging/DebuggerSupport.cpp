//===------ DebuggerSupport.cpp - Utils for enabling debugger support -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "jit/orc/Debugging/DebuggerSupport.h"
#include "jit/orc/Debugging/DebuggerSupportPlugin.h"
#include "jit/orc/Debugging/ELFDebugObjectPlugin.h"
#include "jit/orc/LLJIT.h"

#define DEBUG_TYPE "orc"

using namespace llvm;
using namespace llvm::orc;

namespace llvm::orc {

Error enableDebuggerSupport(LLJIT &J) {
  auto *ObjLinkingLayer = dyn_cast<ObjectLinkingLayer>(&J.getObjLinkingLayer());
  if (!ObjLinkingLayer)
    return make_error<StringError>("Cannot enable LLJIT debugger support: "
                                   "Debugger support requires JITLink",
                                   inconvertibleErrorCode());
  auto ProcessSymsJD = J.getProcessSymbolsJITDylib();
  if (!ProcessSymsJD)
    return make_error<StringError>("Cannot enable LLJIT debugger support: "
                                   "Process symbols are not available",
                                   inconvertibleErrorCode());

  auto &ES = J.getExecutionSession();
  const auto &TT = J.getTargetTriple();

  switch (TT.getObjectFormat()) {
  case Triple::ELF: {
    Error TargetSymErr = Error::success();
    ObjLinkingLayer->addPlugin(
        std::make_unique<ELFDebugObjectPlugin>(ES, false, true, TargetSymErr));
    return TargetSymErr;
  }
  case Triple::MachO: {
    auto DS = GDBJITDebugInfoRegistrationPlugin::Create(ES, *ProcessSymsJD, TT);
    if (!DS)
      return DS.takeError();
    ObjLinkingLayer->addPlugin(std::move(*DS));
    return Error::success();
  }
  default:
    return make_error<StringError>(
        "Cannot enable LLJIT debugger support: " +
            Triple::getObjectFormatTypeName(TT.getObjectFormat()) +
            " is not supported",
        inconvertibleErrorCode());
  }
}

} // namespace llvm::orc
