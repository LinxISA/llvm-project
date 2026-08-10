# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=CHECK
# RUN: llvm-mc -triple=linx32 -filetype=obj %s -o - | llvm-objdump -d --triple=linx32 - | FileCheck %s --check-prefix=CHECK

	.text
	.globl test_v058_b_iot_raw_size_roundtrip
test_v058_b_iot_raw_size_roundtrip:
	B.IOT t#1, mask=1111, last, ->u<128B>
	B.IOT t#1, t#2, mask=1111, last, ->u<128B>
	B.IOT mask=1111, last, ->u<128B>
	C.BSTOP

# CHECK-LABEL: <test_v058_b_iot_raw_size_roundtrip>:
# CHECK: B.IOT{{[[:space:]]+}}t#1, mask=1111, last,{{[[:space:]]+}}->u<128B>
# CHECK: B.IOT{{[[:space:]]+}}t#1, t#2, mask=1111, last,{{[[:space:]]+}}->u<128B>
# CHECK: B.IOT{{[[:space:]]+}}mask=1111, last,{{[[:space:]]+}}->u<128B>
# CHECK: C.BSTOP
