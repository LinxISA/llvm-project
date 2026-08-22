# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --triple=linx64 %t | FileCheck %s

	HL.QMT.i a0, a1, ->a2
	HL.QMT.e a0, ->a2
	HL.QMT.s a0, ->a2
	HL.QMT.r a0, ->a2
	HL.QMT.ie a0, a1, ->a2
	HL.QMT.is a0, a1, ->a2
	HL.QMT.ir a0, a1, ->a2
	HL.QMT.es a0, ->a2
	HL.QMT.er a0, ->a2
	HL.QMT.ies a0, a1, ->a2
	HL.QMT.ier a0, a1, ->a2
	HL.QPOP.e a0, ->a1, a2
	HL.QPOP.r a0, ->a1, a2
	HL.QPOP.er a0, ->a1, a2
	HL.QPUSH.h a0, a1, ->a2
	HL.QPUSH.e a0, a1, ->a2
	HL.QPUSH.r a0, a1, ->a2
	HL.QPUSH.he a0, a1, ->a2
	HL.QPUSH.hr a0, a1, ->a2
	HL.QPUSH.er a0, a1, ->a2
	HL.QPUSH.her a0, a1, ->a2

# CHECK: HL.QMT.i{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QMT.e{{[[:space:]]+}}a0, ->a2
# CHECK: HL.QMT.s{{[[:space:]]+}}a0, ->a2
# CHECK: HL.QMT.r{{[[:space:]]+}}a0, ->a2
# CHECK: HL.QMT.ie{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QMT.is{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QMT.ir{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QMT.es{{[[:space:]]+}}a0, ->a2
# CHECK: HL.QMT.er{{[[:space:]]+}}a0, ->a2
# CHECK: HL.QMT.ies{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QMT.ier{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QPOP.e{{[[:space:]]+}}a0, ->a1, a2
# CHECK: HL.QPOP.r{{[[:space:]]+}}a0, ->a1, a2
# CHECK: HL.QPOP.er{{[[:space:]]+}}a0, ->a1, a2
# CHECK: HL.QPUSH.h{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QPUSH.e{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QPUSH.r{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QPUSH.he{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QPUSH.hr{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QPUSH.er{{[[:space:]]+}}a0, a1, ->a2
# CHECK: HL.QPUSH.her{{[[:space:]]+}}a0, a1, ->a2
