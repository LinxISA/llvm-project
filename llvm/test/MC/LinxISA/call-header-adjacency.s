# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64 -filetype=obj %t/ok.s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %t/ok.s
# RUN: not llvm-mc -triple=linx64 -filetype=asm %t/bad-ra.s -o /dev/null 2>&1 | FileCheck %t/bad-ra.s

#--- ok.s
# CHECK-LABEL: <caller>:
# CHECK: BSTART.CALL{{[[:space:]]+}}callee, 0x4, ->ra
# CHECK-NOT: setret
	.text
	.globl caller
	.type caller,@function
caller:
	BSTART.CALL callee, .Lret, ->ra
.Lret:
	C.BSTOP
	.size caller, .-caller

	.globl callee
	.type callee,@function
callee:
	C.BSTOP
	.size callee, .-callee

#--- bad-ra.s
# CHECK: error: expected 'CALL' for fused BSTART 'ra=' syntax
	.text
	BSTART DIRECT, callee, ra=.Lret
