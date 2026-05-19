# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64 -filetype=obj %t/attrs.s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %t/attrs.s
# RUN: llvm-mc -triple=linx64 -filetype=obj %t/tpcrel.s -o - | llvm-objdump -r - | FileCheck %t/tpcrel.s
# RUN: llvm-mc -triple=linx64 -filetype=asm %t/directives.s -o - | FileCheck %t/directives.s

#--- attrs.s
	.text
	.globl attrs
	.type attrs,@function
attrs:
	B.CATR trap, atomic, aqrl, far, dr
	B.DATR normal, fp32, zero
	C.BSTOP
	.size attrs, .-attrs

# CHECK-LABEL: <attrs>:
# CHECK: B.CATR trap, atomic, aqrl, far, dr
# CHECK: B.DATR normal, fp32, zero
# CHECK: C.BSTOP

#--- tpcrel.s
	.text
	.globl tpcrel
	.type tpcrel,@function
tpcrel:
	addtpc %tpcrel_hi(sym), ->a1
	addi   a1, %tpcrel_lo(sym), ->a1
	C.BSTOP
	.size tpcrel, .-tpcrel

	.section .data
sym:
	.word 0

# CHECK: RELOCATION RECORDS FOR [.text]:
# CHECK-DAG: R_LINX_PCREL_HI20{{[[:space:]]+}}sym
# CHECK-DAG: R_LINX_LO12{{[[:space:]]+}}sym

#--- directives.s
	.text
directives:
	.word 0
	.half 1
	.dword 2

# CHECK: .word 0
# CHECK: .half 1
# CHECK: .dword 2
