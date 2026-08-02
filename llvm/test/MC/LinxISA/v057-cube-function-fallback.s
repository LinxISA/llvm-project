# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

	.text
	.globl	test_v057_cube_function_fallback
	.type	test_v057_cube_function_fallback,@function
test_v057_cube_function_fallback:
	BSTART.CUBE 5, FP16
	.size	test_v057_cube_function_fallback, .-test_v057_cube_function_fallback

# CHECK: error: unrecognized instruction 'bstart.cube'
