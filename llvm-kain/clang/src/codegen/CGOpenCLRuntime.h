//===----- CGOpenCLRuntime.h - Interface to OpenCL Runtimes -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for OpenCL code generation.  Concrete
// subclasses of this implement code generation for specific OpenCL
// runtime libraries.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_CODEGEN_CGOPENCLRUNTIME_H
#define LLVM_CLANG_LIB_CODEGEN_CGOPENCLRUNTIME_H

#include "ast/Expr.h"
#include "ast/Type.h"
#include "support/adt/DenseMap.h"
#include "support/adt/StringMap.h"
#include "core/ir/Type.h"
#include "core/ir/Value.h"

namespace clang {

class BlockExpr;
class Expr;
class VarDecl;

namespace CodeGen {

class CodeGenFunction;
class CodeGenModule;

class CGOpenCLRuntime {
protected:
  CodeGenModule &CGM;
  llvm::Type *PipeTy;
  llvm::Type *SamplerTy;

  /// Structure for enqueued block information.
  struct EnqueuedBlockInfo {
    llvm::Function *InvokeFunc; /// Block invoke function.
    llvm::Value *KernelHandle;  /// Enqueued block kernel reference.
    llvm::Value *BlockArg;      /// The first argument to enqueued block kernel.
    llvm::Type *BlockTy;        /// Type of the block argument.
  };
  /// Maps block expression to block information.
  llvm::DenseMap<const Expr *, EnqueuedBlockInfo> EnqueuedBlockMap;

  llvm::PointerType *getPointerType(const Type *T);

public:
  CGOpenCLRuntime(CodeGenModule &CGM)
      : CGM(CGM), PipeTy(nullptr), SamplerTy(nullptr) {}
  ~CGOpenCLRuntime();

  /// Emit the IR required for a work-group-local variable declaration, and add
  /// an entry to CGF's LocalDeclMap for D.  The base class does this using
  /// CodeGenFunction::EmitStaticVarDecl to emit an internal global for D.
  void EmitWorkGroupLocalVarDecl(CodeGenFunction &CGF, const VarDecl &D);

  llvm::Type *convertOpenCLSpecificType(const Type *T);

  llvm::Type *getPipeType(const PipeType *T);

  llvm::Type *getSamplerType(const Type *T);

  // Returns a value which indicates the size in bytes of the pipe
  // element.
  llvm::Value *getPipeElemSize(const Expr *PipeArg);

  // Returns a value which indicates the alignment in bytes of the pipe
  // element.
  llvm::Value *getPipeElemAlign(const Expr *PipeArg);

  /// \return __generic void* type.
  llvm::PointerType *getGenericVoidPointerType();

  /// \return enqueued block information for enqueued block.
  EnqueuedBlockInfo emitOpenCLEnqueuedBlock(CodeGenFunction &CGF,
                                            const Expr *E);

  /// Record invoke function and block literal emitted during normal
  /// codegen for a block expression. The information is used by
  /// emitOpenCLEnqueuedBlock to emit wrapper kernel.
  ///
  /// \param InvokeF invoke function emitted for the block expression.
  /// \param Block block literal emitted for the block expression.
  void recordBlockInfo(const BlockExpr *E, llvm::Function *InvokeF,
                       llvm::Value *Block, llvm::Type *BlockTy);

  /// \return LLVM block invoke function emitted for an expression derived from
  /// the block expression.
  llvm::Function *getInvokeFunction(const Expr *E);
};

}
}

#endif
