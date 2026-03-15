# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	body_local_label_branches
	.type	body_local_label_branches,@function
body_local_label_branches:
	b.nz	.Ltake0
	j	.Ljoin0
.Ltake0:
	b.z	.Ljoin0
	j	.Ljoin1
.Ljoin0:
	j	.Ldone
.Ljoin1:
	j	.Ldone
.Ldone:
	C.BSTOP
	.size	body_local_label_branches, .-body_local_label_branches

# CHECK-LABEL: <body_local_label_branches>:
# CHECK: b.nz
# CHECK: j
# CHECK: b.z
# CHECK: j
# CHECK: j
# CHECK: j
# CHECK: C.BSTOP
