# RUN: not llvm-mc -triple=linx64 %s -o /dev/null 2>&1 | FileCheck %s

	.text
	BSTART.MAMULB FP16
	BSTART.MAMULB.ACC FP16

# CHECK: error: unrecognized instruction 'bstart.mamulb'
# CHECK: error: unrecognized instruction 'bstart.mamulb.acc'
