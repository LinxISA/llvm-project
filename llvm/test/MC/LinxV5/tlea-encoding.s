# RUN: llvm-mc -triple=linx64v5 --show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=DIS

# TEPL Mode 1 / Function 14 is selector 0x02e.  The source DataType owns
# signed extension; the destination type is the corresponding S64/U64.
# CHECK: BSTART.TEPL TLEA, S32
# CHECK-SAME: encoding: [0x81,0x91,0xe1,0x8a]
# CHECK: BSTART.TEPL TLEA, U32
# CHECK-SAME: encoding: [0x81,0x91,0xe1,0xca]
# CHECK: BSTART.TEPL TLEA, S64
# CHECK-SAME: encoding: [0x81,0x91,0xe1,0x82]
# CHECK: BSTART.TEPL TLEA, U64
# CHECK-SAME: encoding: [0x81,0x91,0xe1,0xc2]

# DIS: BSTART.TEPL TLEA, S32
# DIS: BSTART.TEPL TLEA, U32
# DIS: BSTART.TEPL TLEA, S64
# DIS: BSTART.TEPL TLEA, U64

BSTART.TEPL TLEA, S32
BSTART.TEPL TLEA, U32
BSTART.TEPL TLEA, S64
BSTART.TEPL TLEA, U64
