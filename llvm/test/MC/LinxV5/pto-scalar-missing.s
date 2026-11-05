# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=DIS

# PTO-ISA/pto-spec 2209562859a89be80b73ea926f5d60b9d018faae.
# Expected bytes are constructed from owning ASL field pieces and mask/match.
# Short prefetch signed boundaries, source modifiers, BWT and exact SYNC.CORE.

bwt a0
# ENC: bwt{{.*}}encoding: [0x2b,0x00,0x31,0x00]
# DIS: bwt a0

bwt u#4
# ENC: bwt{{.*}}encoding: [0x2b,0x80,0x3f,0x00]
# DIS: bwt u#4

sync.core
# ENC: sync.core{{.*}}encoding: [0x2b,0xa0,0x30,0x03]
# DIS: sync.core

prf [a0, a1]
# ENC: prf{{.*}}encoding: [0x09,0x70,0x31,0x00]
# DIS: prf [a0, a1]

prf [t#1, u#1.sw<<31]
# ENC: prf{{.*}}encoding: [0x09,0x70,0xcc,0xfb]
# DIS: prf [t#1, u#1.sw<<31]

prf [a0, a1.uw<<1]
# ENC: prf{{.*}}encoding: [0x09,0x70,0x31,0x0c]
# DIS: prf [a0, a1.uw<<1]

prfi.u [a0, -2048]
# ENC: prfi.u{{.*}}encoding: [0x29,0x70,0x01,0x80]
# DIS: prfi.u [a0, -2048]

prfi.u [u#4, 2047]
# ENC: prfi.u{{.*}}encoding: [0x29,0xf0,0xff,0x7f]
# DIS: prfi.u [u#4, 2047]
