//===-- LinxISAFixupKinds.h - LinxISA Specific Fixup Entries ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_MCTARGETDESC_LINXISAFIXUPKINDS_H
#define LLVM_LIB_TARGET_LINXISA_MCTARGETDESC_LINXISAFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace LinxISA {

enum Fixups {
  FIXUP_LINX_NONE = FirstTargetFixupKind,

  // PC-relative branch immediate (simm12, scaled by 2 bytes).
  FIXUP_LINX_B12_PCREL,

  // PC-relative jump immediate (simm22, scaled by 2 bytes).
  FIXUP_LINX_J22_PCREL,

  // PC-relative compressed block-start immediate (C.BSTART simm12, scaled by 2
  // bytes).
  FIXUP_LINX_CBSTART12_PCREL,

  // PC-relative block-start immediate (simm17, scaled by 2 bytes).
  FIXUP_LINX_B17_PCREL,

  // PC-relative block text immediate (B.TEXT simm25, scaled by 2 bytes).
  FIXUP_LINX_B25_PCREL,

  // PC-relative block-start immediate targeting a PLT entry (simm17, scaled by
  // 2 bytes).
  FIXUP_LINX_B17_PLT,

  // PC-relative long block-start immediate (HL.BSTART.* simm30, in bytes,
  // instruction-aligned).
  FIXUP_LINX_HL_BSTART30_PCREL,

  // PC-relative 64-bit block-start byte delta. L.BSTART.* encodes the
  // 2-byte-scaled result in a signed split simm42 field.
  FIXUP_LINX_L_BSTART42_PCREL,

  // PC-relative compressed setret immediate (C.SETRET uimm5, scaled by 2
  // bytes).
  FIXUP_LINX_CSETRET5_PCREL,

  // PC-relative setret immediate (imm20, scaled by 2 bytes).
  FIXUP_LINX_SETRET20_PCREL,

  // PC-relative long setret immediate (HL.SETRET imm32, scaled by 2 bytes).
  FIXUP_LINX_HL_SETRET32_PCREL,

  // PC-relative 20-bit offset for ADDTPC (global address, not scaled).
  FIXUP_LINX_PCREL_HI20,

  // PC-relative 20-bit page offset for ADDTPC when materializing a GOT entry
  // address in PIC mode.
  FIXUP_LINX_GOT_HI20,

  // Absolute low 12 bits of the GOT entry address for ADDI/ADDIW uimm12 (used
  // with FIXUP_LINX_GOT_HI20).
  FIXUP_LINX_GOT_LO12,

  // Absolute low 12 bits for ADDI/ADDIW uimm12 (used with ADDTPC).
  FIXUP_LINX_LO12,

  // PC-relative loads/stores using *.PCR / HL.*.PCR encodings.
  //
  // The 32-bit forms use a signed 17-bit byte offset:
  //   - Loads: simm17 in bits [31:15]
  //   - Stores: simm split across bits [31:20] and [11:7]
  FIXUP_LINX_PCR17_LOAD,
  FIXUP_LINX_PCR17_STORE,

  // The 48-bit HL.*.PCR forms use a signed 29-bit byte offset (simm):
  //   - Loads: simm split across bits [47:31] and [15:4]
  //   - Stores: simm split across bits [47:36], [27:23], and [15:4]
  FIXUP_LINX_HL_PCR29_LOAD,
  FIXUP_LINX_HL_PCR29_STORE,

  // Marker.
  LastTargetFixupKind,
  NumTargetFixupKinds = LastTargetFixupKind - FirstTargetFixupKind
};

} // namespace LinxISA
} // namespace llvm

#endif // LLVM_LIB_TARGET_LINXISA_MCTARGETDESC_LINXISAFIXUPKINDS_H
