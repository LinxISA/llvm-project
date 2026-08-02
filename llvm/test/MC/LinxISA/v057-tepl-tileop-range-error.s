# RUN: not llvm-mc -triple=linx64 %s -o /dev/null 2>&1 | FileCheck %s

	.text
bad_v057_tepl_tileop:
	BSTART.TEPL 4, 0, FP16
	BSTART.TEPL 0, 32, FP16

# CHECK-COUNT-2: error: BSTART.TEPL requires Mode 0..3 and Function 0..31
