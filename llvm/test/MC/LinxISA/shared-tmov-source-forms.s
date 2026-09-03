# RUN: not llvm-mc -triple=linx64 -show-encoding %s 2>&1 | FileCheck %s

	BSTART.TMOV FP16
	BSTART.TMOV.L2S.INSERT FP16
	BSTART.TMOV.L2S.PUBLISH FP16
	BSTART.TMOV.S2L.BROADCAST FP16
	BSTART.TMOV.S2L.EXTRACT FP16

# CHECK-COUNT-4: error: unrecognized instruction
# CHECK: BSTART.TMOV{{[[:space:]]+}}FP16{{.*}}encoding: [0x81,0x11,0x21,0x20]
