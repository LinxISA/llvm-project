# RUN: not llvm-mc -triple=linx64 -show-encoding %s 2>&1 | FileCheck %s

	BSTART.TSTORE FP32
	BSTART.TSTORE.SPART FP32

# CHECK: error: unrecognized instruction 'bstart.tstore.spart'
# CHECK: BSTART.TSTORE{{[[:space:]]+}}FP32{{.*}}encoding: [0x81,0x11,0x11,0x08]
