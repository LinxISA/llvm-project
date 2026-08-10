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
  auto castToI32 = [&](llvm::Value *V) {
    return Builder.CreateIntCast(V, Builder.getInt32Ty(), /*isSigned=*/false);
  };
  auto castToI64 = [&](llvm::Value *V) {
    return Builder.CreateIntCast(V, Builder.getInt64Ty(), /*isSigned=*/false);
  };

  switch (BuiltinID) {
  case LinxISA::BI__builtin_linx_tile_tload: {
    llvm::Value *Base = EmitScalarExpr(E->getArg(0));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(1)));
    llvm::Value *DType = castToI32(EmitScalarExpr(E->getArg(2)));
    llvm::Value *Layout = castToI32(EmitScalarExpr(E->getArg(3)));
    llvm::Value *LB0 = castToI64(EmitScalarExpr(E->getArg(4)));
    llvm::Value *LB1 = castToI64(EmitScalarExpr(E->getArg(5)));
    llvm::Value *LB2 = castToI64(EmitScalarExpr(E->getArg(6)));
    llvm::Value *Stride = castToI64(EmitScalarExpr(E->getArg(7)));

    llvm::Type *TileTy = ConvertType(E->getType());
    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_tlsu_tload_shape, {TileTy});
    return Builder.CreateCall(
        F, {Base, DType, Layout, LB0, LB1, LB2, Size, Stride},
        "linx.tile.tload");
  }
  case LinxISA::BI__builtin_linx_tile_tstore: {
    llvm::Value *Base = EmitScalarExpr(E->getArg(0));
    llvm::Value *Tile = EmitScalarExpr(E->getArg(1));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(2)));
    llvm::Value *DType = castToI32(EmitScalarExpr(E->getArg(3)));
    llvm::Value *Layout = castToI32(EmitScalarExpr(E->getArg(4)));
    llvm::Value *LB0 = castToI64(EmitScalarExpr(E->getArg(5)));
    llvm::Value *LB1 = castToI64(EmitScalarExpr(E->getArg(6)));
    llvm::Value *LB2 = castToI64(EmitScalarExpr(E->getArg(7)));
    llvm::Value *Stride = castToI64(EmitScalarExpr(E->getArg(8)));

    llvm::Function *F = CGM.getIntrinsic(
        llvm::Intrinsic::linx_tlsu_tstore_shape, {Tile->getType()});
    return Builder.CreateCall(
        F, {Base, Tile, DType, Layout, LB0, LB1, LB2, Size, Stride});
  }
  case LinxISA::BI__builtin_linx_tile_tmov: {
    llvm::Value *Src = EmitScalarExpr(E->getArg(0));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(1)));
    llvm::Value *DType = castToI32(EmitScalarExpr(E->getArg(2)));
    llvm::Value *Layout = castToI64(EmitScalarExpr(E->getArg(3)));
    llvm::Value *HasLayout = EmitScalarExpr(E->getArg(4));
    if (HasLayout->getType() != Builder.getInt1Ty())
      HasLayout = Builder.CreateIntCast(HasLayout, Builder.getInt1Ty(), false);

    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_tile_tmov_vec, {Src->getType()});
    return Builder.CreateCall(F, {Src, Size, DType, Layout, HasLayout},
                              "linx.tile.tmov");
  }
  case LinxISA::BI__builtin_linx_cube_tmatmul: {
    llvm::Value *A = EmitScalarExpr(E->getArg(0));
    llvm::Value *B = EmitScalarExpr(E->getArg(1));
    llvm::Value *M = castToI32(EmitScalarExpr(E->getArg(2)));
    llvm::Value *N = castToI32(EmitScalarExpr(E->getArg(3)));
    llvm::Value *K = castToI32(EmitScalarExpr(E->getArg(4)));

    llvm::Function *F = CGM.getIntrinsic(llvm::Intrinsic::linx_cube_tmatmul_vec,
                                         {A->getType()});
    return Builder.CreateCall(F, {A, B, M, N, K}, "linx.tmatmul");
  }
  case LinxISA::BI__builtin_linx_cube_tmatmul_acc: {
    llvm::Value *Acc = EmitScalarExpr(E->getArg(0));
    llvm::Value *A = EmitScalarExpr(E->getArg(1));
    llvm::Value *B = EmitScalarExpr(E->getArg(2));
    llvm::Value *M = castToI32(EmitScalarExpr(E->getArg(3)));
    llvm::Value *N = castToI32(EmitScalarExpr(E->getArg(4)));
    llvm::Value *K = castToI32(EmitScalarExpr(E->getArg(5)));

    llvm::Function *F = CGM.getIntrinsic(
        llvm::Intrinsic::linx_cube_tmatmul_acc_vec, {Acc->getType()});
    return Builder.CreateCall(F, {Acc, A, B, M, N, K}, "linx.tmatmul.acc");
  }
  case LinxISA::BI__builtin_linx_tileop_unary: {
    llvm::Value *Src = EmitScalarExpr(E->getArg(0));
    llvm::Value *TileOp = castToI32(EmitScalarExpr(E->getArg(1)));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(2)));
    llvm::Value *DType = castToI32(EmitScalarExpr(E->getArg(3)));
    llvm::Value *ValidCol = castToI64(EmitScalarExpr(E->getArg(4)));
    llvm::Value *ValidRow = castToI64(EmitScalarExpr(E->getArg(5)));
    llvm::Value *PhysicalCol = castToI64(EmitScalarExpr(E->getArg(6)));

    llvm::Function *F = CGM.getIntrinsic(
        llvm::Intrinsic::linx_tileop_unary_shape, {Src->getType()});
    return Builder.CreateCall(
        F, {Src, TileOp, Size, DType, ValidCol, ValidRow, PhysicalCol},
        "linx.tileop.u");
  }
  case LinxISA::BI__builtin_linx_tileop_binary: {
    llvm::Value *A = EmitScalarExpr(E->getArg(0));
    llvm::Value *B = EmitScalarExpr(E->getArg(1));
    llvm::Value *TileOp = castToI32(EmitScalarExpr(E->getArg(2)));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(3)));
    llvm::Value *DType = castToI32(EmitScalarExpr(E->getArg(4)));
    llvm::Value *ValidCol = castToI64(EmitScalarExpr(E->getArg(5)));
    llvm::Value *ValidRow = castToI64(EmitScalarExpr(E->getArg(6)));
    llvm::Value *PhysicalCol = castToI64(EmitScalarExpr(E->getArg(7)));

    llvm::Function *F = CGM.getIntrinsic(
        llvm::Intrinsic::linx_tileop_binary_shape, {A->getType()});
    return Builder.CreateCall(
        F, {A, B, TileOp, Size, DType, ValidCol, ValidRow, PhysicalCol},
        "linx.tileop.b");
  }
  case LinxISA::BI__builtin_linx_tileop_binary_scalar: {
    llvm::Value *A = EmitScalarExpr(E->getArg(0));
    llvm::Value *Scalar = castToI64(EmitScalarExpr(E->getArg(1)));
    llvm::Value *TileOp = castToI32(EmitScalarExpr(E->getArg(2)));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(3)));
    llvm::Value *DType = castToI32(EmitScalarExpr(E->getArg(4)));
    llvm::Value *Mode = castToI32(EmitScalarExpr(E->getArg(5)));
    llvm::Value *ValidCol = castToI64(EmitScalarExpr(E->getArg(6)));
    llvm::Value *ValidRow = castToI64(EmitScalarExpr(E->getArg(7)));
    llvm::Value *PhysicalCol = castToI64(EmitScalarExpr(E->getArg(8)));

    llvm::Function *F = CGM.getIntrinsic(
        llvm::Intrinsic::linx_tileop_binary_scalar_shape, {A->getType()});
    return Builder.CreateCall(
        F,
        {A, Scalar, TileOp, Size, DType, Mode, ValidCol, ValidRow, PhysicalCol},
        "linx.tileop.bs");
  }
  case LinxISA::BI__builtin_linx_tileop_splat: {
    llvm::Value *Scalar = castToI64(EmitScalarExpr(E->getArg(0)));
    llvm::Value *TileOp = castToI32(EmitScalarExpr(E->getArg(1)));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(2)));
    llvm::Value *DType = castToI32(EmitScalarExpr(E->getArg(3)));
    llvm::Value *Mode = castToI32(EmitScalarExpr(E->getArg(4)));
    llvm::Value *ValidCol = castToI64(EmitScalarExpr(E->getArg(5)));
    llvm::Value *ValidRow = castToI64(EmitScalarExpr(E->getArg(6)));
    llvm::Value *PhysicalCol = castToI64(EmitScalarExpr(E->getArg(7)));

    llvm::Type *TileTy = ConvertType(E->getType());
    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_tileop_splat_shape, {TileTy});
    return Builder.CreateCall(
        F, {Scalar, TileOp, Size, DType, Mode, ValidCol, ValidRow, PhysicalCol},
        "linx.tileop.splat");
  }
  case LinxISA::BI__builtin_linx_vblock_launch: {
    llvm::Value *VKind = EmitScalarExpr(E->getArg(0));
    llvm::Value *Body = EmitScalarExpr(E->getArg(1));
    llvm::Value *Dim0 = EmitScalarExpr(E->getArg(2));
    llvm::Value *Dim1 = EmitScalarExpr(E->getArg(3));
    llvm::Value *Dim2 = EmitScalarExpr(E->getArg(4));
    llvm::Value *Attr = EmitScalarExpr(E->getArg(5));

    VKind = castToI32(VKind);
    Dim0 = castToI64(Dim0);
    Dim1 = castToI64(Dim1);
    Dim2 = castToI64(Dim2);
    Attr = castToI32(Attr);

    llvm::Function *F = CGM.getIntrinsic(llvm::Intrinsic::linx_vblock_launch);
    llvm::Value *Z = Builder.getInt64(0);
    return Builder.CreateCall(F, {VKind, Body, Dim0, Dim1, Dim2, Attr, Z, Z, Z,
                                  Z, Z, Z, Z, Z, Z, Z, Z, Z});
  }
  case LinxISA::BI__builtin_linx_vpar_tadd: {
    llvm::Value *A = EmitScalarExpr(E->getArg(0));
    llvm::Value *B = EmitScalarExpr(E->getArg(1));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(2)));

    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_vpar_tadd_vec, {A->getType()});
    return Builder.CreateCall(F, {A, B, Size}, "linx.vpar.tadd");
  }
  case LinxISA::BI__builtin_linx_vpar_tsub: {
    llvm::Value *A = EmitScalarExpr(E->getArg(0));
    llvm::Value *B = EmitScalarExpr(E->getArg(1));
    llvm::Value *Size = castToI32(EmitScalarExpr(E->getArg(2)));

    llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::linx_vpar_tsub_vec, {A->getType()});
    return Builder.CreateCall(F, {A, B, Size}, "linx.vpar.tsub");
  }
  default:
    break;
  }

  return nullptr;
}
