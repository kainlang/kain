//===------ CompileUtils.cpp - Utilities for compiling IR in the JIT ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "jit/orc/CompileUtils.h"

#include "support/adt/SmallVector.h"
#include "jit/ObjectCache.h"
#include "core/ir/LegacyPassManager.h"
#include "core/ir/Module.h"
#include "core/mc/MCContext.h"
#include "core/object/ObjectFile.h"
#include "support/adt/Error.h"
#include "support/adt/MemoryBuffer.h"
#include "support/adt/SmallVectorMemoryBuffer.h"
#include "target/shared/TargetMachine.h"

namespace llvm {
namespace orc {

IRSymbolMapper::ManglingOptions
irManglingOptionsFromTargetOptions(const TargetOptions &Opts) {
  IRSymbolMapper::ManglingOptions MO;

  MO.EmulatedTLS = Opts.EmulatedTLS;

  return MO;
}

/// Compile a Module to an ObjectFile.
Expected<SimpleCompiler::CompileResult> SimpleCompiler::operator()(Module &M) {
  if (M.getDataLayout().isDefault())
    M.setDataLayout(TM.createDataLayout());

  CompileResult CachedObject = tryToLoadFromObjectCache(M);
  if (CachedObject)
    return std::move(CachedObject);

  SmallVector<char, 0> ObjBufferSV;

  {
    raw_svector_ostream ObjStream(ObjBufferSV);

    legacy::PassManager PM;
    MCContext *Ctx;
    if (TM.addPassesToEmitMC(PM, Ctx, ObjStream))
      return make_error<StringError>("Target does not support MC emission",
                                     inconvertibleErrorCode());
    PM.run(M);
  }

  auto ObjBuffer = std::make_unique<SmallVectorMemoryBuffer>(
      std::move(ObjBufferSV), M.getModuleIdentifier() + "-jitted-objectbuffer",
      /*RequiresNullTerminator=*/false);

  auto Obj = object::ObjectFile::createObjectFile(ObjBuffer->getMemBufferRef());

  if (!Obj)
    return Obj.takeError();

  notifyObjectCompiled(M, *ObjBuffer);
  return std::move(ObjBuffer);
}

SimpleCompiler::CompileResult
SimpleCompiler::tryToLoadFromObjectCache(const Module &M) {
  if (!ObjCache)
    return CompileResult();

  return ObjCache->getObject(&M);
}

void SimpleCompiler::notifyObjectCompiled(const Module &M,
                                          const MemoryBuffer &ObjBuffer) {
  if (ObjCache)
    ObjCache->notifyObjectCompiled(&M, ObjBuffer.getMemBufferRef());
}

ConcurrentIRCompiler::ConcurrentIRCompiler(JITTargetMachineBuilder JTMB,
                                           ObjectCache *ObjCache)
    : IRCompiler(irManglingOptionsFromTargetOptions(JTMB.getOptions())),
      JTMB(std::move(JTMB)), ObjCache(ObjCache) {}

Expected<std::unique_ptr<MemoryBuffer>>
ConcurrentIRCompiler::operator()(Module &M) {
  auto TM = cantFail(JTMB.createTargetMachine());
  SimpleCompiler C(*TM, ObjCache);
  return C(M);
}

} // end namespace orc
} // end namespace llvm
