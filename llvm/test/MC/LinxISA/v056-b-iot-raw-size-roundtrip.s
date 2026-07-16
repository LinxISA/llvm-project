# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=CHECK
# RUN: llvm-mc -triple=linx32 -filetype=obj %s -o - | llvm-objdump -d --triple=linx32 - | FileCheck %s --check-prefix=CHECK

	.text
	.globl test_v056_b_iot_raw_size_roundtrip
test_v056_b_iot_raw_size_roundtrip:
	B.IOT t#1.reuse, last, ->u<1>
	B.IOT t#1.reuse, t#2, last, ->u<1>
	B.IOT last, ->u<1>
	C.BSTOP

# CHECK-LABEL: <test_v056_b_iot_raw_size_roundtrip>:
# CHECK: B.IOT{{[[:space:]]+}}t#1.reuse, last,{{[[:space:]]+}}->u<1>
# CHECK: B.IOT{{[[:space:]]+}}t#1.reuse, t#2, last,{{[[:space:]]+}}->u<1>
# CHECK: B.IOT{{[[:space:]]+}}last,{{[[:space:]]+}}->u<1>
# CHECK: C.BSTOP
