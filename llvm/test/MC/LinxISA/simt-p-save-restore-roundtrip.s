# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_simt_p_save_restore_roundtrip
	.type	test_simt_p_save_restore_roundtrip,@function
test_simt_p_save_restore_roundtrip:
	BSTART.MSEQ 0
	B.TEXT .body
	B.IOT last, ->t<1KB>
	C.B.DIMI 8, ->lb0
	C.B.DIMI 1, ->lb1
	C.BSTART
.body:
	v.cmp.lt lc0.uh, ri0, ->p
	v.psel p, ri1, ->vt.w
	v.cmp.ne vt#1.sw, zero, ->p
	C.BSTOP
	.size	test_simt_p_save_restore_roundtrip, .-test_simt_p_save_restore_roundtrip

# CHECK-LABEL: <test_simt_p_save_restore_roundtrip>:
# CHECK: BSTART.MSEQ
# CHECK: B.TEXT
# CHECK: B.IOT{{[[:space:]]+}}last,{{[[:space:]]+}}->t<1KB>
# CHECK: C.B.DIMI{{[[:space:]]+}}8, {{[[:space:]]*}}->lb0
# CHECK: C.B.DIMI{{[[:space:]]+}}1, {{[[:space:]]*}}->lb1
# CHECK: C.BSTART
# CHECK: v.cmp.lt{{[[:space:]]+}}lc0, ri0,{{[[:space:]]*}}->p
# CHECK: v.psel{{[[:space:]]+}}p, ri1,{{[[:space:]]*}}->vt
# CHECK: v.cmp.ne{{[[:space:]]+}}vt#1, zero,{{[[:space:]]*}}->p
# CHECK: C.BSTOP
