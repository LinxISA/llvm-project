# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | llvm-mc -triple=linx64v5 -filetype=obj -o %t.asm-roundtrip
# RUN: cmp %t %t.asm-roundtrip
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | sed -n 's/^[[:space:]]*[[:xdigit:]]\+:[[:space:]]*//p' | llvm-mc -triple=linx64v5 -filetype=obj -o %t.dis-roundtrip
# RUN: cmp %t %t.dis-roundtrip

# PTO-ISA PR #423: Function 2 TMOV Shared ND -> Local CUBE_M32/M16.
# Layout 21/22 are transport selectors; the header preserves backing dtype.
# LB0/LB1 are valid columns/rows. LB2 is omitted (effective 1); capacity
# and the inherited backing dtype determine physical columns.
BSTART.TMOV DTYPE_NONE
B.DATR ND2M32, DTYPE_NONE, Zero
B.DIM zero, 3, ->lb0
B.DIM zero, 19, ->lb1
B.IOS S0, mask=1000
B.IOT mask=1000, last, ->t<128B>

BSTART.TLSU TMOV, DTYPE_NONE
B.DATR ND2M16, DTYPE_NONE, Zero
B.DIM a0, 0, ->lb0
B.DIM a1, 0, ->lb1
B.IOS S63, mask=1111
B.SUBVIEW 0, a2, 0, 1
B.IOT mask=1111, last, ->t<128B>

BSTART.TLSU 2, DTYPE_NONE
B.DATR 21, DTYPE_NONE, Zero
B.DIM zero, 4, ->lb0
B.DIM zero, 32, ->lb1
B.IOS S1, mask=1111
B.SUBVIEW 0, zero, 2, 1
B.IOT mask=1111, last, ->t<128B>

BSTART.TMOV DTYPE_NONE
B.DATR 22, DTYPE_NONE, Zero
B.DIM zero, 4, ->lb0
B.DIM zero, 16, ->lb1
B.IOS S2, mask=1111
B.IOT mask=1111, last, ->t<128B>

# Existing forward TMOV remains legal for both selector spellings.
BSTART.TMOV U8
B.DATR M322ND, DTYPE_NONE, Zero
BSTART.TLSU 2, U16
B.DATR 25, DTYPE_NONE, Zero

# ENC: B.DATR{{.*}}ND2M32.normal, Zero{{.*}}encoding: [0xa3,0x1a,0xf0,0x01]
# ENC: B.DATR{{.*}}ND2M16.normal, Zero{{.*}}encoding: [0x23,0x1b,0xf0,0x01]
# ENC: B.DATR{{.*}}ND2M32.normal, Zero{{.*}}encoding: [0xa3,0x1a,0xf0,0x01]
# ENC: B.DATR{{.*}}ND2M16.normal, Zero{{.*}}encoding: [0x23,0x1b,0xf0,0x01]
# CHECK: BSTART.TLSU TMOV,{{.*}}DTYPE_NONE
# CHECK-NEXT: {{.*}}B.DATR{{.*}}ND2M32.normal, Zero
# CHECK: ->lb0
# CHECK: ->lb1
# CHECK-NOT: ->lb2
# CHECK: B.IOS{{.*}}S0, mask=1000
# CHECK-NEXT: {{.*}}B.IOT{{.*}}mask=1000, last, ->t<128B>
# CHECK: BSTART.TLSU TMOV,{{.*}}DTYPE_NONE
# CHECK-NEXT: {{.*}}B.DATR{{.*}}ND2M16.normal, Zero
# CHECK: B.IOS{{.*}}S63, mask=1111
# CHECK-NEXT: {{.*}}B.SUBVIEW{{.*}}0, a2, 0, 1
# CHECK-NEXT: {{.*}}B.IOT{{.*}}mask=1111, last, ->t<128B>
# CHECK: B.DATR{{.*}}ND2M32.normal, Zero
# CHECK: B.IOS{{.*}}S1, mask=1111
# CHECK-NEXT: {{.*}}B.SUBVIEW{{.*}}0, zero, 2, 1
# CHECK: B.DATR{{.*}}ND2M16.normal, Zero
# CHECK: B.IOS{{.*}}S2, mask=1111
# CHECK: B.DATR{{.*}}M322ND.normal, Zero
# CHECK: B.DATR{{.*}}M162ND.normal, Zero
