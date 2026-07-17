# RUN: not llvm-mc -triple=linx64 %s -o /dev/null 2>&1 | FileCheck %s

	.text
bad_v057_tma_func:
	BSTART.TMA 9, FP16

# CHECK: error: unrecognized instruction 'bstart.tma'

bad_v057_tma_tprefetch_with_dest:
	BSTART.TPREFETCH r1, FP16

# CHECK: error: too many register operands
