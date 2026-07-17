# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

	.text
	.globl	test_v057_reserved_encoding_reject
	.type	test_v057_reserved_encoding_reject,@function
test_v057_reserved_encoding_reject:
	# B.CATR with reserved bit 15 set.
	.long 0x00008023
	# B.HINT TRACE.begin with reserved bit 16 set.
	.long 0x00011033
	# BSTART.STD RET with reserved bit 15 set.
	.long 0x0000f001
	# BSTART.FP ICALL with reserved bit 15 set.
	.long 0x0000e101
	.size	test_v057_reserved_encoding_reject, .-test_v057_reserved_encoding_reject

# CHECK-LABEL: <test_v057_reserved_encoding_reject>:
# CHECK-COUNT-4: <unknown>
# CHECK-NOT: B.CATR
# CHECK-NOT: B.HINT
# CHECK-NOT: BSTART
