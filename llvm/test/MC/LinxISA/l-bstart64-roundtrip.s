# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --triple=linx64 %t.o | FileCheck %s --check-prefix=DIS
# RUN: llvm-objdump -d --triple=linx64 --no-leading-addr --no-show-raw-insn %t.o \
# RUN:   | sed -n '/L.BSTART/p' \
# RUN:   | llvm-mc -triple=linx64 -filetype=obj -o %t.roundtrip.o
# RUN: llvm-objdump -d --triple=linx64 %t.roundtrip.o | FileCheck %s --check-prefix=ROUNDTRIP
# RUN: llvm-objcopy --only-section=.text -O binary %t.o %t.bin
# RUN: llvm-objcopy --only-section=.text -O binary %t.roundtrip.o %t.roundtrip.bin
# RUN: cmp %t.bin %t.roundtrip.bin

	.text
l_bstart64_roundtrip:
	L.BSTART.STD FALL
	L.BSTART.STD DIRECT, .Ltarget
	L.BSTART.STD COND, .Ltarget
	L.BSTART.STD CALL, .Ltarget
	L.BSTART.FP FALL
	L.BSTART.FP DIRECT, .Ltarget
	L.BSTART.FP COND, .Ltarget
	L.BSTART.FP CALL, .Ltarget
	L.BSTART.SYS FALL
.Ltarget:
	L.BSTART.STD FALL, 2
	L.BSTART.FP FALL, -2
	L.BSTART.STD DIRECT, 67108862
	L.BSTART.FP DIRECT, 67108864

# DIS-LABEL: <l_bstart64_roundtrip>:
# DIS: 0f 00 00 00 01 10 00 00{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}FALL
# DIS-NEXT: 0f 10 00 00 01 20 00 00{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}DIRECT,
# DIS-NEXT: 0f 0e 00 00 01 30 00 00{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}COND,
# DIS-NEXT: 0f 0c 00 00 01 40 00 00{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}CALL,
# DIS-NEXT: 0f 00 00 00 81 10 00 00{{[[:space:]]+}}L.BSTART.FP{{[[:space:]]+}}FALL
# DIS-NEXT: 0f 08 00 00 81 20 00 00{{[[:space:]]+}}L.BSTART.FP{{[[:space:]]+}}DIRECT,
# DIS-NEXT: 0f 06 00 00 81 30 00 00{{[[:space:]]+}}L.BSTART.FP{{[[:space:]]+}}COND,
# DIS-NEXT: 0f 04 00 00 81 40 00 00{{[[:space:]]+}}L.BSTART.FP{{[[:space:]]+}}CALL,
# DIS-NEXT: 0f 00 00 00 11 10 00 00{{[[:space:]]+}}L.BSTART.SYS{{[[:space:]]+}}FALL
# DIS-NEXT: 8f 00 00 00 01 10 00 00{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}FALL, 2
# DIS-NEXT: 8f ff ff ff 81 90 ff ff{{[[:space:]]+}}L.BSTART.FP{{[[:space:]]+}}FALL, -2
# DIS-NEXT: 8f ff ff ff 01 20 00 00{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}DIRECT, 67108862
# DIS-NEXT: 0f 00 00 00 81 a0 00 00{{[[:space:]]+}}L.BSTART.FP{{[[:space:]]+}}DIRECT, 67108864

# ROUNDTRIP: L.BSTART.STD{{[[:space:]]+}}FALL
# ROUNDTRIP-NEXT: L.BSTART.STD{{[[:space:]]+}}DIRECT,
# ROUNDTRIP-NEXT: L.BSTART.STD{{[[:space:]]+}}COND,
# ROUNDTRIP-NEXT: L.BSTART.STD{{[[:space:]]+}}CALL,
# ROUNDTRIP-NEXT: L.BSTART.FP{{[[:space:]]+}}FALL
# ROUNDTRIP-NEXT: L.BSTART.FP{{[[:space:]]+}}DIRECT,
# ROUNDTRIP-NEXT: L.BSTART.FP{{[[:space:]]+}}COND,
# ROUNDTRIP-NEXT: L.BSTART.FP{{[[:space:]]+}}CALL,
# ROUNDTRIP-NEXT: L.BSTART.SYS{{[[:space:]]+}}FALL
# ROUNDTRIP-NEXT: L.BSTART.STD{{[[:space:]]+}}FALL, 2
# ROUNDTRIP-NEXT: L.BSTART.FP{{[[:space:]]+}}FALL, -2
# ROUNDTRIP-NEXT: L.BSTART.STD{{[[:space:]]+}}DIRECT, 67108862
# ROUNDTRIP-NEXT: L.BSTART.FP{{[[:space:]]+}}DIRECT, 67108864
