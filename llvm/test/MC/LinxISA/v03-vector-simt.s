# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v04_vector_simt
	.type	test_v04_vector_simt,@function
test_v04_vector_simt:
	BSTART.MSEQ 0
	B.TEXT .body
	B.ARG NORM.normal
	B.IOR [s0],[]
	B.IOT last, ->t<4KB>
	C.B.DIMI 64, ->lb0
	C.B.DIMI 32, ->lb1
	C.BSTART
.body:
	v.add	lc0.sw, lc1.sw, ->vt.w
	v.lw.brg	[ri0, lc0<<2, lc1<<10], ->vt.w
	v.fadd	vt#1, zero, ->vt
	v.fmul	vt#1, zero, ->vt
	v.sw.brg	vt#1, [ri0, lc0<<2, lc1<<10]
	v.sw.local	vt#1, [to, lc0<<2, lc1<<6]
	v.lw.local	[to, lc0<<2, lc1<<6], ->vt.w
	C.BSTOP
	.size	test_v04_vector_simt, .-test_v04_vector_simt

# CHECK-LABEL: <test_v04_vector_simt>:
# CHECK: BSTART.MSEQ
# CHECK: B.TEXT
# CHECK: B.ARG{{[[:space:]]+}}NORM.normal
# CHECK: B.IOR{{[[:space:]]+}}[s0],[]{{[[:space:]]*$}}
# CHECK: B.IOT{{[[:space:]]+}}last,{{[[:space:]]+}}->t<4KB>
# CHECK: C.B.DIMI{{[[:space:]]+}}64, {{[[:space:]]*}}->lb0
# CHECK: C.B.DIMI{{[[:space:]]+}}32, {{[[:space:]]*}}->lb1
# CHECK: C.BSTART
# CHECK: v.add{{[[:space:]]+}}lc0, lc1{{.*}}->vt
# CHECK: v.lw.brg{{[[:space:]]+}}[ri0, lc0<<2, lc1<<10], ->vt
# CHECK: v.fadd{{[[:space:]]+}}vt#1, zero,{{.*}}->vt
# CHECK: v.fmul{{[[:space:]]+}}vt#1, zero,{{.*}}->vt
# CHECK: v.sw.brg{{[[:space:]]+}}vt#1, [ri0, lc0<<2, lc1<<10]
# CHECK: v.sw.local{{[[:space:]]+}}vt#1, [to, lc0<<2, lc1<<6]
# CHECK: v.lw.local{{[[:space:]]+}}[to, lc0<<2, lc1<<6], ->vt
# CHECK: C.BSTOP
