# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

HL.QMT.iesr a0, a1, ->a2
HL.QMT.ei a0, a1, ->a2
HL.QMT.ii a0, a1, ->a2
HL.QMT.e a0, a1, ->a2
HL.QMT.i a0, ->a2
HL.QPOP.i a0, ->a1, a2
HL.QPOP.s a0, ->a1, a2
HL.QPOP.ee a0, ->a1, a2
HL.QPUSH.i a0, a1, ->a2
HL.QPUSH.s a0, a1, ->a2
HL.QPUSH.eh a0, a1, ->a2
HL.QPUSH.hh a0, a1, ->a2

# CHECK-COUNT-12: error:
