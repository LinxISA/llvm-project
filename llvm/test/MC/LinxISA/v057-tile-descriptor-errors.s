# RUN: not llvm-mc -triple=linx64 %s -o /dev/null 2>&1 | FileCheck %s

	.text
bad_v057_tzero_destination:
	BSTART.TLOAD FP16
	B.OTA ->TZERO<0>, last, 0

# CHECK: error: B.OTA destination cannot be TZERO
