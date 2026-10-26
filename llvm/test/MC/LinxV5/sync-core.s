# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=DIS

BSTART.SYS FALL
SYNC.CORE
BSTOP

# ENC: BSTART.SYS FALL{{.*}}encoding: [0x81,0x10,0x00,0x00]
# ENC: SYNC.CORE{{.*}}encoding: [0x2b,0xa0,0x30,0x03]
# ENC: BSTOP

# DIS: BSTART.AUX FALL
# DIS-NEXT: SYNC.CORE
# DIS-NEXT: BSTOP
