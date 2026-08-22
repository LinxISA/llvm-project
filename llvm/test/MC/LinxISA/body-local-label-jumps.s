# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	body_local_label_jumps
	.type	body_local_label_jumps,@function
body_local_label_jumps:
	j	.Ljoin0
.Ltake0:
	j	.Ljoin1
.Ljoin0:
	j	.Ldone
.Ljoin1:
	j	.Ldone
.Ldone:
	C.BSTOP
	.size	body_local_label_jumps, .-body_local_label_jumps

# CHECK-LABEL: <body_local_label_jumps>:
# CHECK-COUNT-4: j
# CHECK: C.BSTOP
