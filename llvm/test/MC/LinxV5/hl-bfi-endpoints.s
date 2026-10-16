# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=DIS

# Physical bytes 0e fe cd 2f 8c 01 are displayed as 16-bit parcels
# `fe0e 2fcd 018c`. Their logical word is 0x018c2fcdfe0e, whose fixed bits
# satisfy (word & 0xfe00707f000f) == 0x0000204d000e.
# ENC: hl.bfi t#1, t#1, 32, 63, ->t
# ENC-SAME: encoding: [0x0e,0xfe,0xcd,0x2f,0x8c,0x01]
hl.bfi t#1, t#1, 32, 63, ->t

# DIS: hl.bfi t#1, t#1, 32, 63, ->t
