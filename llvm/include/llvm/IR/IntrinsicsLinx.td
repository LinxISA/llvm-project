//===------ IntrinsicsLinx.td - Defines Linx intrinsics ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the Linx-specific intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "linx" in {
def int_linx_hyper_region_start : Intrinsic<[], [llvm_i64_ty], [IntrHasSideEffects]>;

def int_linx_hyper_region_end : Intrinsic<[],[llvm_i64_ty], [IntrHasSideEffects]>;

def int_linx_get_sysreg : DefaultAttrsIntrinsic<[llvm_i64_ty],
                                                [llvm_i64_ty],
                                                [IntrHasSideEffects,]>;

def int_linx_get_thread_id : Intrinsic<[llvm_i64_ty], [], [IntrNoMem]>;
def int_linx_get_thread_idx : Intrinsic<[llvm_i64_ty], [], [IntrNoMem]>;

def int_linx_v5_gmov : Intrinsic<[llvm_anyvector_ty],
    [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, LLVMMatchType<0>],
    [IntrHasSideEffects]>;
def int_linx_v5_shared_l2s_insert : Intrinsic<[llvm_i64_ty],
    [llvm_i64_ty, llvm_i64_ty, llvm_anyvector_ty],
    [IntrHasSideEffects]>;
def int_linx_v5_shared_l2s_publish : Intrinsic<[llvm_i64_ty],
    [llvm_i64_ty, llvm_i64_ty, llvm_anyvector_ty],
    [IntrHasSideEffects]>;
def int_linx_v5_shared_s2l_broadcast : Intrinsic<[llvm_anyvector_ty],
    [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>;
def int_linx_v5_shared_s2l_extract : Intrinsic<[llvm_anyvector_ty],
    [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], [IntrHasSideEffects]>;

def int_linx_set_loop_iterations : Intrinsic<[],
                                             [llvm_i64_ty, llvm_anyint_ty],
                                             [IntrHasSideEffects,]>;

def int_linx_reduce_add : Intrinsic<[],
                                    [llvm_i64_ty, llvm_any_ty],
                                    [IntrHasSideEffects,]>;

def int_linx_reduce_max : Intrinsic<[],
                                    [llvm_i64_ty, llvm_any_ty],
                                    [IntrHasSideEffects,]>;

def int_linx_reduce_min : Intrinsic<[],
                                    [llvm_i64_ty, llvm_any_ty],
                                    [IntrHasSideEffects,]>;

def int_linx_get_simt_ret : Intrinsic<[llvm_any_ty],
                                      [llvm_i64_ty],
                                      [IntrHasSideEffects,]>;

def int_linx_shuffle_up : Intrinsic<[llvm_any_ty],
                                    [LLVMMatchType<0>, llvm_anyint_ty, llvm_anyint_ty],
                                    [IntrHasSideEffects,]>;

def int_linx_shuffle_down : Intrinsic<[llvm_any_ty],
                                      [LLVMMatchType<0>, llvm_anyint_ty, llvm_anyint_ty],
                                      [IntrHasSideEffects,]>;

def int_linx_shuffle_idx : Intrinsic<[llvm_any_ty],
                                     [LLVMMatchType<0>, llvm_anyint_ty, llvm_anyint_ty],
                                     [IntrHasSideEffects,]>;

def int_linx_shuffle_xor : Intrinsic<[llvm_any_ty],
                                     [LLVMMatchType<0>, llvm_anyint_ty, llvm_anyint_ty],
                                     [IntrHasSideEffects,]>;

def int_linx_shuffle_bfly : Intrinsic<[llvm_any_ty],
                                     [LLVMMatchType<0>, llvm_any_ty],
                                     [IntrHasSideEffects,]>;

def int_linx_blkv_max : Intrinsic<[llvm_any_ty], [LLVMMatchType<0>, LLVMMatchType<0>], [IntrHasSideEffects]>;

def int_linx_blkv_min : Intrinsic<[llvm_any_ty], [LLVMMatchType<0>, LLVMMatchType<0>], [IntrHasSideEffects]>;

def int_linx_blkv_rdmax : Intrinsic<[llvm_any_ty], [LLVMMatchType<0>], [IntrHasSideEffects]>;

def int_linx_blkv_rdmin : Intrinsic<[llvm_any_ty], [LLVMMatchType<0>], [IntrHasSideEffects]>;

def int_linx_blkv_rdadd : Intrinsic<[llvm_any_ty], [LLVMMatchType<0>], [IntrHasSideEffects]>;

def int_linx_blkv_rdor : Intrinsic<[llvm_any_ty], [LLVMMatchType<0>], [IntrHasSideEffects]>;

def int_linx_blk_matmul : Intrinsic<[llvm_anyvector_ty],
                                    [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
                                     llvm_anyvector_ty, llvm_anyvector_ty],
                                    [IntrHasSideEffects]>;

// v5 Shared-Right TMATMUL: same CUBE op as blk_matmul, but the Right (B)
// operand is a Shared tile consumed via a C.B.IOS binder rather than a B.IOT
// source stream. The B operand is therefore an opaque i64 Shared SSA handle
// (NOT a tile vector), so it must not enter the vector OverloadType list. A
// stays a normal Local tile vector; the result is an ordinary Local tile.
def int_linx_blk_matmul_shared : Intrinsic<[llvm_anyvector_ty],
    [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
     llvm_anyvector_ty, llvm_i64_ty],
    [IntrHasSideEffects]>;

def int_linx_blk_matmul_ac : Intrinsic<[llvm_anyvector_ty],
                                    [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
                                     llvm_anyvector_ty, llvm_anyvector_ty, llvm_anyvector_ty],
                                    [IntrHasSideEffects]>;

def int_linx_blk_matmulmx : Intrinsic<[llvm_anyvector_ty],
                                      [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
                                       llvm_i64_ty, llvm_i64_ty,
                                       llvm_anyvector_ty, llvm_anyvector_ty,
                                       llvm_anyvector_ty, llvm_anyvector_ty],
                                      [IntrHasSideEffects]>;

def int_linx_blk_matmulmxb : Intrinsic<[llvm_anyvector_ty],
                                       [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
                                        llvm_i64_ty, llvm_i64_ty,
                                        llvm_anyvector_ty, llvm_anyvector_ty,
                                        llvm_anyvector_ty],
                                       [IntrHasSideEffects]>;

def int_linx_blk_matmulmx_ac : Intrinsic<[llvm_anyvector_ty],
                                         [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
                                          llvm_i64_ty, llvm_i64_ty,
                                          llvm_anyvector_ty, llvm_anyvector_ty,
                                          llvm_anyvector_ty, llvm_anyvector_ty,
                                          llvm_anyvector_ty],
                                         [IntrHasSideEffects]>;

def int_linx_blk_matmulmxb_ac : Intrinsic<[llvm_anyvector_ty],
                                          [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty,
                                           llvm_i64_ty, llvm_i64_ty,
                                           llvm_anyvector_ty, llvm_anyvector_ty,
                                           llvm_anyvector_ty, llvm_anyvector_ty],
                                          [IntrHasSideEffects]>;

def int_linx_blk_tload : Intrinsic<[llvm_anyvector_ty], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_ptr_ty, llvm_i64_ty], [IntrHasSideEffects]>;

def int_linx_blk_tstore : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_ptr_ty, llvm_i64_ty, llvm_anyvector_ty], [IntrHasSideEffects]>;

def int_linx_blk_acccvt : Intrinsic<[llvm_anyvector_ty], [llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty, llvm_anyvector_ty], [IntrHasSideEffects]>;

def int_linx_blkv_fabs : Intrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrHasSideEffects]>;

def int_linx_blkv_fsqrt : Intrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrHasSideEffects]>;

def int_linx_blkv_fexp : Intrinsic<[llvm_anyfloat_ty], [LLVMMatchType<0>], [IntrHasSideEffects]>;

// Experimental compiler semantic operations for aligned Tile partitions and
// linear assembly sessions. These deliberately carry byte capacities rather
// than PTO SizeCodes so the IR contract does not depend on the unresolved
// Local B.IOT boundary. They are verifier-only until the architecture contract
// is finalized and therefore have no target lowering yet.
def int_linx_experimental_tpartition : Intrinsic<[llvm_token_ty],
    [llvm_anyvector_ty, llvm_i32_ty, llvm_i32_ty, llvm_i64_ty, llvm_i64_ty,
     llvm_i32_ty],
    [IntrHasSideEffects, IntrConvergent, IntrNoDuplicate,
     ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>,
     ImmArg<ArgIndex<4>>, ImmArg<ArgIndex<5>>]>;

def int_linx_experimental_tile_array_get : Intrinsic<[llvm_token_ty],
    [llvm_token_ty, llvm_i32_ty, llvm_i32_ty],
    [IntrHasSideEffects, IntrConvergent, IntrNoDuplicate]>;

def int_linx_experimental_tile_array_empty : Intrinsic<[llvm_token_ty],
    [llvm_i32_ty, llvm_i32_ty, llvm_i64_ty, llvm_i64_ty, llvm_i32_ty],
    [IntrHasSideEffects, IntrConvergent, IntrNoDuplicate,
     ImmArg<ArgIndex<0>>, ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>,
     ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

def int_linx_experimental_tile_array_insert : Intrinsic<[llvm_token_ty],
    [llvm_token_ty, llvm_anyvector_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
    [IntrHasSideEffects, IntrConvergent, IntrNoDuplicate,
     ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;

def int_linx_experimental_tassembly : Intrinsic<[llvm_anyvector_ty],
    [llvm_token_ty, llvm_i64_ty],
    [IntrHasSideEffects, IntrConvergent, IntrNoDuplicate,
     ImmArg<ArgIndex<1>>]>;
}

class Repeat<int num, LLVMType Ty> {
  list<LLVMType> val = !if(
    !eq(num, 0),
    [],
    !listconcat(Repeat<!sub(num, 1), Ty>.val, [Ty])
  );
}

class TileCallRetsBuilder<int onum> {
  list<LLVMType> RetTypes = Repeat<onum, llvm_anyvector_ty>.val;
}

class TileCallArgsBuilder<int inum> {
  list<LLVMType> VecTypes = Repeat<inum, llvm_anyvector_ty>.val;
  list<LLVMType> ArgTypes = !listconcat([llvm_ptr_ty, llvm_i64_ty, llvm_i64_ty, llvm_i64_ty], VecTypes, [llvm_vararg_ty]);
}

multiclass IntTileCall<int onum, int inum> {
  def _# onum # d # inum # u : Intrinsic<
      TileCallRetsBuilder<onum>.RetTypes,
      TileCallArgsBuilder<inum>.ArgTypes,
      [IntrHasSideEffects]>;
}

foreach onum = [0, 1, 2, 3, 4, 5, 6, 7, 8] in
foreach inum = [0, 1, 2, 3, 4, 5, 6, 7, 8] in {
let TargetPrefix = "linx" in {
defm int_linx_vcall_par : IntTileCall<onum, inum>;
defm int_linx_mcall_par : IntTileCall<onum, inum>;
}
}
