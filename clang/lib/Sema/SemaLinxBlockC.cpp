//===--- SemaLinxBlockC.cpp - Semantic Analysis for Linx Block-C constructs -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements semantic analysis for Linx Block-C constructs.
///
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/Builtins.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/ScopeInfo.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/Template.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
using namespace clang;

ExprResult Sema::ActOnLinxBlockCExecConfigExpr(Scope *S, SourceLocation LLLLoc,
                                         MultiExprArg ExecConfig,
                                         SourceLocation GGGLoc) {
  if (Context.getTargetInfo().getTriple().isLinxClass())
    return ExprError(
        Diag(LLLLoc, diag::err_linx_superscalar_simt_unsupported));

  constexpr unsigned MaxDimArgs = 3;
  constexpr unsigned DefaultDimArg = 1;
  if (ExecConfig.size() > MaxDimArgs)
    return ExprError(Diag(LLLLoc, diag::err_blkc_wrong_dim_config)
        << getLinxVcallFuncName());
  SmallVector<Expr *, MaxDimArgs> NewArgs;
  for (unsigned I = 0; I < MaxDimArgs; I++) {
    if (I < ExecConfig.size())
      NewArgs.push_back(ExecConfig[I]);
    else
      NewArgs.push_back(IntegerLiteral::Create(Context,
                                            llvm::APInt(32, DefaultDimArg),
                                            Context.IntTy,
                                            LLLLoc));
  }
  ExprResult ER = new (Context) InitListExpr(Context, LLLLoc,
                                              cast<ArrayRef<Expr *>>(MultiExprArg(NewArgs.data(), NewArgs.size())),
                                              GGGLoc);
  return ER;
}

ExprResult Sema::ActOnLinxBlockCCall(ExprResult Call, Scope *Scope,
                                     SourceLocation LParenLoc,
                                     MultiExprArg ArgExprs,
                                     SourceLocation RParenLoc,
                                     Expr *ExecConfig) {

  SmallVector<Expr *, 12> ArgsPack;
  CallExpr *CE = dyn_cast<CallExpr>(Call.get());
  if (!CE)
    return Call;

  Expr *Callee = CE->getCallee();
  if (!Callee)
    return Call;

  if (isa<OverloadExpr>(Callee)) {
    // Check if this is a function call(not Linx VCall/MCall), return.
    if (!ExecConfig)
      return Call;
    ArgsPack.push_back(Callee);
  } else {
    FunctionDecl *CalleeFnDecl = CE->getDirectCallee();
    if (!CalleeFnDecl)
      return Call;

    const FunctionDecl *Canon = CalleeFnDecl->getCanonicalDecl();
    bool isMCall = false;
    bool isVCall = false;
    // Traverse redecl chain to detect attributes on any declaration.
    for (const FunctionDecl *D = Canon; D; D = D->getPreviousDecl()) {
      if (D->hasAttr<LinxBLKFuncMTCAttr>())
        isMCall = true;
      if (D->hasAttr<LinxBLKFuncVECAttr>())
        isVCall = true;
    }
    // We're in a plain function call (not a <<<...>>> launch). If the target
    // function is declared with mtc or vec attributes, this is an error.
    if (!ExecConfig && (isMCall || isVCall)) {
      Diag(CE->getBeginLoc(), diag::err_linx_mtc_vec_called_regular)
          << CalleeFnDecl->getNameAsString();
      return ExprError();
    }
    // Check if this is a function call(not Linx VCall/MCall), return.
    if (!ExecConfig && (!isMCall && !isVCall))
      return Call;
    // We are in <<<...>>> initiating a function call, and if the target
    // function does not use the mtc or vec attribute declaration, an error
    // will be generated.
    if (ExecConfig && (!isMCall && !isVCall)) {
      Diag(CE->getBeginLoc(), diag::err_linx_lack_mtc_vec_called_kernel)
          << CalleeFnDecl->getNameAsString();
      return ExprError();
    }
    ExprResult DeclRef = BuildDeclRefExpr(CalleeFnDecl, CalleeFnDecl->getType(), VK_LValue, LParenLoc);
    ArgsPack.push_back(DeclRef.get());
  }

  ArrayRef<Expr *> ConfigArgs = cast<InitListExpr>(ExecConfig)->inits();
  unsigned NumConfigArgs = ConfigArgs.size();
  for (unsigned I = 0; I < NumConfigArgs; I++) {
    ArgsPack.push_back(ConfigArgs[I]);
  }
  for (unsigned I = 0; I < ArgExprs.size(); I++) {
    ArgsPack.push_back(ArgExprs[I]);
  }

  ExprResult NewCall = BuildBuiltinCallExpr(LParenLoc, Builtin::BI__linx_vcall_par, ArgsPack);

  assert(NewCall.isUsable() && "__linx_vcall_par is not usable");
  return NewCall;
}

std::string Sema::getLinxVcallFuncName() const {
  return "__linx_vcall_par";
}
