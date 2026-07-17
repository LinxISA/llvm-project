# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=DIS

	.text
	CASB [a0], a1, a2, ->a3
	CASH.AQ [a0], a1, a2, ->a3
	CASW.RL [a0], a1, a2, ->a3
	CASD.AQRL [a0], a1, a2, ->a3
	DMA [a0], a1

# CHECK: casb{{[[:space:]]+}}[a0, 0], a1, a2,{{[[:space:]]+}}->a3{{[[:space:]]+}}#  encoding: [0x9b,0x02,0x31,0x20]
# CHECK: cash.aq{{[[:space:]]+}}[a0, 0], a1, a2,{{[[:space:]]+}}->a3{{[[:space:]]+}}#  encoding: [0x9b,0x12,0x31,0x24]
# CHECK: casw.rl{{[[:space:]]+}}[a0, 0], a1, a2,{{[[:space:]]+}}->a3{{[[:space:]]+}}#  encoding: [0x9b,0x22,0x31,0x22]
# CHECK: casd.aqrl{{[[:space:]]+}}[a0, 0], a1, a2,{{[[:space:]]+}}->a3{{[[:space:]]+}}#  encoding: [0x9b,0x32,0x31,0x26]
# CHECK: dma{{[[:space:]]+}}[a0, 0], a1{{[[:space:]]+}}#  encoding: [0x0b,0x70,0x31,0x00]

# DIS: casb{{[[:space:]]+}}[a0, 0], a1, a2,{{[[:space:]]+}}->a3
# DIS: cash.aq{{[[:space:]]+}}[a0, 0], a1, a2,{{[[:space:]]+}}->a3
# DIS: casw.rl{{[[:space:]]+}}[a0, 0], a1, a2,{{[[:space:]]+}}->a3
# DIS: casd.aqrl{{[[:space:]]+}}[a0, 0], a1, a2,{{[[:space:]]+}}->a3
# DIS: dma{{[[:space:]]+}}[a0, 0], a1
