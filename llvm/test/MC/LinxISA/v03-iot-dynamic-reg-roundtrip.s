# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

	.text
	.globl	test_v056_iot_reject_dynamic_size
	.type	test_v056_iot_reject_dynamic_size,@function
test_v056_iot_reject_dynamic_size:
	BSTART.MSEQ 0
	B.IOT t#1.reuse, ->t<a0>
	C.BSTOP
	.size	test_v056_iot_reject_dynamic_size, .-test_v056_iot_reject_dynamic_size

# CHECK: error: B.IOT expects size suffix '->t<Size>'
# CHECK-NEXT: {{.*}}B.IOT t#1.reuse, ->t<a0>
