# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v058_block_descriptor_roundtrip
	.type	test_v058_block_descriptor_roundtrip,@function
test_v058_block_descriptor_roundtrip:
	BSTART.TMOV FP16
	B.IOT t#1, t#3, mask=1111, last
	B.IOT u#3, mask=1111, last, ->t<1KB>
	B.IOT mask=1111, last, ->u<128B>
	B.CATR trap, atomic, aqrl, far, dr
	B.DATR normal, S8, ZERO, cmode3, rmode2, sat
	C.BSTOP
	.size	test_v058_block_descriptor_roundtrip, .-test_v058_block_descriptor_roundtrip

# CHECK-LABEL: <test_v058_block_descriptor_roundtrip>:
# CHECK: BSTART.TMOV{{[[:space:]]+}}FP16
# CHECK: B.IOT{{[[:space:]]+}}t#1, t#3, mask=1111, last
# CHECK: B.IOT{{[[:space:]]+}}u#3, mask=1111, last,{{[[:space:]]+}}->t<1KB>
# CHECK: B.IOT{{[[:space:]]+}}mask=1111, last,{{[[:space:]]+}}->u<128B>
# CHECK: B.CATR{{[[:space:]]+}}trap, atomic, aqrl, far, dr
# CHECK: B.DATR{{[[:space:]]+}}NORM.normal, S8, Zero, cmode3, rmode2, sat
# CHECK: C.BSTOP
