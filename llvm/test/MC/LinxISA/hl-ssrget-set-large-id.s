# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

# CHECK-LABEL: <foo>:
# CHECK: hl.ssrget {{(0x1f06|ETEMP0_ACRn)}}, ->x0
# CHECK: hl.ssrset x0, {{(0x1f06|ETEMP0_ACRn)}}
# CHECK: hl.ssrget {{(0x1f51|EBARG_TPLFLAGS_ACRn)}}, ->x1
# CHECK: hl.ssrset x1, {{(0x1f51|EBARG_TPLFLAGS_ACRn)}}

	.text
	.globl	foo
	.type	foo,@function
foo:
	hl.ssrget	0x1f06, ->x0
	hl.ssrset	x0, 0x1f06
	hl.ssrget	0x1f51, ->x1
	hl.ssrset	x1, 0x1f51
	C.BSTOP
	.size	foo, .-foo
