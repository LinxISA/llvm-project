# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

# CHECK-LABEL: <foo>:
# CHECK: hl.lwip{{[[:space:]]+}}[s0, 0],{{[[:space:]]+}}->s1, s2
# CHECK: hl.lwip.u{{[[:space:]]+}}[s0, 3],{{[[:space:]]+}}->s1, s2
# CHECK: hl.lwi.po{{[[:space:]]+}}[s0, 4],{{[[:space:]]+}}->s1, s3
# CHECK: hl.lwi.upo{{[[:space:]]+}}[s0, 4],{{[[:space:]]+}}->s1, s3
# CHECK: hl.swi.po{{[[:space:]]+}}s1, [s0, 4],{{[[:space:]]+}}->s3
# CHECK: hl.swi.upo{{[[:space:]]+}}s1, [s0, 4],{{[[:space:]]+}}->s3
# CHECK: hl.swip{{[[:space:]]+}}s1, s2, [s0, 8]
# CHECK: hl.swip.u{{[[:space:]]+}}s1, s2, [s0, 8]
# CHECK: hl.sw.po{{[[:space:]]+}}s1, [s0, s2.uw<<2],{{[[:space:]]+}}->s3
# CHECK: hl.lw.po{{[[:space:]]+}}[s0, s2.uw<<1],{{[[:space:]]+}}->s1, s3

	.text
	.globl	foo
	.type	foo,@function
foo:
	hl.lwip		[s0, 0], ->s1, s2
	hl.lwip.u	[s0, 3], ->s1, s2

	hl.lwi.po	[s0, 4], ->s1, s3
	hl.lwi.upo	[s0, 4], ->s1, s3

	hl.swi.po	s1, [s0, 4], ->s3
	hl.swi.upo	s1, [s0, 4], ->s3

	hl.swip		s1, s2, [s0, 8]
	hl.swip.u	s1, s2, [s0, 8]

	hl.sw.po	s1, [s0, s2.uw], ->s3
	hl.lw.po	[s0, s2.uw<<1], ->s1, s3

	C.BSTOP
	.size	foo, .-foo

