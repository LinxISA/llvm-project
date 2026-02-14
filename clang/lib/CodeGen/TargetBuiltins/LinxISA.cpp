//===--- LinxISA.cpp - Emit LLVM Code for LinxISA builtins ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CGBuiltin.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsLinx.h"

using namespace clang;
using namespace clang::CodeGen;

llvm::Value *CodeGenFunction::EmitLinxISABuiltinExpr(unsigned BuiltinID,
                                                     const CallExpr *E,
                                                     ReturnValueSlot) {
  switch (BuiltinID) {
  case LinxISA::BI__builtin_linx_tma_tload: {
    llvm::Value *Base = EmitScalarExpr(E->getArg(0));
    llvm::Value *Size = EmitScalarExpr(E->getArg(1));
    Size = Builder.CreateIntCast(Size, Builder.getInt32Ty(), /*isSigned=*/false);

    llvm::Type *TileTy = ConvertType(E->getType());
    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_tma_tload, {TileTy});
    return Builder.CreateCall(F, {Base, Size}, "linx.tload");
  }
  case LinxISA::BI__builtin_linx_tma_tstore: {
    llvm::Value *Base = EmitScalarExpr(E->getArg(0));
    llvm::Value *Tile = EmitScalarExpr(E->getArg(1));
    llvm::Value *Size = EmitScalarExpr(E->getArg(2));
    Size = Builder.CreateIntCast(Size, Builder.getInt32Ty(), /*isSigned=*/false);

    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_tma_tstore, {Tile->getType()});
    return Builder.CreateCall(F, {Base, Tile, Size});
  }
  case LinxISA::BI__builtin_linx_tma_tload_desc: {
    llvm::Value *Base = EmitScalarExpr(E->getArg(0));
    llvm::Value *Layout = EmitScalarExpr(E->getArg(1));
    llvm::Value *LB0 = EmitScalarExpr(E->getArg(2));
    llvm::Value *LB1 = EmitScalarExpr(E->getArg(3));
    llvm::Value *LB2 = EmitScalarExpr(E->getArg(4));
    llvm::Value *Size = EmitScalarExpr(E->getArg(5));
    llvm::Type *I32 = Builder.getInt32Ty();
    Layout = Builder.CreateIntCast(Layout, I32, /*isSigned=*/false);
    LB0 = Builder.CreateIntCast(LB0, I32, /*isSigned=*/false);
    LB1 = Builder.CreateIntCast(LB1, I32, /*isSigned=*/false);
    LB2 = Builder.CreateIntCast(LB2, I32, /*isSigned=*/false);
    Size = Builder.CreateIntCast(Size, I32, /*isSigned=*/false);

    llvm::Type *TileTy = ConvertType(E->getType());
    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_tma_tload_desc, {TileTy});
    return Builder.CreateCall(F, {Base, Layout, LB0, LB1, LB2, Size},
                              "linx.tload.desc");
  }
  case LinxISA::BI__builtin_linx_tma_tstore_desc: {
    llvm::Value *Base = EmitScalarExpr(E->getArg(0));
    llvm::Value *Tile = EmitScalarExpr(E->getArg(1));
    llvm::Value *Layout = EmitScalarExpr(E->getArg(2));
    llvm::Value *LB0 = EmitScalarExpr(E->getArg(3));
    llvm::Value *LB1 = EmitScalarExpr(E->getArg(4));
    llvm::Value *LB2 = EmitScalarExpr(E->getArg(5));
    llvm::Value *Size = EmitScalarExpr(E->getArg(6));
    llvm::Type *I32 = Builder.getInt32Ty();
    Layout = Builder.CreateIntCast(Layout, I32, /*isSigned=*/false);
    LB0 = Builder.CreateIntCast(LB0, I32, /*isSigned=*/false);
    LB1 = Builder.CreateIntCast(LB1, I32, /*isSigned=*/false);
    LB2 = Builder.CreateIntCast(LB2, I32, /*isSigned=*/false);
    Size = Builder.CreateIntCast(Size, I32, /*isSigned=*/false);

    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_tma_tstore_desc,
                         {Tile->getType()});
    return Builder.CreateCall(F, {Base, Tile, Layout, LB0, LB1, LB2, Size});
  }
  case LinxISA::BI__builtin_linx_cube_mamulb: {
    llvm::Value *A = EmitScalarExpr(E->getArg(0));
    llvm::Value *B = EmitScalarExpr(E->getArg(1));
    llvm::Value *M = EmitScalarExpr(E->getArg(2));
    llvm::Value *N = EmitScalarExpr(E->getArg(3));
    llvm::Value *K = EmitScalarExpr(E->getArg(4));

    llvm::Type *I32 = Builder.getInt32Ty();
    M = Builder.CreateIntCast(M, I32, /*isSigned=*/false);
    N = Builder.CreateIntCast(N, I32, /*isSigned=*/false);
    K = Builder.CreateIntCast(K, I32, /*isSigned=*/false);

    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_cube_mamulb, {A->getType()});
    return Builder.CreateCall(F, {A, B, M, N, K}, "linx.mamulb");
  }
  case LinxISA::BI__builtin_linx_cube_mamulb_acc: {
    llvm::Value *Acc = EmitScalarExpr(E->getArg(0));
    llvm::Value *A = EmitScalarExpr(E->getArg(1));
    llvm::Value *B = EmitScalarExpr(E->getArg(2));
    llvm::Value *M = EmitScalarExpr(E->getArg(3));
    llvm::Value *N = EmitScalarExpr(E->getArg(4));
    llvm::Value *K = EmitScalarExpr(E->getArg(5));

    llvm::Type *I32 = Builder.getInt32Ty();
    M = Builder.CreateIntCast(M, I32, /*isSigned=*/false);
    N = Builder.CreateIntCast(N, I32, /*isSigned=*/false);
    K = Builder.CreateIntCast(K, I32, /*isSigned=*/false);

    llvm::Function *F = CGM.getIntrinsic(llvm::Intrinsic::linx_cube_mamulb_acc,
                                         {Acc->getType()});
    return Builder.CreateCall(F, {Acc, A, B, M, N, K}, "linx.mamulb.acc");
  }
  case LinxISA::BI__builtin_linx_vblock_launch: {
    llvm::Value *VKind = EmitScalarExpr(E->getArg(0));
    llvm::Value *Body = EmitScalarExpr(E->getArg(1));
    llvm::Value *Dim0 = EmitScalarExpr(E->getArg(2));
    llvm::Value *Dim1 = EmitScalarExpr(E->getArg(3));
    llvm::Value *Dim2 = EmitScalarExpr(E->getArg(4));
    llvm::Value *Attr = EmitScalarExpr(E->getArg(5));

    VKind = Builder.CreateIntCast(VKind, Builder.getInt32Ty(), /*isSigned=*/false);
    Dim0 = Builder.CreateIntCast(Dim0, Builder.getInt64Ty(), /*isSigned=*/false);
    Dim1 = Builder.CreateIntCast(Dim1, Builder.getInt64Ty(), /*isSigned=*/false);
    Dim2 = Builder.CreateIntCast(Dim2, Builder.getInt64Ty(), /*isSigned=*/false);
    Attr = Builder.CreateIntCast(Attr, Builder.getInt32Ty(), /*isSigned=*/false);

    llvm::Function *F = CGM.getIntrinsic(llvm::Intrinsic::linx_vblock_launch);
    return Builder.CreateCall(F, {VKind, Body, Dim0, Dim1, Dim2, Attr});
  }
  case LinxISA::BI__builtin_linx_vpar_tadd: {
    llvm::Value *A = EmitScalarExpr(E->getArg(0));
    llvm::Value *B = EmitScalarExpr(E->getArg(1));
    llvm::Value *Size = EmitScalarExpr(E->getArg(2));
    Size = Builder.CreateIntCast(Size, Builder.getInt32Ty(), /*isSigned=*/false);

    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_vpar_tadd, {A->getType()});
    return Builder.CreateCall(F, {A, B, Size}, "linx.vpar.tadd");
  }
  case LinxISA::BI__builtin_linx_vpar_tsub: {
    llvm::Value *A = EmitScalarExpr(E->getArg(0));
    llvm::Value *B = EmitScalarExpr(E->getArg(1));
    llvm::Value *Size = EmitScalarExpr(E->getArg(2));
    Size = Builder.CreateIntCast(Size, Builder.getInt32Ty(), /*isSigned=*/false);

    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_vpar_tsub, {A->getType()});
    return Builder.CreateCall(F, {A, B, Size}, "linx.vpar.tsub");
  }
  default:
    break;
  }

  return nullptr;
}
