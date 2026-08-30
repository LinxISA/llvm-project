# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v058_dtype_keywords
	.type	test_v058_dtype_keywords,@function
test_v058_dtype_keywords:
	BSTART.TLOAD FP64
	BSTART.TSTORE S32
	BSTART.TMOV U8
	BSTART.TMOV DTYPE_NONE
	BSTART.TMATMUL HiF8
	BSTART.VEC TADD, HiF4X2
	BSTART.TEPL 3, 21, E2M1X2
	BSTART.TEPL 2, 29, U4X2
	B.DATR Layout21, DTYPE_NONE, Null
	B.DATR Layout26, DTYPE_NONE, Null
	C.BSTOP
	.size	test_v058_dtype_keywords, .-test_v058_dtype_keywords

# CHECK-LABEL: <test_v058_dtype_keywords>:
# CHECK: BSTART.TLOAD{{[[:space:]]+}}FP64
# CHECK: BSTART.TSTORE{{[[:space:]]+}}S32
# CHECK: BSTART.TMOV{{[[:space:]]+}}U8
# CHECK: BSTART.TMOV{{[[:space:]]+}}DTYPE_NONE
# CHECK: BSTART.TMATMUL{{[[:space:]]+}}HiF8
# CHECK: BSTART.VEC{{[[:space:]]+}}TADD, HiF4X2
# CHECK: BSTART.SFU{{[[:space:]]+}}TPERMUTE, E2M1X2
# CHECK: BSTART.SFU{{[[:space:]]+}}TCOLARGMIN, U4X2
# CHECK: B.DATR{{[[:space:]]+}}ND2M32, DTYPE_NONE, Null
# CHECK: B.DATR{{[[:space:]]+}}N82ND, DTYPE_NONE, Null
# CHECK: C.BSTOP
