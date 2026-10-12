# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=DIS

# TCMP: execution mask is source0; output a0 is bits11:7.
B.IOR [a4,zero,zero],[a0], ExecMaskPresent
# ENC: B.IOR [a4,zero,zero],[a0], ExecMaskPresent
# ENC-SAME: encoding: [0x13,0x01,0x03,0x04]
# DIS: B.IOR [a4,zero,zero],[a0], ExecMaskPresent

# TCMPS scalar +0 still occupies source0; mask remains source1.
B.IOR [zero,a4,zero],[a1], ExecMaskPresent
# ENC: B.IOR [zero,a4,zero],[a1], ExecMaskPresent
# ENC-SAME: encoding: [0x93,0x01,0x60,0x04]
# DIS: B.IOR [zero,a4,zero],[a1], ExecMaskPresent

# An aliased mask/result register is encoded without losing the input slot.
B.IOR [a4,zero,zero],[a4], ExecMaskPresent
# ENC: B.IOR [a4,zero,zero],[a4], ExecMaskPresent
# ENC-SAME: encoding: [0x13,0x03,0x03,0x04]
# DIS: B.IOR [a4,zero,zero],[a4], ExecMaskPresent
