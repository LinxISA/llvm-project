# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v058_tmov_descriptor_roundtrip
	.type	test_v058_tmov_descriptor_roundtrip,@function
test_v058_tmov_descriptor_roundtrip:
	BSTART.TMOV FP16
	B.IOT t#2, mask=1111, last, ->u<4KB>
	C.BSTOP
	.size	test_v058_tmov_descriptor_roundtrip, .-test_v058_tmov_descriptor_roundtrip

# CHECK-LABEL: <test_v058_tmov_descriptor_roundtrip>:
# CHECK: BSTART.TMOV{{[[:space:]]+}}FP16
# CHECK: B.IOT{{[[:space:]]+}}t#2, mask=1111, last,{{[[:space:]]+}}->u<4KB>
# CHECK: C.BSTOP
