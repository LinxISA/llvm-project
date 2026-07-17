# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_vpar_vseq_encoding
	.type	test_v057_vpar_vseq_encoding,@function
test_v057_vpar_vseq_encoding:
	BSTART.VPAR 0
	BSTART.VSEQ 0
	C.BSTOP
	.size	test_v057_vpar_vseq_encoding, .-test_v057_vpar_vseq_encoding

# CHECK-LABEL: <test_v057_vpar_vseq_encoding>:
# CHECK: BSTART.VPAR
# CHECK: BSTART.VSEQ
# CHECK: C.BSTOP
# CHECK-NOT: BSTART.TVEC
