# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_cube_function_canonical
	.type	test_v057_cube_function_canonical,@function
test_v057_cube_function_canonical:
	BSTART.TMATMUL FP16
	BSTART.TMATMUL.BIAS FP16
	BSTART.TMATMUL.ACC FP16
	BSTART.TMATMULMX FP16
	BSTART.TMATMULMX.BIAS FP16
	BSTART.TMATMULMX.ACC FP16
	BSTART.ACCCVT FP32
	BSTART.TGEMV FP16
	BSTART.TGEMV.BIAS FP16
	BSTART.TGEMV.ACC FP16
	BSTART.TGEMVMX FP16
	BSTART.TGEMVMX.BIAS FP16
	BSTART.TGEMVMX.ACC FP16
	C.BSTOP
	.size	test_v057_cube_function_canonical, .-test_v057_cube_function_canonical

# CHECK-LABEL: <test_v057_cube_function_canonical>:
# CHECK: BSTART.TMATMUL{{[[:space:]]+}}FP16
# CHECK: BSTART.TMATMUL.BIAS{{[[:space:]]+}}FP16
# CHECK: BSTART.TMATMUL.ACC{{[[:space:]]+}}FP16
# CHECK: BSTART.TMATMULMX{{[[:space:]]+}}FP16
# CHECK: BSTART.TMATMULMX.BIAS{{[[:space:]]+}}FP16
# CHECK: BSTART.TMATMULMX.ACC{{[[:space:]]+}}FP16
# CHECK: BSTART.ACCCVT{{[[:space:]]+}}FP32
# CHECK: BSTART.TGEMV{{[[:space:]]+}}FP16
# CHECK: BSTART.TGEMV.BIAS{{[[:space:]]+}}FP16
# CHECK: BSTART.TGEMV.ACC{{[[:space:]]+}}FP16
# CHECK: BSTART.TGEMVMX{{[[:space:]]+}}FP16
# CHECK: BSTART.TGEMVMX.BIAS{{[[:space:]]+}}FP16
# CHECK: BSTART.TGEMVMX.ACC{{[[:space:]]+}}FP16
# CHECK: C.BSTOP
