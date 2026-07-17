# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_cube_function_fallback
	.type	test_v057_cube_function_fallback,@function
test_v057_cube_function_fallback:
	BSTART.CUBE 5, FP16
	C.BSTOP
	.size	test_v057_cube_function_fallback, .-test_v057_cube_function_fallback

# CHECK-LABEL: <test_v057_cube_function_fallback>:
# CHECK: BSTART.CUBE{{[[:space:]]+}}5, FP16
# CHECK: C.BSTOP
