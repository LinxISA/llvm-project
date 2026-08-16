# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -r - | FileCheck %s

	.text
	.globl caller
	.type caller,@function
caller:
		# Local returning call keeps the BSTART relocation so the linker can
		# recompute the call target after relaxation/layout. The return address is
		# encoded directly by the PTO/Linx 0.58.1 atomic CALL form.
	BSTART.CALL callee, .Lret, ->ra
.Lret:
	C.BSTOP
	.size caller, .-caller

	.globl callee
	.type callee,@function
callee:
	C.BSTART.STD
	C.BSTOP
	.size callee, .-callee

# CHECK: RELOCATION RECORDS FOR [.text]:
# CHECK: R_LINX_CBSTART12_PCREL{{[[:space:]]+}}callee
