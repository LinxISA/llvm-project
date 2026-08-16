# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

# CHECK-LABEL: <foo>:
# CHECK: BSTART.CALL{{[[:space:]]+}}bar, 0x4, ->ra
# CHECK-NOT: setret

	.text
	.globl	foo
	.type	foo,@function
foo:
	# PTO ISA 0.58.1 returning calls are a single atomic instruction.
	BSTART.CALL	bar, foo_ret, ->ra
foo_ret:
	C.BSTOP
	.size	foo, .-foo

	.globl	bar
	.type	bar,@function
bar:
	C.BSTOP
	.size	bar, .-bar
