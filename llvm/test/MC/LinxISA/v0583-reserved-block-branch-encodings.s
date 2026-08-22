# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl reserved_block_branches
reserved_block_branches:
	# PTO ISA 0.58.3 reserves the eight retired two-level body-branch slots.
	.long 0x00000027
	.long 0x00001027
	.long 0x00002027
	.long 0x00003027
	.long 0x00004027
	.long 0x00005027
	.long 0x00001037
	.long 0x00002037

# CHECK-LABEL: <reserved_block_branches>:
# CHECK-COUNT-8: <unknown>
# CHECK-NOT: B.EQ
# CHECK-NOT: B.NE
# CHECK-NOT: B.LT
# CHECK-NOT: B.GE
# CHECK-NOT: B.LTU
# CHECK-NOT: B.GEU
# CHECK-NOT: B.Z
# CHECK-NOT: B.NZ
