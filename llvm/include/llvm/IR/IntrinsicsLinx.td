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
  // PTO/Linx 0.58 tile block intrinsics.
  //
  // Preferred PTO 0.58 frontend surface:
  //   llvm.linx.tile.tload/tstore/tmov
  //
  // Type contract:
  // - Frontends should use: %linx.tile = target("linx.tile")
  // -------------------------------------------------------------------------

  // %linx.tile @llvm.linx.tile.tload(ptr base, i32 immTSize, i32 immDType,
  //                                  i64 immLayout, i64 immLB0, i64 immLB1,
  //                                  i64 immStrideBytes)
  //
  // PTO 0.58 legality (enforced in Linx backend):
  // - tsize in [1, 10] (128B..64KB)
  // - dtype is concrete: 0..14, 16..20, or 24..28
  // - layout is assigned: 0,1,3,4,6,8,9,17,18,20..28,30
  // - lb0/lb1/stride are immediate scalar descriptor fields
  def int_linx_tile_tload :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrReadMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>, ImmArg<ArgIndex<6>>]>;

  // void @llvm.linx.tile.tstore(ptr base, %linx.tile tile, i32 immTSize,
  //                             i32 immDType, i64 immLayout, i64 immLB0,
  //                             i64 immLB1, i64 immStrideBytes)
  //
  // PTO 0.58 legality matches tile.tload: SizeCode 1..10, concrete dtype
  // 0..14/16..20/24..28, and an assigned B.DATR Layout.
  def int_linx_tile_tstore :
      Intrinsic<[],
                [llvm_ptr_ty, llvm_linx_tile_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrWriteMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>,
                 ImmArg<ArgIndex<6>>, ImmArg<ArgIndex<7>>]>;

  // %linx.tile @llvm.linx.tile.tmov(%linx.tile src, i32 immTSize,
  //                                 i32 immDType,
  //                                 i64 immLayout, i1 immHasLayout)
  //
  // PTO 0.58 legality (enforced in Linx backend):
  // - tsize in [1, 10] (128B..64KB)
  // - dtype is concrete or DTYPE_NONE=31 for source-descriptor inference
  // - has_layout=0 requires layout=0; has_layout=1 requires an assigned layout
  def int_linx_tile_tmov :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty,
                 llvm_i1_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // %linx.tile @llvm.linx.cube.tmatmul(%linx.tile a, %linx.tile b,
  //                                  i32 immM, i32 immN, i32 immK)
  //
  // M/N/K are arbitrary positive integers in [1,65535]. This bridge uses
  // CUBE_M32 for A/output and CUBE_N8 for B with S32 elements, so M <= 32.
  // Each operand and the rounded output SizeCode must fit 64 KiB Local
  // capacity; A, B, and output capacities are validated independently.
  def int_linx_cube_tmatmul :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile acc,
  //                                      %linx.tile a, %linx.tile b,
  //                                      i32 immM, i32 immN, i32 immK)
  //
  // PTO 0.58 model:
  // - the accumulator is an ordinary explicit Local tile source;
  // - the result is written to an explicit Local tile destination;
  // - destination/accumulator aliasing is legal read-old/write-new when their
  //   descriptor metadata is identical.
  // - dimensions and CUBE_M32/CUBE_N8 capacity rules match cube.tmatmul;
  //   the explicit accumulator has the same required capacity as the output.
  def int_linx_cube_tmatmul_acc :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>]>;

  // %linx.tile @llvm.linx.tileop.unary(%linx.tile src, i32 immTileOp7,
  //                                  i32 immTSize, i32 immDType)
  //
  // PTO 0.58 tile-operation surface (header-only template dispatch).
  // PTO 0.58 legality (enforced in Linx backend):
  // - tileop7 is the packed Mode/Function selector in [0, 127]
  // - tsize in [1, 10] (128B..64KB)
  // - dtype is concrete: 0..14, 16..20, or 24..28
  def int_linx_tileop_unary :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>]>;

  // %linx.tile @llvm.linx.tileop.binary(%linx.tile a, %linx.tile b,
  //                                   i32 immTileOp7, i32 immTSize,
  //                                   i32 immDType)
  def int_linx_tileop_binary :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>]>;

  // %linx.tile @llvm.linx.tileop.binary.scalar(%linx.tile a, i64 scalar,
  //                                          i32 immTileOp7, i32 immTSize,
  //                                          i32 immDType, i32 immMode)
  //
  // Scalar tile-operation mode extension for PTO 0.58:
  // - mode 1 = VS (tile,scalar)
  def int_linx_tileop_binary_scalar :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_i64_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>]>;

  // %linx.tile @llvm.linx.tileop.splat(i64 scalar, i32 immTileOp7,
  //                                  i32 immTSize, i32 immDType,
  //                                  i32 immMode)
  //
  // Scalar tile-operation mode extension for PTO 0.58:
  // - mode 2 = SV (scalar->tile splat)
  def int_linx_tileop_splat :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_i64_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // Engine-typed tile-operation wrappers:
  // - map to fixed packed Mode/Function values while keeping explicit
  //   size/dtype controls.
  //
  // %linx.tile @llvm.linx.vec.tadd(%linx.tile a, %linx.tile b,
  //                                 i32 immTSize, i32 immDType)
  //   packed Mode/Function = 0x000 (TADD)
  def int_linx_vec_tadd :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>]>;

  // %linx.tile @llvm.linx.vec.tsub(%linx.tile a, %linx.tile b,
  //                                 i32 immTSize, i32 immDType)
  //   packed Mode/Function = 0x001 (TSUB)
  def int_linx_vec_tsub :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty,
                 llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>]>;

  // %linx.tile @llvm.linx.sfu.trowmax(%linx.tile src,
  //                                    i32 immTSize, i32 immDType)
  //   packed Mode/Function = 0x041 (TROWMAX)
  def int_linx_sfu_trowmax :
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
  // `ri*` namespace via header `B.IOR` descriptors under PTO 0.58.
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
  // - immTSize uses the same 2^(TSize+6) convention as TLOAD/TSTORE.
  // -------------------------------------------------------------------------

  // %linx.tile @llvm.linx.vpar.tadd(%linx.tile a, %linx.tile b, i32 immTSize)
  def int_linx_vpar_tadd :
      Intrinsic<[llvm_linx_tile_ty],
                [llvm_linx_tile_ty, llvm_linx_tile_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;

  // %linx.tile @llvm.linx.vpar.tsub(%linx.tile a, %linx.tile b, i32 immTSize)
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

  // TLSU descriptor forms (internal clang bridge only).
  def int_linx_tlsu_tload_desc :
      Intrinsic<[llvm_anyvector_ty],
                [llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i64_ty],
                [IntrReadMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>, ImmArg<ArgIndex<1>>,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>,
                 ImmArg<ArgIndex<6>>]>;
  def int_linx_tlsu_tstore_desc :
      Intrinsic<[],
                [llvm_ptr_ty, llvm_anyvector_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty],
                [IntrWriteMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>, ImmArg<ArgIndex<6>>,
                 ImmArg<ArgIndex<7>>]>;

  // PTO 0.58 dynamic transfer shape bridges.  LB0/LB1 are the valid
  // GM rectangle and LB2 is the physical tile column count.
  def int_linx_tlsu_tload_shape :
      Intrinsic<[llvm_anyvector_ty],
                [llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i32_ty, llvm_i64_ty],
                [IntrReadMem, IntrArgMemOnly, NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<6>>]>;
  def int_linx_tlsu_tstore_shape :
      Intrinsic<[],
                [llvm_ptr_ty, llvm_anyvector_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i32_ty,
                 llvm_i64_ty],
                [IntrWriteMem, IntrArgMemOnly, NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<7>>]>;

  // Frontend vector TMOV bridge.
  def int_linx_tile_tmov_vec :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty,
                 llvm_i1_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // Frontend vector CUBE bridges.
  def int_linx_cube_tmatmul_vec :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>]>;
  def int_linx_cube_tmatmul_acc_vec :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, LLVMMatchType<0>,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>]>;
  // PTO 0.58 frontend vector bridges.  The final three operands are
  // LB0=ValidCol, LB1=ValidRow, and LB2=physical Col and may be dynamic.
  def int_linx_tileop_unary_shape :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>]>;
  def int_linx_tileop_binary_shape :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>]>;
  def int_linx_tileop_binary_scalar_shape :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, llvm_i64_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i64_ty, llvm_i64_ty,
                 llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>]>;
  def int_linx_tileop_splat_shape :
      Intrinsic<[llvm_anyvector_ty],
                [llvm_i64_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // Frontend vector VPAR bridges.
  def int_linx_vpar_tadd_vec :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;
  def int_linx_vpar_tsub_vec :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;
}
