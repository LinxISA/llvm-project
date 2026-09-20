# RUN: llvm-mc -triple linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s

# SuperScalarModel#740: a TEPL reduce-expand bundle whose ValidCol is the
# constant one must keep its encoded B.DIM LB0 line. The per-block default
# of one only applies to omitted LB1/LB2; dropping the required LB0 made
# gfrun reject the legal [1,1] -> [32,1] TCOLEXPAND at
# ValidateReduceAndExpandTepl (bdimMask=0b010 instead of 0b001|0b010).
# This is the exact CodeGen output of TileOP TCOLEXPAND(dst [32,1] v32v1,
# src [1,1]) with the fix; the LB2 line (Col=1, default of ValidCol) stays
# elided because LB2 legitimately defaults to ValidCol.

# CHECK-LABEL: <tcolexpand_lb0_1>:
# CHECK: BSTART.TEPL TCOLEXPAND, FP32
# CHECK: C.B.DIMI 1, ->lb0
# CHECK: C.B.DIMI 32, ->lb1
# CHECK-NOT: C.B.DIMI
# CHECK: B.IOT t#1, mask=1111, last, ->t<128B>
# CHECK: BSTOP

	.text
	.globl	tcolexpand_lb0_1
tcolexpand_lb0_1:
	//APP
	BSTART.TEPL	TCOLEXPAND, FP32
	C.B.DIMI	1, 	->lb0
	C.B.DIMI	32, 	->lb1
	B.IOT	t#1, mask=1111, last, 	->t<128B>
	//NO_APP
	BSTOP
