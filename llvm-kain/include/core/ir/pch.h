//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// Precompiled header for LLVMCore.
///
//===----------------------------------------------------------------------===//

#include "core/ir/BasicBlock.h"
#include "core/ir/CFG.h"
#include "core/ir/Constant.h"
#include "core/ir/Constants.h"
#include "core/ir/DataLayout.h"
#include "core/ir/DebugInfoMetadata.h"
#include "core/ir/Dominators.h"
#include "core/ir/Function.h"
#include "core/ir/IRBuilder.h"
#include "core/ir/InlineAsm.h"
#include "core/ir/Instruction.h"
#include "core/ir/Instructions.h"
#include "core/ir/IntrinsicInst.h"
#include "core/ir/Module.h"
#include "core/ir/ModuleSummaryIndex.h"
#include "core/ir/PassManager.h"
#include "core/ir/PatternMatch.h"
#include "core/ir/Value.h"
#include "support/adt/pch.h"
