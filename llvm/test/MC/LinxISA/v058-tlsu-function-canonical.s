# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v058_tlsu_function_canonical
	.type	test_v058_tlsu_function_canonical,@function
test_v058_tlsu_function_canonical:
	BSTART.TLOAD FP16
	BSTART.TSTORE FP16
	BSTART.TMOV FP16
	BSTART.TPREFETCH FP16
	BSTART.MGATHER FP16
	BSTART.MSCATTER FP16
	BSTART.MGATHER.MASK FP16
	BSTART.MSCATTER.MASK FP16
	BSTART.MGATHER.CAS FP16
	C.BSTOP
	.size	test_v058_tlsu_function_canonical, .-test_v058_tlsu_function_canonical

# CHECK-LABEL: <test_v058_tlsu_function_canonical>:
# CHECK: BSTART.TLOAD{{[[:space:]]+}}FP16
# CHECK: BSTART.TSTORE{{[[:space:]]+}}FP16
# CHECK: BSTART.TMOV{{[[:space:]]+}}FP16
# CHECK: BSTART.TPREFETCH{{[[:space:]]+}}FP16
# CHECK: BSTART.MGATHER{{[[:space:]]+}}FP16
# CHECK: BSTART.MSCATTER{{[[:space:]]+}}FP16
# CHECK: BSTART.MGATHER.MASK{{[[:space:]]+}}FP16
# CHECK: BSTART.MSCATTER.MASK{{[[:space:]]+}}FP16
# CHECK: BSTART.MGATHER.CAS{{[[:space:]]+}}FP16
# CHECK: C.BSTOP
