# RUN: not llvm-mc -triple=linx64v5 -show-encoding %s 2>&1 | FileCheck %s

# PTO-ISA ADR 0069 negative cases: SizeCode out of range / reserved, PE masks
# with no PEMode, B.IOT legacy TSize violating the Local 1..10 contract, and
# B.IOS destination spelling with SizeCode 0 (silent source) must all fail.

# CHECK: B.IOT{{.*}}->t<128KB
B.IOT mask=1111, last, ->t<128KB>
# CHECK: B.IOT{{.*}}->t<256KB
B.IOT mask=1111, last, ->t<256KB>
# CHECK: B.IOS{{.*}}->S0<512KB
B.IOS mask=1111, ->S0<512KB>
# CHECK: B.IOS{{.*}}->S0<13>
B.IOS mask=1111, ->S0<13>
# CHECK: PE mask has no PEMode encoding
B.IOS S1, mask=0110
# CHECK: PE mask has no PEMode encoding
B.IOT mask=0110, last, ->t<128B>

# B.IOT legacy "TSize=N" destination spelling honors the Local 1..10 contract.
# CHECK: TSize=0
B.IOT mask=1111, TSize=0, last, ->t
# CHECK: TSize=11
B.IOT mask=1111, TSize=11, last, ->t
# CHECK: TSize=12
B.IOT mask=1111, TSize=12, last, ->t
# CHECK: TSize=13
B.IOT t#1, mask=1111, TSize=13, last, ->u

# B.IOS destination must carry 1..12; "->S0<0B>" would silently become source.
# CHECK: B.IOS{{.*}}->S0<0B>
B.IOS mask=1111, ->S0<0B>
# CHECK: B.IOS{{.*}}->S0<0>
B.IOS mask=1111, ->S0<0>
# CHECK: B.IOS{{.*}}->S0<13>>
B.IOS mask=1111, ->S0<13>>