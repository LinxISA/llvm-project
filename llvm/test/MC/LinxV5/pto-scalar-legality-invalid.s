# RUN: not llvm-mc -triple=linx64v5 -show-encoding %s 2>&1 | FileCheck %s
# Every non-conversion FSU SrcType=2/3 is reserved.
# CHECK: error:
fabs.fb	a0, ->a1
# CHECK: error:
fabs.fh	a0, ->a1
# CHECK: error:
fadd.fb	a0, a0, ->a1
# CHECK: error:
fadd.fh	a0, a0, ->a1
# CHECK: error:
fdiv.fb	a0, a0, ->a1
# CHECK: error:
fdiv.fh	a0, a0, ->a1
# CHECK: error:
feq.fb	a0, a0, ->a1
# CHECK: error:
feq.fh	a0, a0, ->a1
# CHECK: error:
feqs.fb	a0, a0, ->a1
# CHECK: error:
feqs.fh	a0, a0, ->a1
# CHECK: error:
fexp.fb	a0, ->a1
# CHECK: error:
fexp.fh	a0, ->a1
# CHECK: error:
fge.fb	a0, a0, ->a1
# CHECK: error:
fge.fh	a0, a0, ->a1
# CHECK: error:
fges.fb	a0, a0, ->a1
# CHECK: error:
fges.fh	a0, a0, ->a1
# CHECK: error:
flt.fb	a0, a0, ->a1
# CHECK: error:
flt.fh	a0, a0, ->a1
# CHECK: error:
flts.fb	a0, a0, ->a1
# CHECK: error:
flts.fh	a0, a0, ->a1
# CHECK: error:
fmadd.fb	a0, a0, a0, ->a1
# CHECK: error:
fmadd.fh	a0, a0, a0, ->a1
# CHECK: error:
fmax.fb	a0, a0, ->a1
# CHECK: error:
fmax.fh	a0, a0, ->a1
# CHECK: error:
fmin.fb	a0, a0, ->a1
# CHECK: error:
fmin.fh	a0, a0, ->a1
# CHECK: error:
fmsub.fb	a0, a0, a0, ->a1
# CHECK: error:
fmsub.fh	a0, a0, a0, ->a1
# CHECK: error:
fmul.fb	a0, a0, ->a1
# CHECK: error:
fmul.fh	a0, a0, ->a1
# CHECK: error:
fne.fb	a0, a0, ->a1
# CHECK: error:
fne.fh	a0, a0, ->a1
# CHECK: error:
fnes.fb	a0, a0, ->a1
# CHECK: error:
fnes.fh	a0, a0, ->a1
# CHECK: error:
fnmadd.fb	a0, a0, a0, ->a1
# CHECK: error:
fnmadd.fh	a0, a0, a0, ->a1
# CHECK: error:
fnmsub.fb	a0, a0, a0, ->a1
# CHECK: error:
fnmsub.fh	a0, a0, a0, ->a1
# CHECK: error:
frecip.fb	a0, ->a1
# CHECK: error:
frecip.fh	a0, ->a1
# CHECK: error:
fsqrt.fb	a0, ->a1
# CHECK: error:
fsqrt.fh	a0, ->a1
# CHECK: error:
fsub.fb	a0, a0, ->a1
# CHECK: error:
fsub.fh	a0, a0, ->a1
# CHECK: error:
acre 2
# CHECK: error:
acre 3
# CHECK: error:
acre 4
# CHECK: error:
acre 5
# CHECK: error:
acre 6
# CHECK: error:
acre 7
# CHECK: error:
acre 8
# CHECK: error:
acre 9
# CHECK: error:
acre 10
# CHECK: error:
acre 11
# CHECK: error:
acre 12
# CHECK: error:
acre 13
# CHECK: error:
acre 14
# CHECK: error:
acre 15
# CHECK: error:
fence.d -1, 0
# CHECK: error:
fence.d 16, 1
# CHECK: error:
fence.d 0, 16
# CHECK: error:
fence.d 0, -1
# CHECK: error:
fence.d bad, r
# CHECK: error:
fence.d "r", w
# CHECK: error:
c.setret -1, ->ra
# CHECK: error:
c.setret 32, ->ra
# CHECK: error:
trap zero
