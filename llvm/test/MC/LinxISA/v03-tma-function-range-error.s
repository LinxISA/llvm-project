# RUN: not llvm-mc -triple=linx64 %s -o /dev/null 2>&1 | FileCheck %s

	.text
bad_v03_tma_func:
	BSTART.TMA 3, FP16

# CHECK: error: BSTART.TMA Function must be in range 0..2 in canonical v0.56
