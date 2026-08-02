# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_dtype_keywords
	.type	test_v057_dtype_keywords,@function
test_v057_dtype_keywords:
	BSTART.TLOAD FP64
	BSTART.TSTORE INT32
	BSTART.TMOV UINT8
	BSTART.TMATMUL FP8
	BSTART.ACCCVT BF16
	BSTART.TADD FP4
	BSTART.TEPL 3, 14, FPL4
	BSTART.TEPL 2, 29, UINT4
	C.BSTOP
	.size	test_v057_dtype_keywords, .-test_v057_dtype_keywords

# CHECK-LABEL: <test_v057_dtype_keywords>:
# CHECK: BSTART.TLOAD{{[[:space:]]+}}FP64
# CHECK: BSTART.TSTORE{{[[:space:]]+}}INT32
# CHECK: BSTART.TMOV{{[[:space:]]+}}UINT8
# CHECK: BSTART.TMATMUL{{[[:space:]]+}}FP8
# CHECK: BSTART.ACCCVT{{[[:space:]]+}}BF16
# CHECK: BSTART.TADD{{[[:space:]]+}}FP4
# CHECK: BSTART.TTRANS{{[[:space:]]+}}FPL4
# CHECK: BSTART.TCOLARGMIN{{[[:space:]]+}}UINT4
# CHECK: C.BSTOP
