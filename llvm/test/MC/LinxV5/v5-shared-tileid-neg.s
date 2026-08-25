# RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %S/v5-shared-tileid-raw.s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=RAW
#
# PTO-ISA ADR-0097: SharedTileID is six bits (S0..S63). Bits 27:26 are
# reserved and must remain zero in the B.IOS encoding.

B.IOS S64, mask=1111
B.IOS S255, mask=1111

# ASM: SharedTileID must be in the range S0..S63
# ASM: SharedTileID must be in the range S0..S63

# RAW: B.IOS{{.*}}S0, mask=1111
# RAW: <unknown>
