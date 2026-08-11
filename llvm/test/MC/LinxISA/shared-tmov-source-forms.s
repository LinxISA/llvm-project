# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

	BSTART.TMOV.L2S.INSERT FP16
	BSTART.TMOV.L2S.PUBLISH FP16
	BSTART.TMOV.S2L.BROADCAST FP16
	BSTART.TMOV.S2L.EXTRACT FP16

# ENC: BSTART.TMOV.L2S.INSERT{{[[:space:]]+}}FP16{{.*}}encoding: [0x81,0x11,0x91,0x20]
# ENC: BSTART.TMOV.L2S.PUBLISH{{[[:space:]]+}}FP16{{.*}}encoding: [0x81,0x11,0xa1,0x20]
# ENC: BSTART.TMOV.S2L.BROADCAST{{[[:space:]]+}}FP16{{.*}}encoding: [0x81,0x11,0xb1,0x20]
# ENC: BSTART.TMOV.S2L.EXTRACT{{[[:space:]]+}}FP16{{.*}}encoding: [0x81,0x11,0xc1,0x20]

# DIS: BSTART.TMOV.L2S.INSERT{{[[:space:]]+}}FP16
# DIS: BSTART.TMOV.L2S.PUBLISH{{[[:space:]]+}}FP16
# DIS: BSTART.TMOV.S2L.BROADCAST{{[[:space:]]+}}FP16
# DIS: BSTART.TMOV.S2L.EXTRACT{{[[:space:]]+}}FP16
