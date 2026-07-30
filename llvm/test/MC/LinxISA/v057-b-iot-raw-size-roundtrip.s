# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=CHECK
# RUN: llvm-mc -triple=linx32 -filetype=obj %s -o - | llvm-objdump -d --triple=linx32 - | FileCheck %s --check-prefix=CHECK

	.text
	.globl test_v057_b_iot_raw_size_roundtrip
test_v057_b_iot_raw_size_roundtrip:
	B.IOT t#1.reuse, last, ->u<3>
	B.IOT t#1.reuse, t#2, last, ->u<3>
	B.IOT last, ->u<3>
	C.BSTOP

# CHECK-LABEL: <test_v057_b_iot_raw_size_roundtrip>:
# CHECK: B.IOT{{[[:space:]]+}}t#1.reuse, last,{{[[:space:]]+}}->u<128B>
# CHECK: B.IOT{{[[:space:]]+}}t#1.reuse, t#2, last,{{[[:space:]]+}}->u<128B>
# CHECK: B.IOT{{[[:space:]]+}}last,{{[[:space:]]+}}->u<128B>
# CHECK: C.BSTOP
