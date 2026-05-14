//===-- LinxV5FixupKinds.h - LinxV5 Specific Fixup Entries -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV4FIXUPKINDS_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV4FIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

#undef LinxV5

namespace llvm {
namespace LinxV5 {
enum Fixups {
  // fixup_linxv5_bnext - Used to generate an R_LinxV5_BNEXT relocation type,
  // which used for bnext inst.
  fixup_linxv5_bnext = FirstTargetFixupKind,
  // fixup_linxv5_bnext_c - Used to generate an R_LinxV5_BNEXT_C relocation
  // type,
  // which used for bnext inst.
  fixup_linxv5_bnext_c,
  // fixup_linxv5_btext - Used for btext
  fixup_linxv5_btext,
  // fixup_linxv5_32_bnext - Used to generate an R_LinxV5_32_BNEXT relocation
  // type,
  // which used for bnext inst.
  fixup_linxv5_32_bnext,
  // fixup_linxv5_48_bnext - Used to generate an R_LinxV5_48_BNEXT relocation
  // type,
  // which used for bnext inst.
  fixup_linxv5_48_bnext,
  // fixup_linxv5_64_bnext - Used to generate an R_LinxV5_64_BNEXT relocation
  // type,
  // which used for bnext inst.
  fixup_linxv5_64_bnext,
  // 20-bit fixup corresponding to %tpcrel_hi(foo) for instructions like addtpc
  fixup_linxv5_tpcrel_hi20,
  // 32-bit fixup corresponding to %tpcrel_hi32(foo) for instructions like addtpc
  fixup_linxv5_tpcrel_hi32,
  // 12-bit fixup corresponding to %tpcrel_lo(foo) for instructions like addi
  fixup_linxv5_tpcrel_lo12_i,
  // 12-bit fixup corresponding to %tpcrel_lo(foo) for the L-type store
  // instructions
  fixup_linxv5_tpcrel_lo12_l,
  // 12-bit fixup corresponding to %tpcrel_lo(foo) for the S-type store
  // instructions
  fixup_linxv5_tpcrel_lo12_s,
  // fixup_linxv5_c_addpc - Used to generate an R_LinxV5_C_ADDPC relocation
  // type, which used for c.addpc inst.
  fixup_linxv5_c_addpc,
  // fixup_linxv5_addpc - Used to generate an R_LinxV5_ADDPC relocation
  // type, which used for addpc inst.
  fixup_linxv5_addpc,
  // fixup_linxv5_hlsetret - Used to generate an R_LinxV5_HLSETRET relocation
  // type, which used for addpc inst.
  fixup_linxv5_hlsetret,

  // fixup_linxv5_simt_branch - Used to generate an R_LinxV5_SIMT_BRANCH
  // relocation
  // type, which used for branch inst.
  fixup_linxv5_simt_branch,

  // fixup_linxv5_simt_branch_rc - Used to generate an R_LinxV5_SIMT_BRANCH_RC
  // relocation
  // type, which used for branch inst.
  fixup_linxv5_simt_branch_rc,
  // fixup_linxv5_simt_jump - 22bit fixup for symbol reference in the j/pc.push
  // instruction
  fixup_linxv5_simt_jump,

  fixup_linxv5_simt_tpcrel_lo12_i,
  fixup_linxv5_simt_tpcrel_lo12_l,
  fixup_linxv5_simt_tpcrel_lo12_s,
  fixup_linxv5_load_symbol,
  fixup_linxv5_store_symbol,
  fixup_linxv5_branch,
  fixup_linxv5_load_symbol_target_42,
  fixup_linxv5_store_symbol_target_42,
  fixup_linxv5_branch_22,
  fixup_linxv5_load_symbol_target_29,
  fixup_linxv5_store_symbol_target_29,
  fixup_linxv5_stack_size,

  // 8-bit fixup corresponding to R_LinxV5_ADD8 for 8-bit symbolic difference
  // paired relocations.
  fixup_linxv5_add_8,
  // 8-bit fixup corresponding to R_LinxV5_SUB8 for 8-bit symbolic difference
  // paired relocations.
  fixup_linxv5_sub_8,
  // 16-bit fixup corresponding to R_LinxV5_ADD16 for 16-bit symbolic difference
  // paired reloctions.
  fixup_linxv5_add_16,
  // 16-bit fixup corresponding to R_LinxV5_SUB16 for 16-bit symbolic difference
  // paired reloctions.
  fixup_linxv5_sub_16,
  // 32-bit fixup corresponding to R_LinxV5_ADD32 for 32-bit symbolic difference
  // paired relocations.
  fixup_linxv5_add_32,
  // 32-bit fixup corresponding to R_LinxV5_SUB32 for 32-bit symbolic difference
  // paired relocations.
  fixup_linxv5_sub_32,
  // 64-bit fixup corresponding to R_LinxV5_ADD64 for 64-bit symbolic difference
  // paired relocations.
  fixup_linxv5_add_64,
  // 64-bit fixup corresponding to R_LinxV5_SUB64 for 64-bit symbolic difference
  // paired relocations.
  fixup_linxv5_sub_64,
  // fixup_linx_relax - Used to generate an R_LinxV5_RELAX relocation type,
  // which indicates the linker may relax the instruction pair.
  fixup_linxv5_relax,
  // fixup_linxv5_align - Used to generate an R_LinxV5_ALIGN relocation type,
  // which indicates the linker should fixup the alignment after linker
  // relaxation.
  fixup_linxv5_align,
  fixup_linxv5_tprel_hi20,
  fixup_linxv5_tprel_lo12_i,
  fixup_linxv5_tprel_lo12_l,
  fixup_linxv5_tprel_lo12_s,
  // fixup_linxv5_invalid - used as a sentinel and a marker, must be last fixup
  fixup_linxv5_invalid,
  NumTargetFixupKinds = fixup_linxv5_invalid - FirstTargetFixupKind
};
} // end namespace LinxV5
} // end namespace llvm

#endif
