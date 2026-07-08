//===--------- LLJITUtilsCBindings.cpp - Advanced LLJIT features ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "c-api/LLJIT.h"
#include "c-api/LLJITUtils.h"

#include "jit/orc/Debugging/DebuggerSupport.h"
#include "jit/orc/LLJIT.h"

using namespace llvm;
using namespace llvm::orc;

DEFINE_SIMPLE_CONVERSION_FUNCTIONS(LLJIT, LLVMOrcLLJITRef)

LLVMErrorRef LLVMOrcLLJITEnableDebugSupport(LLVMOrcLLJITRef J) {
  return wrap(llvm::orc::enableDebuggerSupport(*unwrap(J)));
}
