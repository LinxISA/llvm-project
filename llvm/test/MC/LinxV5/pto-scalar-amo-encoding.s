# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=DIS

# PTO-ISA/pto-spec 2209562859a89be80b73ea926f5d60b9d018faae.
# Expected bytes are constructed from owning ASL field pieces and mask/match.
# Every accepted AMO mnemonic and every assigned ordering/routing combination.

casb [a0], a1, a2, ->a3
# ENC: casb{{.*}}encoding: [0x9b,0x02,0x31,0x20]
# DIS: casb [a0], a1, a2, ->a3

casb.aq [t#1], u#1, a0, ->t
# ENC: casb.aq{{.*}}encoding: [0x9b,0x0f,0xcc,0x15]
# DIS: casb.aq [t#1], u#1, a0, ->t

casb.rl [a0], a1, a2, ->a3
# ENC: casb.rl{{.*}}encoding: [0x9b,0x02,0x31,0x22]
# DIS: casb.rl [a0], a1, a2, ->a3

casb.aqrl [t#1], u#1, a0, ->t
# ENC: casb.aqrl{{.*}}encoding: [0x9b,0x0f,0xcc,0x17]
# DIS: casb.aqrl [t#1], u#1, a0, ->t

casd [a0], a1, a2, ->a3
# ENC: casd{{.*}}encoding: [0x9b,0x32,0x31,0x20]
# DIS: casd [a0], a1, a2, ->a3

casd.aq [t#1], u#1, a0, ->t
# ENC: casd.aq{{.*}}encoding: [0x9b,0x3f,0xcc,0x15]
# DIS: casd.aq [t#1], u#1, a0, ->t

casd.rl [a0], a1, a2, ->a3
# ENC: casd.rl{{.*}}encoding: [0x9b,0x32,0x31,0x22]
# DIS: casd.rl [a0], a1, a2, ->a3

casd.aqrl [t#1], u#1, a0, ->t
# ENC: casd.aqrl{{.*}}encoding: [0x9b,0x3f,0xcc,0x17]
# DIS: casd.aqrl [t#1], u#1, a0, ->t

cash [a0], a1, a2, ->a3
# ENC: cash{{.*}}encoding: [0x9b,0x12,0x31,0x20]
# DIS: cash [a0], a1, a2, ->a3

cash.aq [t#1], u#1, a0, ->t
# ENC: cash.aq{{.*}}encoding: [0x9b,0x1f,0xcc,0x15]
# DIS: cash.aq [t#1], u#1, a0, ->t

cash.rl [a0], a1, a2, ->a3
# ENC: cash.rl{{.*}}encoding: [0x9b,0x12,0x31,0x22]
# DIS: cash.rl [a0], a1, a2, ->a3

cash.aqrl [t#1], u#1, a0, ->t
# ENC: cash.aqrl{{.*}}encoding: [0x9b,0x1f,0xcc,0x17]
# DIS: cash.aqrl [t#1], u#1, a0, ->t

casw [a0], a1, a2, ->a3
# ENC: casw{{.*}}encoding: [0x9b,0x22,0x31,0x20]
# DIS: casw [a0], a1, a2, ->a3

casw.aq [t#1], u#1, a0, ->t
# ENC: casw.aq{{.*}}encoding: [0x9b,0x2f,0xcc,0x15]
# DIS: casw.aq [t#1], u#1, a0, ->t

casw.rl [a0], a1, a2, ->a3
# ENC: casw.rl{{.*}}encoding: [0x9b,0x22,0x31,0x22]
# DIS: casw.rl [a0], a1, a2, ->a3

casw.aqrl [t#1], u#1, a0, ->t
# ENC: casw.aqrl{{.*}}encoding: [0x9b,0x2f,0xcc,0x17]
# DIS: casw.aqrl [t#1], u#1, a0, ->t

dma [a0], a1
# ENC: dma{{.*}}encoding: [0x0b,0x70,0x31,0x00]
# DIS: dma [a0], a1

hl.casb [a0], a1, a2, ->a3
# ENC: hl.casb{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x00]
# DIS: hl.casb [a0], a1, a2, ->a3

hl.casb.aq [t#1], u#1, a0, ->t
# ENC: hl.casb.aq{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x05]
# DIS: hl.casb.aq [t#1], u#1, a0, ->t

hl.casb.rl [a0], a1, a2, ->a3
# ENC: hl.casb.rl{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x02]
# DIS: hl.casb.rl [a0], a1, a2, ->a3

hl.casb.f [t#1], u#1, a0, ->t
# ENC: hl.casb.f{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x09]
# DIS: hl.casb.f [t#1], u#1, a0, ->t

hl.casb.aqrl [a0], a1, a2, ->a3
# ENC: hl.casb.aqrl{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x06]
# DIS: hl.casb.aqrl [a0], a1, a2, ->a3

hl.casb.aqf [t#1], u#1, a0, ->t
# ENC: hl.casb.aqf{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x0d]
# DIS: hl.casb.aqf [t#1], u#1, a0, ->t

hl.casb.rlf [a0], a1, a2, ->a3
# ENC: hl.casb.rlf{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x0a]
# DIS: hl.casb.rlf [a0], a1, a2, ->a3

hl.casb.aqrlf [t#1], u#1, a0, ->t
# ENC: hl.casb.aqrlf{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x0f]
# DIS: hl.casb.aqrlf [t#1], u#1, a0, ->t

hl.casd [a0], a1, a2, ->a3
# ENC: hl.casd{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x30]
# DIS: hl.casd [a0], a1, a2, ->a3

hl.casd.aq [t#1], u#1, a0, ->t
# ENC: hl.casd.aq{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x35]
# DIS: hl.casd.aq [t#1], u#1, a0, ->t

hl.casd.rl [a0], a1, a2, ->a3
# ENC: hl.casd.rl{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x32]
# DIS: hl.casd.rl [a0], a1, a2, ->a3

hl.casd.f [t#1], u#1, a0, ->t
# ENC: hl.casd.f{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x39]
# DIS: hl.casd.f [t#1], u#1, a0, ->t

hl.casd.aqrl [a0], a1, a2, ->a3
# ENC: hl.casd.aqrl{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x36]
# DIS: hl.casd.aqrl [a0], a1, a2, ->a3

hl.casd.aqf [t#1], u#1, a0, ->t
# ENC: hl.casd.aqf{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x3d]
# DIS: hl.casd.aqf [t#1], u#1, a0, ->t

hl.casd.rlf [a0], a1, a2, ->a3
# ENC: hl.casd.rlf{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x3a]
# DIS: hl.casd.rlf [a0], a1, a2, ->a3

hl.casd.aqrlf [t#1], u#1, a0, ->t
# ENC: hl.casd.aqrlf{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x3f]
# DIS: hl.casd.aqrlf [t#1], u#1, a0, ->t

hl.cash [a0], a1, a2, ->a3
# ENC: hl.cash{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x10]
# DIS: hl.cash [a0], a1, a2, ->a3

hl.cash.aq [t#1], u#1, a0, ->t
# ENC: hl.cash.aq{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x15]
# DIS: hl.cash.aq [t#1], u#1, a0, ->t

hl.cash.rl [a0], a1, a2, ->a3
# ENC: hl.cash.rl{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x12]
# DIS: hl.cash.rl [a0], a1, a2, ->a3

hl.cash.f [t#1], u#1, a0, ->t
# ENC: hl.cash.f{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x19]
# DIS: hl.cash.f [t#1], u#1, a0, ->t

hl.cash.aqrl [a0], a1, a2, ->a3
# ENC: hl.cash.aqrl{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x16]
# DIS: hl.cash.aqrl [a0], a1, a2, ->a3

hl.cash.aqf [t#1], u#1, a0, ->t
# ENC: hl.cash.aqf{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x1d]
# DIS: hl.cash.aqf [t#1], u#1, a0, ->t

hl.cash.rlf [a0], a1, a2, ->a3
# ENC: hl.cash.rlf{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x1a]
# DIS: hl.cash.rlf [a0], a1, a2, ->a3

hl.cash.aqrlf [t#1], u#1, a0, ->t
# ENC: hl.cash.aqrlf{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x1f]
# DIS: hl.cash.aqrlf [t#1], u#1, a0, ->t

hl.casw [a0], a1, a2, ->a3
# ENC: hl.casw{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x20]
# DIS: hl.casw [a0], a1, a2, ->a3

hl.casw.aq [t#1], u#1, a0, ->t
# ENC: hl.casw.aq{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x25]
# DIS: hl.casw.aq [t#1], u#1, a0, ->t

hl.casw.rl [a0], a1, a2, ->a3
# ENC: hl.casw.rl{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x22]
# DIS: hl.casw.rl [a0], a1, a2, ->a3

hl.casw.f [t#1], u#1, a0, ->t
# ENC: hl.casw.f{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x29]
# DIS: hl.casw.f [t#1], u#1, a0, ->t

hl.casw.aqrl [a0], a1, a2, ->a3
# ENC: hl.casw.aqrl{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x26]
# DIS: hl.casw.aqrl [a0], a1, a2, ->a3

hl.casw.aqf [t#1], u#1, a0, ->t
# ENC: hl.casw.aqf{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x2d]
# DIS: hl.casw.aqf [t#1], u#1, a0, ->t

hl.casw.rlf [a0], a1, a2, ->a3
# ENC: hl.casw.rlf{{.*}}encoding: [0x0e,0x01,0x8b,0x62,0x31,0x2a]
# DIS: hl.casw.rlf [a0], a1, a2, ->a3

hl.casw.aqrlf [t#1], u#1, a0, ->t
# ENC: hl.casw.aqrlf{{.*}}encoding: [0x8e,0x00,0x8b,0x6f,0xcc,0x2f]
# DIS: hl.casw.aqrlf [t#1], u#1, a0, ->t

ld.add [a0], a1, ->a3
# ENC: ld.add{{.*}}encoding: [0x8b,0x42,0x31,0x00]
# DIS: ld.add [a0], a1, ->a3

ld.add.aq [t#1], u#1, ->t
# ENC: ld.add.aq{{.*}}encoding: [0x8b,0x4f,0xcc,0x05]
# DIS: ld.add.aq [t#1], u#1, ->t

ld.add.rl [a0], a1, ->a3
# ENC: ld.add.rl{{.*}}encoding: [0x8b,0x42,0x31,0x02]
# DIS: ld.add.rl [a0], a1, ->a3

ld.add.f [t#1], u#1, ->t
# ENC: ld.add.f{{.*}}encoding: [0x8b,0x4f,0xcc,0x81]
# DIS: ld.add.f [t#1], u#1, ->t

ld.add.aqrl [a0], a1, ->a3
# ENC: ld.add.aqrl{{.*}}encoding: [0x8b,0x42,0x31,0x06]
# DIS: ld.add.aqrl [a0], a1, ->a3

ld.add.aqf [t#1], u#1, ->t
# ENC: ld.add.aqf{{.*}}encoding: [0x8b,0x4f,0xcc,0x85]
# DIS: ld.add.aqf [t#1], u#1, ->t

ld.add.rlf [a0], a1, ->a3
# ENC: ld.add.rlf{{.*}}encoding: [0x8b,0x42,0x31,0x82]
# DIS: ld.add.rlf [a0], a1, ->a3

ld.add.aqrlf [t#1], u#1, ->t
# ENC: ld.add.aqrlf{{.*}}encoding: [0x8b,0x4f,0xcc,0x87]
# DIS: ld.add.aqrlf [t#1], u#1, ->t

ld.and [a0], a1, ->a3
# ENC: ld.and{{.*}}encoding: [0x8b,0x42,0x31,0x08]
# DIS: ld.and [a0], a1, ->a3

ld.and.aq [t#1], u#1, ->t
# ENC: ld.and.aq{{.*}}encoding: [0x8b,0x4f,0xcc,0x0d]
# DIS: ld.and.aq [t#1], u#1, ->t

ld.and.rl [a0], a1, ->a3
# ENC: ld.and.rl{{.*}}encoding: [0x8b,0x42,0x31,0x0a]
# DIS: ld.and.rl [a0], a1, ->a3

ld.and.f [t#1], u#1, ->t
# ENC: ld.and.f{{.*}}encoding: [0x8b,0x4f,0xcc,0x89]
# DIS: ld.and.f [t#1], u#1, ->t

ld.and.aqrl [a0], a1, ->a3
# ENC: ld.and.aqrl{{.*}}encoding: [0x8b,0x42,0x31,0x0e]
# DIS: ld.and.aqrl [a0], a1, ->a3

ld.and.aqf [t#1], u#1, ->t
# ENC: ld.and.aqf{{.*}}encoding: [0x8b,0x4f,0xcc,0x8d]
# DIS: ld.and.aqf [t#1], u#1, ->t

ld.and.rlf [a0], a1, ->a3
# ENC: ld.and.rlf{{.*}}encoding: [0x8b,0x42,0x31,0x8a]
# DIS: ld.and.rlf [a0], a1, ->a3

ld.and.aqrlf [t#1], u#1, ->t
# ENC: ld.and.aqrlf{{.*}}encoding: [0x8b,0x4f,0xcc,0x8f]
# DIS: ld.and.aqrlf [t#1], u#1, ->t

ld.or [a0], a1, ->a3
# ENC: ld.or{{.*}}encoding: [0x8b,0x42,0x31,0x10]
# DIS: ld.or [a0], a1, ->a3

ld.or.aq [t#1], u#1, ->t
# ENC: ld.or.aq{{.*}}encoding: [0x8b,0x4f,0xcc,0x15]
# DIS: ld.or.aq [t#1], u#1, ->t

ld.or.rl [a0], a1, ->a3
# ENC: ld.or.rl{{.*}}encoding: [0x8b,0x42,0x31,0x12]
# DIS: ld.or.rl [a0], a1, ->a3

ld.or.f [t#1], u#1, ->t
# ENC: ld.or.f{{.*}}encoding: [0x8b,0x4f,0xcc,0x91]
# DIS: ld.or.f [t#1], u#1, ->t

ld.or.aqrl [a0], a1, ->a3
# ENC: ld.or.aqrl{{.*}}encoding: [0x8b,0x42,0x31,0x16]
# DIS: ld.or.aqrl [a0], a1, ->a3

ld.or.aqf [t#1], u#1, ->t
# ENC: ld.or.aqf{{.*}}encoding: [0x8b,0x4f,0xcc,0x95]
# DIS: ld.or.aqf [t#1], u#1, ->t

ld.or.rlf [a0], a1, ->a3
# ENC: ld.or.rlf{{.*}}encoding: [0x8b,0x42,0x31,0x92]
# DIS: ld.or.rlf [a0], a1, ->a3

ld.or.aqrlf [t#1], u#1, ->t
# ENC: ld.or.aqrlf{{.*}}encoding: [0x8b,0x4f,0xcc,0x97]
# DIS: ld.or.aqrlf [t#1], u#1, ->t

ld.smax [a0], a1, ->a3
# ENC: ld.smax{{.*}}encoding: [0x8b,0x42,0x31,0x20]
# DIS: ld.smax [a0], a1, ->a3

ld.smax.aq [t#1], u#1, ->t
# ENC: ld.smax.aq{{.*}}encoding: [0x8b,0x4f,0xcc,0x25]
# DIS: ld.smax.aq [t#1], u#1, ->t

ld.smax.rl [a0], a1, ->a3
# ENC: ld.smax.rl{{.*}}encoding: [0x8b,0x42,0x31,0x22]
# DIS: ld.smax.rl [a0], a1, ->a3

ld.smax.f [t#1], u#1, ->t
# ENC: ld.smax.f{{.*}}encoding: [0x8b,0x4f,0xcc,0xa1]
# DIS: ld.smax.f [t#1], u#1, ->t

ld.smax.aqrl [a0], a1, ->a3
# ENC: ld.smax.aqrl{{.*}}encoding: [0x8b,0x42,0x31,0x26]
# DIS: ld.smax.aqrl [a0], a1, ->a3

ld.smax.aqf [t#1], u#1, ->t
# ENC: ld.smax.aqf{{.*}}encoding: [0x8b,0x4f,0xcc,0xa5]
# DIS: ld.smax.aqf [t#1], u#1, ->t

ld.smax.rlf [a0], a1, ->a3
# ENC: ld.smax.rlf{{.*}}encoding: [0x8b,0x42,0x31,0xa2]
# DIS: ld.smax.rlf [a0], a1, ->a3

ld.smax.aqrlf [t#1], u#1, ->t
# ENC: ld.smax.aqrlf{{.*}}encoding: [0x8b,0x4f,0xcc,0xa7]
# DIS: ld.smax.aqrlf [t#1], u#1, ->t

ld.smin [a0], a1, ->a3
# ENC: ld.smin{{.*}}encoding: [0x8b,0x42,0x31,0x28]
# DIS: ld.smin [a0], a1, ->a3

ld.smin.aq [t#1], u#1, ->t
# ENC: ld.smin.aq{{.*}}encoding: [0x8b,0x4f,0xcc,0x2d]
# DIS: ld.smin.aq [t#1], u#1, ->t

ld.smin.rl [a0], a1, ->a3
# ENC: ld.smin.rl{{.*}}encoding: [0x8b,0x42,0x31,0x2a]
# DIS: ld.smin.rl [a0], a1, ->a3

ld.smin.f [t#1], u#1, ->t
# ENC: ld.smin.f{{.*}}encoding: [0x8b,0x4f,0xcc,0xa9]
# DIS: ld.smin.f [t#1], u#1, ->t

ld.smin.aqrl [a0], a1, ->a3
# ENC: ld.smin.aqrl{{.*}}encoding: [0x8b,0x42,0x31,0x2e]
# DIS: ld.smin.aqrl [a0], a1, ->a3

ld.smin.aqf [t#1], u#1, ->t
# ENC: ld.smin.aqf{{.*}}encoding: [0x8b,0x4f,0xcc,0xad]
# DIS: ld.smin.aqf [t#1], u#1, ->t

ld.smin.rlf [a0], a1, ->a3
# ENC: ld.smin.rlf{{.*}}encoding: [0x8b,0x42,0x31,0xaa]
# DIS: ld.smin.rlf [a0], a1, ->a3

ld.smin.aqrlf [t#1], u#1, ->t
# ENC: ld.smin.aqrlf{{.*}}encoding: [0x8b,0x4f,0xcc,0xaf]
# DIS: ld.smin.aqrlf [t#1], u#1, ->t

ld.umax [a0], a1, ->a3
# ENC: ld.umax{{.*}}encoding: [0x8b,0x42,0x31,0x30]
# DIS: ld.umax [a0], a1, ->a3

ld.umax.aq [t#1], u#1, ->t
# ENC: ld.umax.aq{{.*}}encoding: [0x8b,0x4f,0xcc,0x35]
# DIS: ld.umax.aq [t#1], u#1, ->t

ld.umax.rl [a0], a1, ->a3
# ENC: ld.umax.rl{{.*}}encoding: [0x8b,0x42,0x31,0x32]
# DIS: ld.umax.rl [a0], a1, ->a3

ld.umax.f [t#1], u#1, ->t
# ENC: ld.umax.f{{.*}}encoding: [0x8b,0x4f,0xcc,0xb1]
# DIS: ld.umax.f [t#1], u#1, ->t

ld.umax.aqrl [a0], a1, ->a3
# ENC: ld.umax.aqrl{{.*}}encoding: [0x8b,0x42,0x31,0x36]
# DIS: ld.umax.aqrl [a0], a1, ->a3

ld.umax.aqf [t#1], u#1, ->t
# ENC: ld.umax.aqf{{.*}}encoding: [0x8b,0x4f,0xcc,0xb5]
# DIS: ld.umax.aqf [t#1], u#1, ->t

ld.umax.rlf [a0], a1, ->a3
# ENC: ld.umax.rlf{{.*}}encoding: [0x8b,0x42,0x31,0xb2]
# DIS: ld.umax.rlf [a0], a1, ->a3

ld.umax.aqrlf [t#1], u#1, ->t
# ENC: ld.umax.aqrlf{{.*}}encoding: [0x8b,0x4f,0xcc,0xb7]
# DIS: ld.umax.aqrlf [t#1], u#1, ->t

ld.umin [a0], a1, ->a3
# ENC: ld.umin{{.*}}encoding: [0x8b,0x42,0x31,0x38]
# DIS: ld.umin [a0], a1, ->a3

ld.umin.aq [t#1], u#1, ->t
# ENC: ld.umin.aq{{.*}}encoding: [0x8b,0x4f,0xcc,0x3d]
# DIS: ld.umin.aq [t#1], u#1, ->t

ld.umin.rl [a0], a1, ->a3
# ENC: ld.umin.rl{{.*}}encoding: [0x8b,0x42,0x31,0x3a]
# DIS: ld.umin.rl [a0], a1, ->a3

ld.umin.f [t#1], u#1, ->t
# ENC: ld.umin.f{{.*}}encoding: [0x8b,0x4f,0xcc,0xb9]
# DIS: ld.umin.f [t#1], u#1, ->t

ld.umin.aqrl [a0], a1, ->a3
# ENC: ld.umin.aqrl{{.*}}encoding: [0x8b,0x42,0x31,0x3e]
# DIS: ld.umin.aqrl [a0], a1, ->a3

ld.umin.aqf [t#1], u#1, ->t
# ENC: ld.umin.aqf{{.*}}encoding: [0x8b,0x4f,0xcc,0xbd]
# DIS: ld.umin.aqf [t#1], u#1, ->t

ld.umin.rlf [a0], a1, ->a3
# ENC: ld.umin.rlf{{.*}}encoding: [0x8b,0x42,0x31,0xba]
# DIS: ld.umin.rlf [a0], a1, ->a3

ld.umin.aqrlf [t#1], u#1, ->t
# ENC: ld.umin.aqrlf{{.*}}encoding: [0x8b,0x4f,0xcc,0xbf]
# DIS: ld.umin.aqrlf [t#1], u#1, ->t

ld.xor [a0], a1, ->a3
# ENC: ld.xor{{.*}}encoding: [0x8b,0x42,0x31,0x18]
# DIS: ld.xor [a0], a1, ->a3

ld.xor.aq [t#1], u#1, ->t
# ENC: ld.xor.aq{{.*}}encoding: [0x8b,0x4f,0xcc,0x1d]
# DIS: ld.xor.aq [t#1], u#1, ->t

ld.xor.rl [a0], a1, ->a3
# ENC: ld.xor.rl{{.*}}encoding: [0x8b,0x42,0x31,0x1a]
# DIS: ld.xor.rl [a0], a1, ->a3

ld.xor.f [t#1], u#1, ->t
# ENC: ld.xor.f{{.*}}encoding: [0x8b,0x4f,0xcc,0x99]
# DIS: ld.xor.f [t#1], u#1, ->t

ld.xor.aqrl [a0], a1, ->a3
# ENC: ld.xor.aqrl{{.*}}encoding: [0x8b,0x42,0x31,0x1e]
# DIS: ld.xor.aqrl [a0], a1, ->a3

ld.xor.aqf [t#1], u#1, ->t
# ENC: ld.xor.aqf{{.*}}encoding: [0x8b,0x4f,0xcc,0x9d]
# DIS: ld.xor.aqf [t#1], u#1, ->t

ld.xor.rlf [a0], a1, ->a3
# ENC: ld.xor.rlf{{.*}}encoding: [0x8b,0x42,0x31,0x9a]
# DIS: ld.xor.rlf [a0], a1, ->a3

ld.xor.aqrlf [t#1], u#1, ->t
# ENC: ld.xor.aqrlf{{.*}}encoding: [0x8b,0x4f,0xcc,0x9f]
# DIS: ld.xor.aqrlf [t#1], u#1, ->t

lr.b [a0], ->a3
# ENC: lr.b{{.*}}encoding: [0x8b,0x02,0x01,0x00]
# DIS: lr.b [a0], ->a3

lr.b.aq [t#1], ->t
# ENC: lr.b.aq{{.*}}encoding: [0x8b,0x0f,0x0c,0x04]
# DIS: lr.b.aq [t#1], ->t

lr.b.rl [a0], ->a3
# ENC: lr.b.rl{{.*}}encoding: [0x8b,0x02,0x01,0x02]
# DIS: lr.b.rl [a0], ->a3

lr.b.f [t#1], ->t
# ENC: lr.b.f{{.*}}encoding: [0x8b,0x0f,0x0c,0x80]
# DIS: lr.b.f [t#1], ->t

lr.b.aqrl [a0], ->a3
# ENC: lr.b.aqrl{{.*}}encoding: [0x8b,0x02,0x01,0x06]
# DIS: lr.b.aqrl [a0], ->a3

lr.b.aqf [t#1], ->t
# ENC: lr.b.aqf{{.*}}encoding: [0x8b,0x0f,0x0c,0x84]
# DIS: lr.b.aqf [t#1], ->t

lr.b.rlf [a0], ->a3
# ENC: lr.b.rlf{{.*}}encoding: [0x8b,0x02,0x01,0x82]
# DIS: lr.b.rlf [a0], ->a3

lr.b.aqrlf [t#1], ->t
# ENC: lr.b.aqrlf{{.*}}encoding: [0x8b,0x0f,0x0c,0x86]
# DIS: lr.b.aqrlf [t#1], ->t

lr.d [a0], ->a3
# ENC: lr.d{{.*}}encoding: [0x8b,0x02,0x01,0x18]
# DIS: lr.d [a0], ->a3

lr.d.aq [t#1], ->t
# ENC: lr.d.aq{{.*}}encoding: [0x8b,0x0f,0x0c,0x1c]
# DIS: lr.d.aq [t#1], ->t

lr.d.rl [a0], ->a3
# ENC: lr.d.rl{{.*}}encoding: [0x8b,0x02,0x01,0x1a]
# DIS: lr.d.rl [a0], ->a3

lr.d.f [t#1], ->t
# ENC: lr.d.f{{.*}}encoding: [0x8b,0x0f,0x0c,0x98]
# DIS: lr.d.f [t#1], ->t

lr.d.aqrl [a0], ->a3
# ENC: lr.d.aqrl{{.*}}encoding: [0x8b,0x02,0x01,0x1e]
# DIS: lr.d.aqrl [a0], ->a3

lr.d.aqf [t#1], ->t
# ENC: lr.d.aqf{{.*}}encoding: [0x8b,0x0f,0x0c,0x9c]
# DIS: lr.d.aqf [t#1], ->t

lr.d.rlf [a0], ->a3
# ENC: lr.d.rlf{{.*}}encoding: [0x8b,0x02,0x01,0x9a]
# DIS: lr.d.rlf [a0], ->a3

lr.d.aqrlf [t#1], ->t
# ENC: lr.d.aqrlf{{.*}}encoding: [0x8b,0x0f,0x0c,0x9e]
# DIS: lr.d.aqrlf [t#1], ->t

lr.h [a0], ->a3
# ENC: lr.h{{.*}}encoding: [0x8b,0x02,0x01,0x08]
# DIS: lr.h [a0], ->a3

lr.h.aq [t#1], ->t
# ENC: lr.h.aq{{.*}}encoding: [0x8b,0x0f,0x0c,0x0c]
# DIS: lr.h.aq [t#1], ->t

lr.h.rl [a0], ->a3
# ENC: lr.h.rl{{.*}}encoding: [0x8b,0x02,0x01,0x0a]
# DIS: lr.h.rl [a0], ->a3

lr.h.f [t#1], ->t
# ENC: lr.h.f{{.*}}encoding: [0x8b,0x0f,0x0c,0x88]
# DIS: lr.h.f [t#1], ->t

lr.h.aqrl [a0], ->a3
# ENC: lr.h.aqrl{{.*}}encoding: [0x8b,0x02,0x01,0x0e]
# DIS: lr.h.aqrl [a0], ->a3

lr.h.aqf [t#1], ->t
# ENC: lr.h.aqf{{.*}}encoding: [0x8b,0x0f,0x0c,0x8c]
# DIS: lr.h.aqf [t#1], ->t

lr.h.rlf [a0], ->a3
# ENC: lr.h.rlf{{.*}}encoding: [0x8b,0x02,0x01,0x8a]
# DIS: lr.h.rlf [a0], ->a3

lr.h.aqrlf [t#1], ->t
# ENC: lr.h.aqrlf{{.*}}encoding: [0x8b,0x0f,0x0c,0x8e]
# DIS: lr.h.aqrlf [t#1], ->t

lr.w [a0], ->a3
# ENC: lr.w{{.*}}encoding: [0x8b,0x02,0x01,0x10]
# DIS: lr.w [a0], ->a3

lr.w.aq [t#1], ->t
# ENC: lr.w.aq{{.*}}encoding: [0x8b,0x0f,0x0c,0x14]
# DIS: lr.w.aq [t#1], ->t

lr.w.rl [a0], ->a3
# ENC: lr.w.rl{{.*}}encoding: [0x8b,0x02,0x01,0x12]
# DIS: lr.w.rl [a0], ->a3

lr.w.f [t#1], ->t
# ENC: lr.w.f{{.*}}encoding: [0x8b,0x0f,0x0c,0x90]
# DIS: lr.w.f [t#1], ->t

lr.w.aqrl [a0], ->a3
# ENC: lr.w.aqrl{{.*}}encoding: [0x8b,0x02,0x01,0x16]
# DIS: lr.w.aqrl [a0], ->a3

lr.w.aqf [t#1], ->t
# ENC: lr.w.aqf{{.*}}encoding: [0x8b,0x0f,0x0c,0x94]
# DIS: lr.w.aqf [t#1], ->t

lr.w.rlf [a0], ->a3
# ENC: lr.w.rlf{{.*}}encoding: [0x8b,0x02,0x01,0x92]
# DIS: lr.w.rlf [a0], ->a3

lr.w.aqrlf [t#1], ->t
# ENC: lr.w.aqrlf{{.*}}encoding: [0x8b,0x0f,0x0c,0x96]
# DIS: lr.w.aqrlf [t#1], ->t

lw.add [a0], a1, ->a3
# ENC: lw.add{{.*}}encoding: [0x8b,0x22,0x31,0x00]
# DIS: lw.add [a0], a1, ->a3

lw.add.aq [t#1], u#1, ->t
# ENC: lw.add.aq{{.*}}encoding: [0x8b,0x2f,0xcc,0x05]
# DIS: lw.add.aq [t#1], u#1, ->t

lw.add.rl [a0], a1, ->a3
# ENC: lw.add.rl{{.*}}encoding: [0x8b,0x22,0x31,0x02]
# DIS: lw.add.rl [a0], a1, ->a3

lw.add.f [t#1], u#1, ->t
# ENC: lw.add.f{{.*}}encoding: [0x8b,0x2f,0xcc,0x81]
# DIS: lw.add.f [t#1], u#1, ->t

lw.add.aqrl [a0], a1, ->a3
# ENC: lw.add.aqrl{{.*}}encoding: [0x8b,0x22,0x31,0x06]
# DIS: lw.add.aqrl [a0], a1, ->a3

lw.add.aqf [t#1], u#1, ->t
# ENC: lw.add.aqf{{.*}}encoding: [0x8b,0x2f,0xcc,0x85]
# DIS: lw.add.aqf [t#1], u#1, ->t

lw.add.rlf [a0], a1, ->a3
# ENC: lw.add.rlf{{.*}}encoding: [0x8b,0x22,0x31,0x82]
# DIS: lw.add.rlf [a0], a1, ->a3

lw.add.aqrlf [t#1], u#1, ->t
# ENC: lw.add.aqrlf{{.*}}encoding: [0x8b,0x2f,0xcc,0x87]
# DIS: lw.add.aqrlf [t#1], u#1, ->t

lw.and [a0], a1, ->a3
# ENC: lw.and{{.*}}encoding: [0x8b,0x22,0x31,0x08]
# DIS: lw.and [a0], a1, ->a3

lw.and.aq [t#1], u#1, ->t
# ENC: lw.and.aq{{.*}}encoding: [0x8b,0x2f,0xcc,0x0d]
# DIS: lw.and.aq [t#1], u#1, ->t

lw.and.rl [a0], a1, ->a3
# ENC: lw.and.rl{{.*}}encoding: [0x8b,0x22,0x31,0x0a]
# DIS: lw.and.rl [a0], a1, ->a3

lw.and.f [t#1], u#1, ->t
# ENC: lw.and.f{{.*}}encoding: [0x8b,0x2f,0xcc,0x89]
# DIS: lw.and.f [t#1], u#1, ->t

lw.and.aqrl [a0], a1, ->a3
# ENC: lw.and.aqrl{{.*}}encoding: [0x8b,0x22,0x31,0x0e]
# DIS: lw.and.aqrl [a0], a1, ->a3

lw.and.aqf [t#1], u#1, ->t
# ENC: lw.and.aqf{{.*}}encoding: [0x8b,0x2f,0xcc,0x8d]
# DIS: lw.and.aqf [t#1], u#1, ->t

lw.and.rlf [a0], a1, ->a3
# ENC: lw.and.rlf{{.*}}encoding: [0x8b,0x22,0x31,0x8a]
# DIS: lw.and.rlf [a0], a1, ->a3

lw.and.aqrlf [t#1], u#1, ->t
# ENC: lw.and.aqrlf{{.*}}encoding: [0x8b,0x2f,0xcc,0x8f]
# DIS: lw.and.aqrlf [t#1], u#1, ->t

lw.or [a0], a1, ->a3
# ENC: lw.or{{.*}}encoding: [0x8b,0x22,0x31,0x10]
# DIS: lw.or [a0], a1, ->a3

lw.or.aq [t#1], u#1, ->t
# ENC: lw.or.aq{{.*}}encoding: [0x8b,0x2f,0xcc,0x15]
# DIS: lw.or.aq [t#1], u#1, ->t

lw.or.rl [a0], a1, ->a3
# ENC: lw.or.rl{{.*}}encoding: [0x8b,0x22,0x31,0x12]
# DIS: lw.or.rl [a0], a1, ->a3

lw.or.f [t#1], u#1, ->t
# ENC: lw.or.f{{.*}}encoding: [0x8b,0x2f,0xcc,0x91]
# DIS: lw.or.f [t#1], u#1, ->t

lw.or.aqrl [a0], a1, ->a3
# ENC: lw.or.aqrl{{.*}}encoding: [0x8b,0x22,0x31,0x16]
# DIS: lw.or.aqrl [a0], a1, ->a3

lw.or.aqf [t#1], u#1, ->t
# ENC: lw.or.aqf{{.*}}encoding: [0x8b,0x2f,0xcc,0x95]
# DIS: lw.or.aqf [t#1], u#1, ->t

lw.or.rlf [a0], a1, ->a3
# ENC: lw.or.rlf{{.*}}encoding: [0x8b,0x22,0x31,0x92]
# DIS: lw.or.rlf [a0], a1, ->a3

lw.or.aqrlf [t#1], u#1, ->t
# ENC: lw.or.aqrlf{{.*}}encoding: [0x8b,0x2f,0xcc,0x97]
# DIS: lw.or.aqrlf [t#1], u#1, ->t

lw.smax [a0], a1, ->a3
# ENC: lw.smax{{.*}}encoding: [0x8b,0x22,0x31,0x20]
# DIS: lw.smax [a0], a1, ->a3

lw.smax.aq [t#1], u#1, ->t
# ENC: lw.smax.aq{{.*}}encoding: [0x8b,0x2f,0xcc,0x25]
# DIS: lw.smax.aq [t#1], u#1, ->t

lw.smax.rl [a0], a1, ->a3
# ENC: lw.smax.rl{{.*}}encoding: [0x8b,0x22,0x31,0x22]
# DIS: lw.smax.rl [a0], a1, ->a3

lw.smax.f [t#1], u#1, ->t
# ENC: lw.smax.f{{.*}}encoding: [0x8b,0x2f,0xcc,0xa1]
# DIS: lw.smax.f [t#1], u#1, ->t

lw.smax.aqrl [a0], a1, ->a3
# ENC: lw.smax.aqrl{{.*}}encoding: [0x8b,0x22,0x31,0x26]
# DIS: lw.smax.aqrl [a0], a1, ->a3

lw.smax.aqf [t#1], u#1, ->t
# ENC: lw.smax.aqf{{.*}}encoding: [0x8b,0x2f,0xcc,0xa5]
# DIS: lw.smax.aqf [t#1], u#1, ->t

lw.smax.rlf [a0], a1, ->a3
# ENC: lw.smax.rlf{{.*}}encoding: [0x8b,0x22,0x31,0xa2]
# DIS: lw.smax.rlf [a0], a1, ->a3

lw.smax.aqrlf [t#1], u#1, ->t
# ENC: lw.smax.aqrlf{{.*}}encoding: [0x8b,0x2f,0xcc,0xa7]
# DIS: lw.smax.aqrlf [t#1], u#1, ->t

lw.smin [a0], a1, ->a3
# ENC: lw.smin{{.*}}encoding: [0x8b,0x22,0x31,0x28]
# DIS: lw.smin [a0], a1, ->a3

lw.smin.aq [t#1], u#1, ->t
# ENC: lw.smin.aq{{.*}}encoding: [0x8b,0x2f,0xcc,0x2d]
# DIS: lw.smin.aq [t#1], u#1, ->t

lw.smin.rl [a0], a1, ->a3
# ENC: lw.smin.rl{{.*}}encoding: [0x8b,0x22,0x31,0x2a]
# DIS: lw.smin.rl [a0], a1, ->a3

lw.smin.f [t#1], u#1, ->t
# ENC: lw.smin.f{{.*}}encoding: [0x8b,0x2f,0xcc,0xa9]
# DIS: lw.smin.f [t#1], u#1, ->t

lw.smin.aqrl [a0], a1, ->a3
# ENC: lw.smin.aqrl{{.*}}encoding: [0x8b,0x22,0x31,0x2e]
# DIS: lw.smin.aqrl [a0], a1, ->a3

lw.smin.aqf [t#1], u#1, ->t
# ENC: lw.smin.aqf{{.*}}encoding: [0x8b,0x2f,0xcc,0xad]
# DIS: lw.smin.aqf [t#1], u#1, ->t

lw.smin.rlf [a0], a1, ->a3
# ENC: lw.smin.rlf{{.*}}encoding: [0x8b,0x22,0x31,0xaa]
# DIS: lw.smin.rlf [a0], a1, ->a3

lw.smin.aqrlf [t#1], u#1, ->t
# ENC: lw.smin.aqrlf{{.*}}encoding: [0x8b,0x2f,0xcc,0xaf]
# DIS: lw.smin.aqrlf [t#1], u#1, ->t

lw.umax [a0], a1, ->a3
# ENC: lw.umax{{.*}}encoding: [0x8b,0x22,0x31,0x30]
# DIS: lw.umax [a0], a1, ->a3

lw.umax.aq [t#1], u#1, ->t
# ENC: lw.umax.aq{{.*}}encoding: [0x8b,0x2f,0xcc,0x35]
# DIS: lw.umax.aq [t#1], u#1, ->t

lw.umax.rl [a0], a1, ->a3
# ENC: lw.umax.rl{{.*}}encoding: [0x8b,0x22,0x31,0x32]
# DIS: lw.umax.rl [a0], a1, ->a3

lw.umax.f [t#1], u#1, ->t
# ENC: lw.umax.f{{.*}}encoding: [0x8b,0x2f,0xcc,0xb1]
# DIS: lw.umax.f [t#1], u#1, ->t

lw.umax.aqrl [a0], a1, ->a3
# ENC: lw.umax.aqrl{{.*}}encoding: [0x8b,0x22,0x31,0x36]
# DIS: lw.umax.aqrl [a0], a1, ->a3

lw.umax.aqf [t#1], u#1, ->t
# ENC: lw.umax.aqf{{.*}}encoding: [0x8b,0x2f,0xcc,0xb5]
# DIS: lw.umax.aqf [t#1], u#1, ->t

lw.umax.rlf [a0], a1, ->a3
# ENC: lw.umax.rlf{{.*}}encoding: [0x8b,0x22,0x31,0xb2]
# DIS: lw.umax.rlf [a0], a1, ->a3

lw.umax.aqrlf [t#1], u#1, ->t
# ENC: lw.umax.aqrlf{{.*}}encoding: [0x8b,0x2f,0xcc,0xb7]
# DIS: lw.umax.aqrlf [t#1], u#1, ->t

lw.umin [a0], a1, ->a3
# ENC: lw.umin{{.*}}encoding: [0x8b,0x22,0x31,0x38]
# DIS: lw.umin [a0], a1, ->a3

lw.umin.aq [t#1], u#1, ->t
# ENC: lw.umin.aq{{.*}}encoding: [0x8b,0x2f,0xcc,0x3d]
# DIS: lw.umin.aq [t#1], u#1, ->t

lw.umin.rl [a0], a1, ->a3
# ENC: lw.umin.rl{{.*}}encoding: [0x8b,0x22,0x31,0x3a]
# DIS: lw.umin.rl [a0], a1, ->a3

lw.umin.f [t#1], u#1, ->t
# ENC: lw.umin.f{{.*}}encoding: [0x8b,0x2f,0xcc,0xb9]
# DIS: lw.umin.f [t#1], u#1, ->t

lw.umin.aqrl [a0], a1, ->a3
# ENC: lw.umin.aqrl{{.*}}encoding: [0x8b,0x22,0x31,0x3e]
# DIS: lw.umin.aqrl [a0], a1, ->a3

lw.umin.aqf [t#1], u#1, ->t
# ENC: lw.umin.aqf{{.*}}encoding: [0x8b,0x2f,0xcc,0xbd]
# DIS: lw.umin.aqf [t#1], u#1, ->t

lw.umin.rlf [a0], a1, ->a3
# ENC: lw.umin.rlf{{.*}}encoding: [0x8b,0x22,0x31,0xba]
# DIS: lw.umin.rlf [a0], a1, ->a3

lw.umin.aqrlf [t#1], u#1, ->t
# ENC: lw.umin.aqrlf{{.*}}encoding: [0x8b,0x2f,0xcc,0xbf]
# DIS: lw.umin.aqrlf [t#1], u#1, ->t

lw.xor [a0], a1, ->a3
# ENC: lw.xor{{.*}}encoding: [0x8b,0x22,0x31,0x18]
# DIS: lw.xor [a0], a1, ->a3

lw.xor.aq [t#1], u#1, ->t
# ENC: lw.xor.aq{{.*}}encoding: [0x8b,0x2f,0xcc,0x1d]
# DIS: lw.xor.aq [t#1], u#1, ->t

lw.xor.rl [a0], a1, ->a3
# ENC: lw.xor.rl{{.*}}encoding: [0x8b,0x22,0x31,0x1a]
# DIS: lw.xor.rl [a0], a1, ->a3

lw.xor.f [t#1], u#1, ->t
# ENC: lw.xor.f{{.*}}encoding: [0x8b,0x2f,0xcc,0x99]
# DIS: lw.xor.f [t#1], u#1, ->t

lw.xor.aqrl [a0], a1, ->a3
# ENC: lw.xor.aqrl{{.*}}encoding: [0x8b,0x22,0x31,0x1e]
# DIS: lw.xor.aqrl [a0], a1, ->a3

lw.xor.aqf [t#1], u#1, ->t
# ENC: lw.xor.aqf{{.*}}encoding: [0x8b,0x2f,0xcc,0x9d]
# DIS: lw.xor.aqf [t#1], u#1, ->t

lw.xor.rlf [a0], a1, ->a3
# ENC: lw.xor.rlf{{.*}}encoding: [0x8b,0x22,0x31,0x9a]
# DIS: lw.xor.rlf [a0], a1, ->a3

lw.xor.aqrlf [t#1], u#1, ->t
# ENC: lw.xor.aqrlf{{.*}}encoding: [0x8b,0x2f,0xcc,0x9f]
# DIS: lw.xor.aqrlf [t#1], u#1, ->t

sc.b a0, [a1], ->a3
# ENC: sc.b{{.*}}encoding: [0x8b,0x12,0x31,0x00]
# DIS: sc.b a0, [a1], ->a3

sc.b.aq t#1, [u#1], ->t
# ENC: sc.b.aq{{.*}}encoding: [0x8b,0x1f,0xcc,0x05]
# DIS: sc.b.aq t#1, [u#1], ->t

sc.b.rl a0, [a1], ->a3
# ENC: sc.b.rl{{.*}}encoding: [0x8b,0x12,0x31,0x02]
# DIS: sc.b.rl a0, [a1], ->a3

sc.b.f t#1, [u#1], ->t
# ENC: sc.b.f{{.*}}encoding: [0x8b,0x1f,0xcc,0x81]
# DIS: sc.b.f t#1, [u#1], ->t

sc.b.aqrl a0, [a1], ->a3
# ENC: sc.b.aqrl{{.*}}encoding: [0x8b,0x12,0x31,0x06]
# DIS: sc.b.aqrl a0, [a1], ->a3

sc.b.aqf t#1, [u#1], ->t
# ENC: sc.b.aqf{{.*}}encoding: [0x8b,0x1f,0xcc,0x85]
# DIS: sc.b.aqf t#1, [u#1], ->t

sc.b.rlf a0, [a1], ->a3
# ENC: sc.b.rlf{{.*}}encoding: [0x8b,0x12,0x31,0x82]
# DIS: sc.b.rlf a0, [a1], ->a3

sc.b.aqrlf t#1, [u#1], ->t
# ENC: sc.b.aqrlf{{.*}}encoding: [0x8b,0x1f,0xcc,0x87]
# DIS: sc.b.aqrlf t#1, [u#1], ->t

sc.d a0, [a1], ->a3
# ENC: sc.d{{.*}}encoding: [0x8b,0x12,0x31,0x18]
# DIS: sc.d a0, [a1], ->a3

sc.d.aq t#1, [u#1], ->t
# ENC: sc.d.aq{{.*}}encoding: [0x8b,0x1f,0xcc,0x1d]
# DIS: sc.d.aq t#1, [u#1], ->t

sc.d.rl a0, [a1], ->a3
# ENC: sc.d.rl{{.*}}encoding: [0x8b,0x12,0x31,0x1a]
# DIS: sc.d.rl a0, [a1], ->a3

sc.d.f t#1, [u#1], ->t
# ENC: sc.d.f{{.*}}encoding: [0x8b,0x1f,0xcc,0x99]
# DIS: sc.d.f t#1, [u#1], ->t

sc.d.aqrl a0, [a1], ->a3
# ENC: sc.d.aqrl{{.*}}encoding: [0x8b,0x12,0x31,0x1e]
# DIS: sc.d.aqrl a0, [a1], ->a3

sc.d.aqf t#1, [u#1], ->t
# ENC: sc.d.aqf{{.*}}encoding: [0x8b,0x1f,0xcc,0x9d]
# DIS: sc.d.aqf t#1, [u#1], ->t

sc.d.rlf a0, [a1], ->a3
# ENC: sc.d.rlf{{.*}}encoding: [0x8b,0x12,0x31,0x9a]
# DIS: sc.d.rlf a0, [a1], ->a3

sc.d.aqrlf t#1, [u#1], ->t
# ENC: sc.d.aqrlf{{.*}}encoding: [0x8b,0x1f,0xcc,0x9f]
# DIS: sc.d.aqrlf t#1, [u#1], ->t

sc.h a0, [a1], ->a3
# ENC: sc.h{{.*}}encoding: [0x8b,0x12,0x31,0x08]
# DIS: sc.h a0, [a1], ->a3

sc.h.aq t#1, [u#1], ->t
# ENC: sc.h.aq{{.*}}encoding: [0x8b,0x1f,0xcc,0x0d]
# DIS: sc.h.aq t#1, [u#1], ->t

sc.h.rl a0, [a1], ->a3
# ENC: sc.h.rl{{.*}}encoding: [0x8b,0x12,0x31,0x0a]
# DIS: sc.h.rl a0, [a1], ->a3

sc.h.f t#1, [u#1], ->t
# ENC: sc.h.f{{.*}}encoding: [0x8b,0x1f,0xcc,0x89]
# DIS: sc.h.f t#1, [u#1], ->t

sc.h.aqrl a0, [a1], ->a3
# ENC: sc.h.aqrl{{.*}}encoding: [0x8b,0x12,0x31,0x0e]
# DIS: sc.h.aqrl a0, [a1], ->a3

sc.h.aqf t#1, [u#1], ->t
# ENC: sc.h.aqf{{.*}}encoding: [0x8b,0x1f,0xcc,0x8d]
# DIS: sc.h.aqf t#1, [u#1], ->t

sc.h.rlf a0, [a1], ->a3
# ENC: sc.h.rlf{{.*}}encoding: [0x8b,0x12,0x31,0x8a]
# DIS: sc.h.rlf a0, [a1], ->a3

sc.h.aqrlf t#1, [u#1], ->t
# ENC: sc.h.aqrlf{{.*}}encoding: [0x8b,0x1f,0xcc,0x8f]
# DIS: sc.h.aqrlf t#1, [u#1], ->t

sc.w a0, [a1], ->a3
# ENC: sc.w{{.*}}encoding: [0x8b,0x12,0x31,0x10]
# DIS: sc.w a0, [a1], ->a3

sc.w.aq t#1, [u#1], ->t
# ENC: sc.w.aq{{.*}}encoding: [0x8b,0x1f,0xcc,0x15]
# DIS: sc.w.aq t#1, [u#1], ->t

sc.w.rl a0, [a1], ->a3
# ENC: sc.w.rl{{.*}}encoding: [0x8b,0x12,0x31,0x12]
# DIS: sc.w.rl a0, [a1], ->a3

sc.w.f t#1, [u#1], ->t
# ENC: sc.w.f{{.*}}encoding: [0x8b,0x1f,0xcc,0x91]
# DIS: sc.w.f t#1, [u#1], ->t

sc.w.aqrl a0, [a1], ->a3
# ENC: sc.w.aqrl{{.*}}encoding: [0x8b,0x12,0x31,0x16]
# DIS: sc.w.aqrl a0, [a1], ->a3

sc.w.aqf t#1, [u#1], ->t
# ENC: sc.w.aqf{{.*}}encoding: [0x8b,0x1f,0xcc,0x95]
# DIS: sc.w.aqf t#1, [u#1], ->t

sc.w.rlf a0, [a1], ->a3
# ENC: sc.w.rlf{{.*}}encoding: [0x8b,0x12,0x31,0x92]
# DIS: sc.w.rlf a0, [a1], ->a3

sc.w.aqrlf t#1, [u#1], ->t
# ENC: sc.w.aqrlf{{.*}}encoding: [0x8b,0x1f,0xcc,0x97]
# DIS: sc.w.aqrlf t#1, [u#1], ->t

sd.add [a0], a1
# ENC: sd.add{{.*}}encoding: [0x0b,0x50,0x31,0x00]
# DIS: sd.add [a0], a1

sd.add.rl [t#1], u#1
# ENC: sd.add.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x03]
# DIS: sd.add.rl [t#1], u#1

sd.and [a0], a1
# ENC: sd.and{{.*}}encoding: [0x0b,0x50,0x31,0x08]
# DIS: sd.and [a0], a1

sd.and.rl [t#1], u#1
# ENC: sd.and.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x0b]
# DIS: sd.and.rl [t#1], u#1

sd.or [a0], a1
# ENC: sd.or{{.*}}encoding: [0x0b,0x50,0x31,0x10]
# DIS: sd.or [a0], a1

sd.or.rl [t#1], u#1
# ENC: sd.or.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x13]
# DIS: sd.or.rl [t#1], u#1

sd.smax [a0], a1
# ENC: sd.smax{{.*}}encoding: [0x0b,0x50,0x31,0x20]
# DIS: sd.smax [a0], a1

sd.smax.rl [t#1], u#1
# ENC: sd.smax.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x23]
# DIS: sd.smax.rl [t#1], u#1

sd.smin [a0], a1
# ENC: sd.smin{{.*}}encoding: [0x0b,0x50,0x31,0x28]
# DIS: sd.smin [a0], a1

sd.smin.rl [t#1], u#1
# ENC: sd.smin.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x2b]
# DIS: sd.smin.rl [t#1], u#1

sd.umax [a0], a1
# ENC: sd.umax{{.*}}encoding: [0x0b,0x50,0x31,0x30]
# DIS: sd.umax [a0], a1

sd.umax.rl [t#1], u#1
# ENC: sd.umax.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x33]
# DIS: sd.umax.rl [t#1], u#1

sd.umin [a0], a1
# ENC: sd.umin{{.*}}encoding: [0x0b,0x50,0x31,0x38]
# DIS: sd.umin [a0], a1

sd.umin.rl [t#1], u#1
# ENC: sd.umin.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x3b]
# DIS: sd.umin.rl [t#1], u#1

sd.xor [a0], a1
# ENC: sd.xor{{.*}}encoding: [0x0b,0x50,0x31,0x18]
# DIS: sd.xor [a0], a1

sd.xor.rl [t#1], u#1
# ENC: sd.xor.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x1b]
# DIS: sd.xor.rl [t#1], u#1

sw.add [a0], a1
# ENC: sw.add{{.*}}encoding: [0x0b,0x30,0x31,0x00]
# DIS: sw.add [a0], a1

sw.add.rl [t#1], u#1
# ENC: sw.add.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x03]
# DIS: sw.add.rl [t#1], u#1

sw.and [a0], a1
# ENC: sw.and{{.*}}encoding: [0x0b,0x30,0x31,0x08]
# DIS: sw.and [a0], a1

sw.and.rl [t#1], u#1
# ENC: sw.and.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x0b]
# DIS: sw.and.rl [t#1], u#1

sw.or [a0], a1
# ENC: sw.or{{.*}}encoding: [0x0b,0x30,0x31,0x10]
# DIS: sw.or [a0], a1

sw.or.rl [t#1], u#1
# ENC: sw.or.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x13]
# DIS: sw.or.rl [t#1], u#1

sw.smax [a0], a1
# ENC: sw.smax{{.*}}encoding: [0x0b,0x30,0x31,0x20]
# DIS: sw.smax [a0], a1

sw.smax.rl [t#1], u#1
# ENC: sw.smax.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x23]
# DIS: sw.smax.rl [t#1], u#1

sw.smin [a0], a1
# ENC: sw.smin{{.*}}encoding: [0x0b,0x30,0x31,0x28]
# DIS: sw.smin [a0], a1

sw.smin.rl [t#1], u#1
# ENC: sw.smin.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x2b]
# DIS: sw.smin.rl [t#1], u#1

sw.umax [a0], a1
# ENC: sw.umax{{.*}}encoding: [0x0b,0x30,0x31,0x30]
# DIS: sw.umax [a0], a1

sw.umax.rl [t#1], u#1
# ENC: sw.umax.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x33]
# DIS: sw.umax.rl [t#1], u#1

sw.umin [a0], a1
# ENC: sw.umin{{.*}}encoding: [0x0b,0x30,0x31,0x38]
# DIS: sw.umin [a0], a1

sw.umin.rl [t#1], u#1
# ENC: sw.umin.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x3b]
# DIS: sw.umin.rl [t#1], u#1

sw.xor [a0], a1
# ENC: sw.xor{{.*}}encoding: [0x0b,0x30,0x31,0x18]
# DIS: sw.xor [a0], a1

sw.xor.rl [t#1], u#1
# ENC: sw.xor.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x1b]
# DIS: sw.xor.rl [t#1], u#1

swapb [a0], a1, ->a3
# ENC: swapb{{.*}}encoding: [0x8b,0x62,0x31,0x00]
# DIS: swapb [a0], a1, ->a3

swapb.aq [t#1], u#1, ->t
# ENC: swapb.aq{{.*}}encoding: [0x8b,0x6f,0xcc,0x05]
# DIS: swapb.aq [t#1], u#1, ->t

swapb.rl [a0], a1, ->a3
# ENC: swapb.rl{{.*}}encoding: [0x8b,0x62,0x31,0x02]
# DIS: swapb.rl [a0], a1, ->a3

swapb.f [t#1], u#1, ->t
# ENC: swapb.f{{.*}}encoding: [0x8b,0x6f,0xcc,0x81]
# DIS: swapb.f [t#1], u#1, ->t

swapb.aqrl [a0], a1, ->a3
# ENC: swapb.aqrl{{.*}}encoding: [0x8b,0x62,0x31,0x06]
# DIS: swapb.aqrl [a0], a1, ->a3

swapb.aqf [t#1], u#1, ->t
# ENC: swapb.aqf{{.*}}encoding: [0x8b,0x6f,0xcc,0x85]
# DIS: swapb.aqf [t#1], u#1, ->t

swapb.rlf [a0], a1, ->a3
# ENC: swapb.rlf{{.*}}encoding: [0x8b,0x62,0x31,0x82]
# DIS: swapb.rlf [a0], a1, ->a3

swapb.aqrlf [t#1], u#1, ->t
# ENC: swapb.aqrlf{{.*}}encoding: [0x8b,0x6f,0xcc,0x87]
# DIS: swapb.aqrlf [t#1], u#1, ->t

swapd [a0], a1, ->a3
# ENC: swapd{{.*}}encoding: [0x8b,0x62,0x31,0x18]
# DIS: swapd [a0], a1, ->a3

swapd.aq [t#1], u#1, ->t
# ENC: swapd.aq{{.*}}encoding: [0x8b,0x6f,0xcc,0x1d]
# DIS: swapd.aq [t#1], u#1, ->t

swapd.rl [a0], a1, ->a3
# ENC: swapd.rl{{.*}}encoding: [0x8b,0x62,0x31,0x1a]
# DIS: swapd.rl [a0], a1, ->a3

swapd.f [t#1], u#1, ->t
# ENC: swapd.f{{.*}}encoding: [0x8b,0x6f,0xcc,0x99]
# DIS: swapd.f [t#1], u#1, ->t

swapd.aqrl [a0], a1, ->a3
# ENC: swapd.aqrl{{.*}}encoding: [0x8b,0x62,0x31,0x1e]
# DIS: swapd.aqrl [a0], a1, ->a3

swapd.aqf [t#1], u#1, ->t
# ENC: swapd.aqf{{.*}}encoding: [0x8b,0x6f,0xcc,0x9d]
# DIS: swapd.aqf [t#1], u#1, ->t

swapd.rlf [a0], a1, ->a3
# ENC: swapd.rlf{{.*}}encoding: [0x8b,0x62,0x31,0x9a]
# DIS: swapd.rlf [a0], a1, ->a3

swapd.aqrlf [t#1], u#1, ->t
# ENC: swapd.aqrlf{{.*}}encoding: [0x8b,0x6f,0xcc,0x9f]
# DIS: swapd.aqrlf [t#1], u#1, ->t

swaph [a0], a1, ->a3
# ENC: swaph{{.*}}encoding: [0x8b,0x62,0x31,0x08]
# DIS: swaph [a0], a1, ->a3

swaph.aq [t#1], u#1, ->t
# ENC: swaph.aq{{.*}}encoding: [0x8b,0x6f,0xcc,0x0d]
# DIS: swaph.aq [t#1], u#1, ->t

swaph.rl [a0], a1, ->a3
# ENC: swaph.rl{{.*}}encoding: [0x8b,0x62,0x31,0x0a]
# DIS: swaph.rl [a0], a1, ->a3

swaph.f [t#1], u#1, ->t
# ENC: swaph.f{{.*}}encoding: [0x8b,0x6f,0xcc,0x89]
# DIS: swaph.f [t#1], u#1, ->t

swaph.aqrl [a0], a1, ->a3
# ENC: swaph.aqrl{{.*}}encoding: [0x8b,0x62,0x31,0x0e]
# DIS: swaph.aqrl [a0], a1, ->a3

swaph.aqf [t#1], u#1, ->t
# ENC: swaph.aqf{{.*}}encoding: [0x8b,0x6f,0xcc,0x8d]
# DIS: swaph.aqf [t#1], u#1, ->t

swaph.rlf [a0], a1, ->a3
# ENC: swaph.rlf{{.*}}encoding: [0x8b,0x62,0x31,0x8a]
# DIS: swaph.rlf [a0], a1, ->a3

swaph.aqrlf [t#1], u#1, ->t
# ENC: swaph.aqrlf{{.*}}encoding: [0x8b,0x6f,0xcc,0x8f]
# DIS: swaph.aqrlf [t#1], u#1, ->t

swapw [a0], a1, ->a3
# ENC: swapw{{.*}}encoding: [0x8b,0x62,0x31,0x10]
# DIS: swapw [a0], a1, ->a3

swapw.aq [t#1], u#1, ->t
# ENC: swapw.aq{{.*}}encoding: [0x8b,0x6f,0xcc,0x15]
# DIS: swapw.aq [t#1], u#1, ->t

swapw.rl [a0], a1, ->a3
# ENC: swapw.rl{{.*}}encoding: [0x8b,0x62,0x31,0x12]
# DIS: swapw.rl [a0], a1, ->a3

swapw.f [t#1], u#1, ->t
# ENC: swapw.f{{.*}}encoding: [0x8b,0x6f,0xcc,0x91]
# DIS: swapw.f [t#1], u#1, ->t

swapw.aqrl [a0], a1, ->a3
# ENC: swapw.aqrl{{.*}}encoding: [0x8b,0x62,0x31,0x16]
# DIS: swapw.aqrl [a0], a1, ->a3

swapw.aqf [t#1], u#1, ->t
# ENC: swapw.aqf{{.*}}encoding: [0x8b,0x6f,0xcc,0x95]
# DIS: swapw.aqf [t#1], u#1, ->t

swapw.rlf [a0], a1, ->a3
# ENC: swapw.rlf{{.*}}encoding: [0x8b,0x62,0x31,0x92]
# DIS: swapw.rlf [a0], a1, ->a3

swapw.aqrlf [t#1], u#1, ->t
# ENC: swapw.aqrlf{{.*}}encoding: [0x8b,0x6f,0xcc,0x97]
# DIS: swapw.aqrlf [t#1], u#1, ->t

sw.sub [a0], a1
# ENC: sw.sub{{.*}}encoding: [0x0b,0x30,0x31,0x40]
# DIS: sw.sub [a0], a1

sw.sub.rl [t#1], u#1
# ENC: sw.sub.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x43]
# DIS: sw.sub.rl [t#1], u#1

sd.sub [a0], a1
# ENC: sd.sub{{.*}}encoding: [0x0b,0x50,0x31,0x40]
# DIS: sd.sub [a0], a1

sd.sub.rl [t#1], u#1
# ENC: sd.sub.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x43]
# DIS: sd.sub.rl [t#1], u#1

sb.add [a0], a1
# ENC: sb.add{{.*}}encoding: [0x0b,0x30,0x31,0x80]
# DIS: sb.add [a0], a1

sb.add.rl [t#1], u#1
# ENC: sb.add.rl{{.*}}encoding: [0x0b,0x30,0xcc,0x83]
# DIS: sb.add.rl [t#1], u#1

sb.sub [a0], a1
# ENC: sb.sub{{.*}}encoding: [0x0b,0x30,0x31,0xc0]
# DIS: sb.sub [a0], a1

sb.sub.rl [t#1], u#1
# ENC: sb.sub.rl{{.*}}encoding: [0x0b,0x30,0xcc,0xc3]
# DIS: sb.sub.rl [t#1], u#1

sh.add [a0], a1
# ENC: sh.add{{.*}}encoding: [0x0b,0x50,0x31,0x80]
# DIS: sh.add [a0], a1

sh.add.rl [t#1], u#1
# ENC: sh.add.rl{{.*}}encoding: [0x0b,0x50,0xcc,0x83]
# DIS: sh.add.rl [t#1], u#1

sh.sub [a0], a1
# ENC: sh.sub{{.*}}encoding: [0x0b,0x50,0x31,0xc0]
# DIS: sh.sub [a0], a1

sh.sub.rl [t#1], u#1
# ENC: sh.sub.rl{{.*}}encoding: [0x0b,0x50,0xcc,0xc3]
# DIS: sh.sub.rl [t#1], u#1

# Field endpoints: SrcD must remain independent of aq/rl and destination.
casd.aqrl [zero], u#4, u#4, ->u
# ENC: casd.aqrl{{.*}}encoding: [0x1b,0x3f,0xf0,0xff]
# DIS: casd.aqrl [zero], u#4, u#4, ->u

dma [zero], u#4
# ENC: dma{{.*}}encoding: [0x0b,0x70,0xf0,0x01]
# DIS: dma [zero], u#4

hl.casd.aqrlf [u#4], zero, u#4, ->zero
# ENC: hl.casd.aqrlf{{.*}}encoding: [0xce,0x07,0x0b,0xe0,0x0f,0x3e]
# DIS: hl.casd.aqrlf [u#4], zero, u#4, ->zero
