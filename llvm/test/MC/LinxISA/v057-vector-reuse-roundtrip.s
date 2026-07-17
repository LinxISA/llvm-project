# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_vector_reuse_roundtrip
	.type	test_v057_vector_reuse_roundtrip,@function
test_v057_vector_reuse_roundtrip:
	BSTART.MSEQ 0
	B.TEXT .body
	B.IOT last, ->t<4KB>
	B.IOT last, ->u<2KB>
	C.B.DIMI 8, ->lb0
	C.B.DIMI 1, ->lb1
	C.BSTART
.body:
	v.lwi.u.local [ts, lc0<<2, 8], ->vt.w
	v.add vt#1.reuse.sw, lc0.uh, ->vu.w
	v.swi.u.local vu#1.reuse.uw, [ts, lc0<<2, 12]
	C.BSTOP
	.size	test_v057_vector_reuse_roundtrip, .-test_v057_vector_reuse_roundtrip

# CHECK-LABEL: <test_v057_vector_reuse_roundtrip>:
# CHECK: BSTART.MSEQ
# CHECK: B.TEXT
# CHECK: B.IOT{{[[:space:]]+}}last,{{[[:space:]]+}}->t<4KB>
# CHECK: B.IOT{{[[:space:]]+}}last,{{[[:space:]]+}}->u<2KB>
# CHECK: C.B.DIMI{{[[:space:]]+}}8, {{[[:space:]]*}}->lb0
# CHECK: C.B.DIMI{{[[:space:]]+}}1, {{[[:space:]]*}}->lb1
# CHECK: C.BSTART
# CHECK: v.lwi.u.local{{[[:space:]]+}}[ts, lc0<<2, 8], ->vt
# CHECK: v.add{{[[:space:]]+}}vt#1, lc0,{{[[:space:]]*}}->vu
# CHECK: v.swi.u.local{{[[:space:]]+}}vu#1, [ts, lc0<<2, 12]
# CHECK: C.BSTOP
