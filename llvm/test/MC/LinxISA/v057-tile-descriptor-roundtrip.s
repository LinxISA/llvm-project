# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_tile_descriptor_roundtrip
	.type	test_v057_tile_descriptor_roundtrip,@function
test_v057_tile_descriptor_roundtrip:
	BSTART.TLOAD FP16
	B.OTA ->tile#255<31>, last, 0
	B.ITP [tile#1.reuse, TZERO], last, 0
	BSTOP
	.size	test_v057_tile_descriptor_roundtrip, .-test_v057_tile_descriptor_roundtrip

# CHECK-LABEL: <test_v057_tile_descriptor_roundtrip>:
# CHECK: BSTART.TLOAD{{[[:space:]]+}}FP16
# CHECK: B.OTA{{[[:space:]]+}}->tile#255<31>, last, 0
# CHECK: B.ITP{{[[:space:]]+}}[t#1.reuse, TZERO], last, 0
# CHECK: BSTOP
