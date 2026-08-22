# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --triple=linx64 %t | FileCheck %s

	HL.QMT.iesr a0, a1, ->a2
	HL.QPOP.er a0, ->a1, a2
	HL.QPUSH.her a0, a1, ->a2

# CHECK: HL.QMT.iesr{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QPOP.er{{[[:space:]]+}}a0, ->a1, a2
# CHECK: HL.QPUSH.her{{[[:space:]]+}}a0, a1, ->a2
