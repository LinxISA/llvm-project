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
  // Tile block intrinsics (TAU)
  //
  // Linx models hardware tile registers as opaque vectors.
  //
  // Bring-up convention: frontends typically use <1024 x i32> (4 KiB) as the
  // tile payload type, but TableGen does not provide a builtin llvm_v1024i32_ty
  // alias. Use llvm_anyvector_ty so the intrinsic can be called with any vector
  // type and let the backend impose target constraints.
  // The intrinsics are lowered by the Linx backend into tile blocks:
  //   BSTART.TMA / BSTART.CUBE + B.IOT/B.IOTI/B.DIM descriptors.
  // -------------------------------------------------------------------------

  // <anyvector> @llvm.linx.tma.tload(ptr base, i32 immSizeCode)
  // SizeCode convention (v0.3 bring-up): bytes = 2^(SizeCode+4). 4 KiB => SizeCode=8.
  def int_linx_tma_tload :
      Intrinsic<[llvm_anyvector_ty],
                [llvm_ptr_ty, llvm_i32_ty],
                [IntrReadMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<1>>]>;

  // void @llvm.linx.tma.tstore(ptr base, <anyvector> tile, i32 immSizeCode)
  def int_linx_tma_tstore :
      Intrinsic<[],
                [llvm_ptr_ty, llvm_anyvector_ty, llvm_i32_ty],
                [IntrWriteMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<2>>]>;

  // Descriptor-carrying staged forms (v0.3 auto-mode policy):
  //
  // - arg is encoded via B.ARG (low bits: unified format, high bits: pad mode).
  // - lb0/lb1/lb2 are encoded via B.DIM.
  // - B.IOR still binds the base GPR (and auxiliary metadata lanes can be
  //   added later without changing this intrinsic ABI).
  //
  // <anyvector> @llvm.linx.tma.tload.desc(ptr base, i32 arg, i32 lb0,
  //                                       i32 lb1, i32 lb2, i32 immSizeCode)
  def int_linx_tma_tload_desc :
      Intrinsic<[llvm_anyvector_ty],
                [llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty],
                [IntrReadMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>,
                 ImmArg<ArgIndex<5>>]>;

  // void @llvm.linx.tma.tstore.desc(ptr base, <anyvector> tile, i32 arg,
  //                                 i32 lb0, i32 lb1, i32 lb2, i32 immSizeCode)
  def int_linx_tma_tstore_desc :
      Intrinsic<[],
                [llvm_ptr_ty, llvm_anyvector_ty,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty,
                 llvm_i32_ty],
                [IntrWriteMem, IntrArgMemOnly,
                 NoCapture<ArgIndex<0>>,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
                 ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>,
                 ImmArg<ArgIndex<6>>]>;

  // <anyvector> @llvm.linx.cube.mamulb(<anyvector> a, <anyvector> b,
  //                                    i32 immM, i32 immN, i32 immK)
  def int_linx_cube_mamulb :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

  // <anyvector> @llvm.linx.cube.mamulb.acc(<anyvector> acc,
  //                                        <anyvector> a, <anyvector> b,
  //                                        i32 immM, i32 immN, i32 immK)
  //
  // Bring-up model: this updates an implicit accumulator state (as modeled by
  // the Linx emulator). The explicit acc operand is kept to preserve SSA
  // dependencies for scheduling and to match PTO's TMATMUL_ACC intent.
  def int_linx_cube_mamulb_acc :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, LLVMMatchType<0>,
                 llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects,
                 ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>]>;

  // -------------------------------------------------------------------------
  // Vector decoupled blocks (bring-up interface).
  //
  // This intrinsic acts as a compiler-facing hook to emit a decoupled block
  // header that references an out-of-line linear body via B.TEXT.
  //
  // Bring-up lowering policy:
  // - vkind: 0 = VSEQ, 1 = VPAR
  // - body_sym is currently a placeholder symbol anchor; the backend emits a
  //   per-function empty body stub for now.
  // - dim0..2 and attr_bits must be constants in the initial implementation.
  // -------------------------------------------------------------------------

  // void @llvm.linx.vblock.launch(i32 vkind, ptr body_sym,
  //                               i64 dim0, i64 dim1, i64 dim2, i32 attr_bits)
  def int_linx_vblock_launch :
      Intrinsic<[],
                [llvm_i32_ty, llvm_ptr_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
                 llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects]>;

  // -------------------------------------------------------------------------
  // Tile vector elementwise ops (VPAR)
  //
  // Bring-up contract:
  // - Executed as a decoupled BSTART.VPAR header that replays a single-lane
  //   vector body over LB0/LB1 dimensions.
  // - Tile operands are bound via B.IOTI descriptors and accessed through TA/TB/TO.
  // - immSizeCode uses the same 2^(SizeCode+4) convention as TLOAD/TSTORE.
  // -------------------------------------------------------------------------

  // <anyvector> @llvm.linx.vpar.tadd(<anyvector> a, <anyvector> b, i32 immSizeCode)
  def int_linx_vpar_tadd :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;

  // <anyvector> @llvm.linx.vpar.tsub(<anyvector> a, <anyvector> b, i32 immSizeCode)
  def int_linx_vpar_tsub :
      Intrinsic<[llvm_anyvector_ty],
                [LLVMMatchType<0>, LLVMMatchType<0>, llvm_i32_ty],
                [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<2>>]>;
}
