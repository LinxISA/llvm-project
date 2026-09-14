# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s
# PTO-ISA #291: for direct Local tile operations, B.DATR Layout 29 selects
# CUBE_M32 and Layout 31 selects CUBE_M16. Unlike the transport selectors
# (21..26, direction-restricted), these name the operand layout and are
# legal wherever a Local operand layout is selected. Round-trip both ways.

# CHECK: BSTART.TEPL TADD, FP32
# CHECK-NEXT: B.DATR CUBE_M32.normal, Zero
BSTART.TEPL TADD, FP32
B.DATR CUBE_M32.normal, Zero

# CHECK: BSTART.TEPL TADD, FP32
# CHECK-NEXT: B.DATR CUBE_M16.normal, Null
BSTART.TEPL TADD, FP32
B.DATR CUBE_M16.normal, Null
