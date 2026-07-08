//===----- LinkAllIR.h - Reference All VMCore Code --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This header file pulls in all the object modules of the VMCore library so
// that tools like llc, opt, and lli can ensure they are linked with all symbols
// from libVMCore.a It should only be used from a tool's main program.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LINKALLIR_H
#define LLVM_LINKALLIR_H

#include "core/support/Dwarf.h"
#include "core/ir/InlineAsm.h"
#include "core/ir/Instructions.h"
#include "core/ir/LLVMContext.h"
#include "core/ir/Module.h"
#include "core/ir/Verifier.h"
#include "support/adt/AlwaysTrue.h"
#include "support/adt/DynamicLibrary.h"
#include "support/adt/MathExtras.h"
#include "support/adt/Memory.h"
#include "support/adt/Mutex.h"
#include "support/adt/Path.h"
#include "support/adt/Process.h"
#include "support/adt/Program.h"
#include "support/adt/Signals.h"

namespace {
  struct ForceVMCoreLinking {
    ForceVMCoreLinking() {
      // We must reference VMCore in such a way that compilers will not
      // delete it all as dead code, even with whole program optimization.
      // This is so that globals in the translation units where these functions
      // are defined are forced to be initialized, populating various
      // registries.
      if (llvm::getNonFoldableAlwaysTrue())
        return;
      llvm::LLVMContext Context;
      (void)new llvm::Module("", Context);
      (void)new llvm::UnreachableInst(Context);
      (void)    llvm::createVerifierPass();
    }
  } ForceVMCoreLinking;
}

#endif
