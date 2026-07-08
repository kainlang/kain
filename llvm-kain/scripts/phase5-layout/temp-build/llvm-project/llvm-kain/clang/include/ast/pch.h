//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// Precompiled header for clangAST.
///
//===----------------------------------------------------------------------===//

#include "ast/ASTContext.h"
#include "ast/Attr.h"
#include "ast/CanonicalType.h"
#include "ast/Decl.h"
#include "ast/DeclCXX.h"
#include "ast/DeclObjC.h"
#include "ast/DeclOpenMP.h"
#include "ast/DeclTemplate.h"
#include "ast/DynamicRecursiveASTVisitor.h"
#include "ast/Expr.h"
#include "ast/ExprCXX.h"
#include "ast/ExprObjC.h"
#include "ast/GlobalDecl.h"
#include "ast/OpenMPClause.h"
#include "ast/RecursiveASTVisitor.h"
#include "ast/Stmt.h"
#include "ast/StmtOpenMP.h"
#include "ast/StmtVisitor.h"
#include "ast/Type.h"
#include "support/adt/pch.h"
