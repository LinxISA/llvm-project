# RUN: not llvm-mc -triple=linx64 %s -o /dev/null 2>&1 | FileCheck %s

	.text
	BSTART.TEPL 0x049, FP16
	BSTART.TEPL 0x08c, FP16
	BSTART.TEPL 0x0c9, FP16
	BSTART.TEPL 0x0e4, FP16

# CHECK-COUNT-4: error: BSTART.TEPL TileOpcode is reserved in canonical v0.57
