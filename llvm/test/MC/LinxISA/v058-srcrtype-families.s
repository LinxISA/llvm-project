# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=DIS

	add a0, a1, ->a2
	add a0, a1.sw, ->a2
	add a0, a1.uw, ->a2
	add a0, a1.neg, ->a2

	cmp.eq a0, a1, ->a2
	cmp.eq a0, a1.sw, ->a2
	cmp.eq a0, a1.uw, ->a2
	cmp.and a0, a1.not, ->a2
	.byte 0x45, 0x02, 0x31, 0x06 # CMP.EQ raw 11 is a no-modifier alias.

	csel a0, a1, a2, ->a3
	csel a0, a1, a2.neg, ->a3
	.byte 0xf7, 0x82, 0x41, 0x12 # CSEL raw 01 is a no-modifier alias.
	.byte 0xf7, 0x82, 0x41, 0x14 # CSEL raw 10 is a no-modifier alias.

	lw [a0, a1], ->a2
	lw [a0, a1.sw], ->a2
	lw [a0, a1.uw], ->a2

# Binary: 00=.sw, 01=.uw, 10=.neg/.not, 11=none.
# ENC: add a0, a1, ->a2{{.*}}encoding: [0x05,0x02,0x31,0x06]
# ENC: add a0, a1.sw, ->a2{{.*}}encoding: [0x05,0x02,0x31,0x00]
# ENC: add a0, a1.uw, ->a2{{.*}}encoding: [0x05,0x02,0x31,0x02]
# ENC: add a0, a1.neg, ->a2{{.*}}encoding: [0x05,0x02,0x31,0x04]

# Comparison: 00=none, 01=.sw, 10=.uw, 11=.not for logical forms.
# ENC: cmp.eq a0, a1, ->a2{{.*}}encoding: [0x45,0x02,0x31,0x00]
# ENC: cmp.eq a0, a1.sw, ->a2{{.*}}encoding: [0x45,0x02,0x31,0x02]
# ENC: cmp.eq a0, a1.uw, ->a2{{.*}}encoding: [0x45,0x02,0x31,0x04]
# ENC: cmp.and a0, a1.not, ->a2{{.*}}encoding: [0x45,0x22,0x31,0x06]

# Select: 00/01/10=none aliases and 11=.neg; canonical assembly emits 00.
# ENC: csel a0, a1, a2, ->a3{{.*}}encoding: [0xf7,0x82,0x41,0x10]
# ENC: csel a0, a1, a2.neg, ->a3{{.*}}encoding: [0xf7,0x82,0x41,0x16]

# Address: 00=none, 01=.sw, 10=.uw, 11=reserved.
# ENC: lw [a0, a1], ->a2{{.*}}encoding: [0x09,0x22,0x31,0x00]
# ENC: lw [a0, a1.sw], ->a2{{.*}}encoding: [0x09,0x22,0x31,0x02]
# ENC: lw [a0, a1.uw], ->a2{{.*}}encoding: [0x09,0x22,0x31,0x04]

# DIS: add{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
# DIS: add{{[[:space:]]+}}a0, a1.sw,{{[[:space:]]+}}->a2
# DIS: add{{[[:space:]]+}}a0, a1.uw,{{[[:space:]]+}}->a2
# DIS: add{{[[:space:]]+}}a0, a1.neg,{{[[:space:]]+}}->a2
# DIS: cmp.eq{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
# DIS: cmp.eq{{[[:space:]]+}}a0, a1.sw,{{[[:space:]]+}}->a2
# DIS: cmp.eq{{[[:space:]]+}}a0, a1.uw,{{[[:space:]]+}}->a2
# DIS: cmp.and{{[[:space:]]+}}a0, a1.not,{{[[:space:]]+}}->a2
# DIS: cmp.eq{{[[:space:]]+}}a0, a1,{{[[:space:]]+}}->a2
# DIS: csel{{[[:space:]]+}}a0, a1, a2,{{[[:space:]]+}}->a3
# DIS: csel{{[[:space:]]+}}a0, a1, a2.neg,{{[[:space:]]+}}->a3
# DIS-COUNT-2: csel{{[[:space:]]+}}a0, a1, a2,{{[[:space:]]+}}->a3
# DIS: lw{{[[:space:]]+}}[a0, a1],{{[[:space:]]+}}->a2
# DIS: lw{{[[:space:]]+}}[a0, a1.sw],{{[[:space:]]+}}->a2
# DIS: lw{{[[:space:]]+}}[a0, a1.uw],{{[[:space:]]+}}->a2
