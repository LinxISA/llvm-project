# RUN: not llvm-mc -triple=linx64 %s -o /dev/null 2>&1 | FileCheck %s

	.text
	BSTART.TEPL 0, 5, FP16
	BSTART.TEPL 0, 24, FP16
	BSTART.TEPL 1, 5, FP16
	BSTART.TEPL 3, 30, FP16

# CHECK-COUNT-4: error: BSTART.TEPL Mode/Function is reserved in PTO ISA 0.57.1
