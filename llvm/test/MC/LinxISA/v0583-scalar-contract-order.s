# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --triple=linx64 %t | FileCheck %s

	# PTO ISA 0.58.3 defines HL.LUI as imm32 << 32, HL.MADDW outputs the
	# sign-extended low/high 32-bit halves, and HL.REM* publishes
	# remainder before quotient.  Lock the architectural Dst0/Dst1 ordering.
	HL.LUI 0x12345678, ->a0
	HL.MADDW a0, a1, a2, ->a3, a4
	HL.REM a0, a1, ->a2, a3
	HL.REMU a0, a1, ->a2, a3
	HL.REMW a0, a1, ->a2, a3
	HL.REMUW a0, a1, ->a2, a3

# CHECK: hl.lui{{[[:space:]]+}}305419896, ->a0
# CHECK: hl.maddw{{[[:space:]]+}}a0, a1, a2, ->a3, a4
# CHECK: hl.rem{{[[:space:]]+}}a0, a1, ->a2, a3
# CHECK: hl.remu{{[[:space:]]+}}a0, a1, ->a2, a3
# CHECK: hl.remw{{[[:space:]]+}}a0, a1, ->a2, a3
# CHECK: hl.remuw{{[[:space:]]+}}a0, a1, ->a2, a3
