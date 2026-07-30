# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_tepl_template_ops
	.type	test_v057_tepl_template_ops,@function
test_v057_tepl_template_ops:
	BSTART.TADD FP16
	BSTART.TROWMAX FP16
	BSTART.TEPL 3, 16, FP16
	BSTART.TTRANS FP16
	BSTART.TSORT FP16
	C.BSTOP
	.size	test_v057_tepl_template_ops, .-test_v057_tepl_template_ops

# CHECK-LABEL: <test_v057_tepl_template_ops>:
# CHECK: BSTART.TADD{{[[:space:]]+}}FP16
# CHECK: BSTART.TROWMAX{{[[:space:]]+}}FP16
# CHECK: BSTART.TSCATTER{{[[:space:]]+}}FP16
# CHECK: BSTART.TTRANS{{[[:space:]]+}}FP16
# CHECK: BSTART.TSORT{{[[:space:]]+}}FP16
# CHECK: C.BSTOP
