# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s
# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC

	.text
	.globl	test_v058_tepl_template_ops
	.type	test_v058_tepl_template_ops,@function
test_v058_tepl_template_ops:
	BSTART.VEC TADD, FP16
	BSTART.SFU TDIV, FP16
	BSTART.SFU TREM, FP16
	BSTART.SFU TDIVS, FP16
	BSTART.SFU TREMS, FP16
	BSTART.SFU TROWMAX, FP16
	BSTART.TEPL 3, 16, FP16
	BSTART.SFU TPERMUTE, FP16
	BSTART.SFU TSORT, FP16
	BSTART.SFU TGPR2T, FP16
	BSTART.TEPL 3, 30, FP16
	C.BSTOP
	.size	test_v058_tepl_template_ops, .-test_v058_tepl_template_ops

# CHECK-LABEL: <test_v058_tepl_template_ops>:
# CHECK: BSTART.VEC{{[[:space:]]+}}TADD, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TDIV, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TREM, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TDIVS, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TREMS, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TROWMAX, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TSCATTER, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TPERMUTE, FP16
# CHECK: BSTART.SFU{{[[:space:]]+}}TSORT, FP16
# CHECK-COUNT-2: BSTART.SFU{{[[:space:]]+}}TGPR2T, FP16
# CHECK: C.BSTOP
# ENC-COUNT-2: BSTART.SFU TGPR2T, FP16{{.*}}encoding: [0x81,0x91,0xe1,0x27]
