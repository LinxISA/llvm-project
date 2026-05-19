# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -r - | FileCheck %s

	.text
	.globl caller
	.type caller,@function
caller:
	BSTART CALL, callee
.Lret:
	setret .Lret
	C.BSTOP
	.size caller, .-caller

	.globl callee
	.type callee,@function
callee:
	C.BSTART.STD
	C.BSTOP
	.size callee, .-callee

# CHECK: RELOCATION RECORDS FOR [.text]:
# CHECK: R_LINX_B25_PCREL{{[[:space:]]+}}callee
