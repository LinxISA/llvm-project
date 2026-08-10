# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v058_tepl_template_ops
	.type	test_v058_tepl_template_ops,@function
test_v058_tepl_template_ops:
	BSTART.VEC TADD, FP16
	BSTART.SFU TROWMAX, FP16
	BSTART.TEPL 3, 16, FP16
	BSTART.SFU TTRANS, FP16
	BSTART.SFU TSORT, FP16
	C.BSTOP
	.size	test_v058_tepl_template_ops, .-test_v058_tepl_template_ops

# CHECK-LABEL: <test_v058_tepl_template_ops>:
# CHECK: BSTART.VEC{{[[:space:]]+}}TADD, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TROWMAX, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TSCATTER, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TTRANS, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TSORT, FP16
# CHECK: C.BSTOP
