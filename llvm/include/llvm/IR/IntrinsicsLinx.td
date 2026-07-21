//===- IntrinsicsLinx.td - Defines Linx intrinsics --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines Linx-specific intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "linx" in {

  // -------------------------------------------------------------------------
  // Tile block intrinsics (TAU) - PR4 surface
  //
  // Preferred strict-v0.3 frontend surface:
  //   llvm.linx.tile.tload/tstore/tmov
  //
  // Type contract:
  // - Frontends should use: %linx.tile = target("linx.tile")
  // -------------------------------------------------------------------------

  // %linx.tile @llvm.linx.tile.tload(ptr base, i32 immSizeCode, i32 immDType,
  //                                  i64 immLayout, i64 immLB0, i64 immLB1,
  //                                  i64 immStrideBytes)
  //
  // Strict-v0.3 legality (enforced in Linx backend):
  // - size_code in [5, 8] (512B..4KB)
  // - dtype in u5
  // - layout/lb0/lb1/stride are immediate scalar descriptor fields
  def int_linx_tile_tload :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrReadMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>, ImmArg<ArgIndex<6>>]>;

  // void @llvm.linx.tile.tstore(ptr base, %linx.tile tile, i32 immSizeCode,
  //                             i32 immDType, i64 immLayout, i64 immLB0,
  //                             i64 immLB1, i64 immStrideBytes)
  def int_linx_tile_tstore :
      Intrinsic<[],
                [llvm_ptr_ty, llvm_linx_tile_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrWriteMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>,
                 ImmArg<ArgIndex<6>>, ImmArg<ArgIndex<7>>]>;

  // %linx.tile @llvm.linx.tile.tmov(%linx.tile src, i32 immMode,
  //                                 i32 immSizeCode, i32 immDType,
  //                                 i64 immLayout, i1 immHasLayout)
  //
  // Strict-v0.3 legality (enforced in Linx backend):
  // - mode in {0(V2V), 1(A2V)}
  // - size_code in [5, 8] (512B..4KB)
  // - dtype in u5
  // - has_layout in {0,1}
  def int_linx_tile_tmov :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i1_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>]>;

  // %linx.tile @llvm.linx.cube.mamulb(%linx.tile a, %linx.tile b,
  //                                  i32 immM, i32 immN, i32 immK)
  def int_linx_cube_mamulb :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // %linx.tile @llvm.linx.cube.mamulb.acc(%linx.tile acc,
  //                                      %linx.tile a, %linx.tile b,
  //                                      i32 immM, i32 immN, i32 immK)
  //
  // Strict-v0.3 model:
  // - CUBE matmul writes a singleton implicit accumulator (ACC) state.
  // - The explicit acc operand is SSA dependency only (no encoded ACC tile id).
  // - Frontends must chain from cube.mamulb / cube.mamulb.acc producers.
  def int_linx_cube_mamulb_acc :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>]>;

  // %linx.tile @llvm.linx.cube.acccvt(%linx.tile acc, i32 immSizeCode,
  //                                   i32 immDType, i64 immQArg0, i64 immQArg1)
  //
  // Strict-v0.3 legality (enforced in Linx backend):
  // - acc operand is SSA dependency only (no encoded ACC tile id)
  // - acc must chain from cube.mamulb / cube.mamulb.acc producer
  // - size_code in [5, 8] (512B..4KB)
  // - dtype in u5
  // - quantization args are immediate scalars; current lowering requires both
  //   to be 0 until full quant descriptor wiring lands.
  def int_linx_cube_acccvt :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // %linx.tile @llvm.linx.tepl.unary(%linx.tile src, i32 immTileOp10,
  //                                  i32 immSizeCode, i32 immDType)
  //
  // PR5 core TEPL surface (header-only template dispatch).
  // Strict-v0.3 legality (enforced in Linx backend):
  // - tileop10 in [0, 1023]
  // - size_code in [5, 8] (512B..4KB)
  // - dtype in u5
  def int_linx_tepl_unary :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>]>;

  // %linx.tile @llvm.linx.tepl.binary(%linx.tile a, %linx.tile b,
  //                                   i32 immTileOp10, i32 immSizeCode,
  //                                   i32 immDType)
  def int_linx_tepl_binary :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>]>;

  // %linx.tile @llvm.linx.tepl.binary.scalar(%linx.tile a, i64 scalar,
  //                                          i32 immTileOp10, i32 immSizeCode,
  //                                          i32 immDType, i32 immMode)
  //
  // Scalar TEPL mode extension for strict-v0.3:
  // - mode 1 = VS (tile,scalar)
  def int_linx_tepl_binary_scalar :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_i64_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>]>;

  // %linx.tile @llvm.linx.tepl.splat(i64 scalar, i32 immTileOp10,
  //                                  i32 immSizeCode, i32 immDType,
  //                                  i32 immMode)
  //
  // Scalar TEPL mode extension for strict-v0.3:
  // - mode 2 = SV (scalar->tile splat)
  def int_linx_tepl_splat :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_i64_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // Typed TEPL wrappers (PR5 follow-on):
  // - map to fixed TileOp10 values while keeping explicit size/dtype controls.
  //
  // %linx.tile @llvm.linx.tepl.tadd(%linx.tile a, %linx.tile b,
  //                                 i32 immSizeCode, i32 immDType)
  //   TileOp10 = 0x000 (TADD)
  def int_linx_tepl_tadd :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>]>;

  // %linx.tile @llvm.linx.tepl.tsub(%linx.tile a, %linx.tile b,
  //                                 i32 immSizeCode, i32 immDType)
  //   TileOp10 = 0x001 (TSUB)
  def int_linx_tepl_tsub :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>]>;

  // %linx.tile @llvm.linx.tepl.trowmax(%linx.tile src,
  //                                    i32 immSizeCode, i32 immDType)
  //   TileOp10 = 0x020 (TROWMAX)
  def int_linx_tepl_trowmax :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>]>;

  // -------------------------------------------------------------------------
  // Vector decoupled blocks (bring-up interface).
  //
  // This intrinsic acts as a compiler-facing hook to emit a decoupled block
  // header that references an out-of-line linear body via B.TEXT.
  //
  // Decoupled launch contract:
  // - Header carries only descriptors and B.TEXT body pointer.
  // - Body is linear and terminated by C.BSTOP.
  // - LB0 = lane count, LB1 = group count, LB2 = extent2.
  // - Lane replay index is lc0 + lc1 * lane_count (lb0/lb1 semantics).
  //
  // Bring-up lowering policy:
  // - vkind: 0 = MSEQ, 1 = MPAR
  // - body_sym is currently a placeholder symbol anchor; the backend emits a
  //   per-function empty body stub for now.
  // - dim0..2 and attr_bits must be constants in the initial implementation.
  // -------------------------------------------------------------------------

  // void @llvm.linx.vblock.launch(i32 vkind, ptr body_sym,
  //                               i64 dim0, i64 dim1, i64 dim2, i32 attr_bits,
  //                               i64 bind0, i64 bind1, i64 bind2,
  //                               i64 bind3, i64 bind4, i64 bind5,
  //                               i64 bind6, i64 bind7, i64 bind8,
  //                               i64 bind9, i64 bind10, i64 bind11)
  //
  // bindN are compiler-managed scalar inputs that will be bound to the SIMT
  // `ri*` namespace via header `B.IOR` descriptors in v0.3 mode.
  def int_linx_vblock_launch :
	      Intrinsic<[],
	                [llvm_i32_ty, llvm_ptr_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
	                 llvm_i32_ty,
	                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
	                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
	                 llvm_i64_ty, llvm_i64_ty],
	                [IntrNoMem, IntrHasSideEffects]>;

  // -------------------------------------------------------------------------
  // Tile vector elementwise ops (VPAR)
  //
  // Bring-up contract:
  // - Executed as a decoupled BSTART.VPAR header that replays a single-lane
  //   vector body over LB0/LB1 dimensions.
  // - Tile operands are bound via B.IOT descriptors and accessed through TA/TB/TO.
  // - immSizeCode uses the same 2^(SizeCode+4) convention as TLOAD/TSTORE.
  // -------------------------------------------------------------------------

  // %linx.tile @llvm.linx.vpar.tadd(%linx.tile a, %linx.tile b, i32 immSizeCode)
  def int_linx_vpar_tadd :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;

  // %linx.tile @llvm.linx.vpar.tsub(%linx.tile a, %linx.tile b, i32 immSizeCode)
  def int_linx_vpar_tsub :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;

  // -------------------------------------------------------------------------
  // Legacy vector bridge intrinsics (clang builtin compatibility).
  //
  // These keep the C/C++ builtin surface usable while the frontend migrates
  // away from `_Vector<1024, int>` tile placeholders.
  // -------------------------------------------------------------------------

  // Legacy TMA descriptor forms (internal clang bridge only).
  def int_linx_tma_tload_desc :
      Intrinsic<[llvm_anyvector_ty],
                [llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i64_ty],
                [IntrReadMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>, ImmArg<ArgIndex<1>>,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>,
                 ImmArg<ArgIndex<6>>]>;
  def int_linx_tma_tstore_desc :
      Intrinsic<[],
                [llvm_ptr_ty, llvm_anyvector_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty],
                [IntrWriteMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>, ImmArg<ArgIndex<6>>,
                 ImmArg<ArgIndex<7>>]>;

  // Canonical v0.57 dynamic transfer shape bridges.  LB0/LB1 are the valid
  // GM rectangle and LB2 is the physical tile column count.
  def int_linx_tma_tload_shape :
      Intrinsic<[llvm_anyvector_ty],
                [llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i32_ty, llvm_i64_ty],
                [IntrReadMem, IntrArgMemOnly, NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<6>>]>;
  def int_linx_tma_tstore_shape :
      Intrinsic<[],
                [llvm_ptr_ty, llvm_anyvector_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i32_ty,
                 llvm_i64_ty],
                [IntrWriteMem, IntrArgMemOnly, NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<7>>]>;

  // Legacy vector TMOV bridge.
  def int_linx_tile_tmov_legacy :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i1_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>]>;

  // Legacy CUBE forms.
  def int_linx_cube_mamulb_legacy :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>]>;
  def int_linx_cube_mamulb_acc_legacy :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, LLVMMatchType<0>,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>]>;
  def int_linx_cube_acccvt_legacy :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty,
                 llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // Canonical v0.57 frontend vector bridges.  The final three operands are
  // LB0=ValidCol, LB1=ValidRow, and LB2=physical Col and may be dynamic.
  def int_linx_tepl_unary_shape :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>]>;
  def int_linx_tepl_binary_shape :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>]>;
  def int_linx_tepl_binary_scalar_shape :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, llvm_i64_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i64_ty, llvm_i64_ty,
                 llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>]>;
  def int_linx_tepl_splat_shape :
      Intrinsic<[llvm_anyvector_ty],
                [llvm_i64_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // Legacy vector VPAR forms.
  def int_linx_vpar_tadd_legacy :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;
  def int_linx_vpar_tsub_legacy :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;
}
