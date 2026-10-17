# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx32 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=DIS
# RUN: llvm-mc -triple=linx32 -filetype=obj %s -o - | llvm-objdump -d --triple=linx32 - | FileCheck %s --check-prefix=DIS

	SB.ADD [a0], a1
	SB.ADD.RL [a0], a1
	SB.SUB [a0], a1
	SH.ADD [a0], a1
	SH.SUB.RL [a0], a1
	SW.SUB [a0], a1
	SD.SUB.RL [a0], a1
	SW.ADD.RL [a0], a1
	SD.ADD [a0], a1
	DMA.FREE.S [a0]
	DMA.FREE.L [a0]
	MCOPY.DS [a0, a1, a2], ->a3
	MCOPY.DL [a0, a1, a2], ->a3
	SYNC.CORE

# ENC: sb.add{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x30,0x31,0x80]
# ENC: sb.add.rl{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x30,0x31,0x82]
# ENC: sb.sub{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x30,0x31,0xc0]
# ENC: sh.add{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x50,0x31,0x80]
# ENC: sh.sub.rl{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x50,0x31,0xc2]
# ENC: sw.sub{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x30,0x31,0x40]
# ENC: sd.sub.rl{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x50,0x31,0x42]
# ENC: sw.add.rl{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x30,0x31,0x02]
# ENC: sd.add{{[[:space:]]+}}[a0, 0], a1{{.*}}encoding: [0x0b,0x50,0x31,0x00]
# ENC: dma.free.s{{[[:space:]]+}}[a0, 0]{{.*}}encoding: [0x0b,0x70,0x01,0x04]
# ENC: dma.free.l{{[[:space:]]+}}[a0, 0]{{.*}}encoding: [0x0b,0x70,0x01,0x06]
# ENC: MCOPY.DS{{[[:space:]]+}}[a0, a1, a2],{{[[:space:]]+}}->a3{{.*}}encoding: [0xb1,0x12,0x31,0x22]
# ENC: MCOPY.DL{{[[:space:]]+}}[a0, a1, a2],{{[[:space:]]+}}->a3{{.*}}encoding: [0xb1,0x12,0x31,0x26]
# ENC: sync.core{{.*}}encoding: [0x2b,0xa0,0x30,0x03]

# DIS: sb.add{{[[:space:]]+}}[a0, 0], a1
# DIS: sb.add.rl{{[[:space:]]+}}[a0, 0], a1
# DIS: sb.sub{{[[:space:]]+}}[a0, 0], a1
# DIS: sh.add{{[[:space:]]+}}[a0, 0], a1
# DIS: sh.sub.rl{{[[:space:]]+}}[a0, 0], a1
# DIS: sw.sub{{[[:space:]]+}}[a0, 0], a1
# DIS: sd.sub.rl{{[[:space:]]+}}[a0, 0], a1
# DIS: sw.add.rl{{[[:space:]]+}}[a0, 0], a1
# DIS: sd.add{{[[:space:]]+}}[a0, 0], a1
# DIS: dma.free.s{{[[:space:]]+}}[a0, 0]
# DIS: dma.free.l{{[[:space:]]+}}[a0, 0]
# DIS: MCOPY.DS{{[[:space:]]+}}[a0, a1, a2],{{[[:space:]]+}}->a3
# DIS: MCOPY.DL{{[[:space:]]+}}[a0, a1, a2],{{[[:space:]]+}}->a3
# DIS: sync.core

