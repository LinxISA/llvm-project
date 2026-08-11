# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC

	BSTART.TSTORE FP32
	BSTART.TSTORE.SPART FP32

# ENC: BSTART.TSTORE{{[[:space:]]+}}FP32{{.*}}encoding: [0x81,0x11,0x11,0x08]
# ENC: BSTART.TSTORE.SPART{{[[:space:]]+}}FP32{{.*}}encoding: [0x81,0x11,0xe1,0x08]
