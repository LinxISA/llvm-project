# RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %S/v5-shared-tileid-raw.s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=RAW
#
# SharedTileID is six bits (S0..S63). Bit 26 is the source last-use marker;
# bit 27 remains reserved.

B.IOS S64, mask=1111
B.IOS S255, mask=1111

# ASM: SharedTileID must be in the range S0..S63
# ASM: SharedTileID must be in the range S0..S63

# RAW: B.IOS{{.*}}S0.reuse, mask=1111
# RAW: B.IOS{{.*}}S0, mask=1111
# RAW: <unknown>
