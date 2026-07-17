# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_block_descriptor_roundtrip
	.type	test_v057_block_descriptor_roundtrip,@function
test_v057_block_descriptor_roundtrip:
	BSTART.TMOV FP16
	B.IOT t#1, t#3.reuse, last, ->acc<4KB>
	B.IOT u#3, last, ->t<1KB>
	B.IOT last, ->u<0>
	B.CATR trap, atomic, aqrl, far, dr
	B.DATR normal, INT8, ZERO, cmode3, rmode2, sat
	C.BSTOP
	.size	test_v057_block_descriptor_roundtrip, .-test_v057_block_descriptor_roundtrip

# CHECK-LABEL: <test_v057_block_descriptor_roundtrip>:
# CHECK: BSTART.TMOV{{[[:space:]]+}}FP16
# CHECK: B.IOT{{[[:space:]]+}}t#1, t#3.reuse, last,{{[[:space:]]+}}->acc<4KB>
# CHECK: B.IOT{{[[:space:]]+}}u#3, last,{{[[:space:]]+}}->t<1KB>
# CHECK: B.IOT{{[[:space:]]+}}last,{{[[:space:]]+}}->u<0>
# CHECK: B.CATR{{[[:space:]]+}}trap, atomic, aqrl, far, dr
# CHECK: B.DATR{{[[:space:]]+}}normal, INT8, Zero, cmode3, rmode2, sat
# CHECK: C.BSTOP
