# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

# v5 shared/tlsu encoding round-trip. The deleted TMATMUL*_FIXP mnemonics
# (Function 9-14) are no longer parsed; active CUBE functions (TMATMUL 0,
# TGEMV 16) and the TLSU functions remain. PTO v0.58 reissue: the retired
# 16-bit C.B.IOS is replaced by the 32-bit B.IOS (source form, TSize=0).

B.IOS S0, mask=1111
B.IOS S255, mask=1111
BSTART.CUBE TMATMUL, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TGEMV, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0
BSTART.TLSU TPREFETCH, FP32
BSTART.TLSU MGATHER.CAS, FP32
BSTART.TLSU TMOV.L2S.INSERT, FP32
BSTART.TLSU TMOV.L2S.PUBLISH, FP32
BSTART.TLSU TMOV.S2L.BROADCAST, FP32
BSTART.TLSU TMOV.S2L.EXTRACT, FP32
BSTART.TLSU GMOV, FP32
BSTART.TLSU TSTORE.SPART, FP32

# CHECK: B.IOS{{.*}}S0, mask=1111
# CHECK: B.IOS{{.*}}S255, mask=1111
# CHECK: BSTART.CUBE{{.*}}TMATMUL
# CHECK: B.FPATR{{.*}}0, 0, 0, 0, 0, 0, 0
# CHECK: BSTART.CUBE{{.*}}TGEMV
# CHECK: BSTART.TLSU{{.*}}TPREFETCH, FP32{{.*}}encoding: [0x81,0x11,0x31,0x08]
# CHECK: BSTART.TLSU{{.*}}MGATHER.CAS, FP32{{.*}}encoding: [0x81,0x11,0x81,0x08]
# CHECK: BSTART.TLSU{{.*}}TMOV.L2S.INSERT, FP32{{.*}}encoding: [0x81,0x11,0x91,0x08]
# CHECK: BSTART.TLSU{{.*}}TMOV.L2S.PUBLISH, FP32{{.*}}encoding: [0x81,0x11,0xa1,0x08]
# CHECK: BSTART.TLSU{{.*}}TMOV.S2L.BROADCAST, FP32{{.*}}encoding: [0x81,0x11,0xb1,0x08]
# CHECK: BSTART.TLSU{{.*}}TMOV.S2L.EXTRACT, FP32{{.*}}encoding: [0x81,0x11,0xc1,0x08]
# CHECK: BSTART.TLSU{{.*}}GMOV, FP32{{.*}}encoding: [0x81,0x11,0xd1,0x08]
# CHECK: BSTART.TLSU{{.*}}TSTORE.SPART, FP32{{.*}}encoding: [0x81,0x11,0xe1,0x08]

# DIS: B.IOS{{.*}}S0, mask=1111
# DIS: B.IOS{{.*}}S255, mask=1111
# DIS: BSTART.CUBE{{.*}}TMATMUL
# DIS: BSTART.TLSU{{.*}}TPREFETCH
# DIS: BSTART.TLSU{{.*}}MGATHER.CAS
# DIS: BSTART.TLSU{{.*}}TMOV.L2S.INSERT
# DIS: BSTART.TLSU{{.*}}TMOV.L2S.PUBLISH
# DIS: BSTART.TLSU{{.*}}TMOV.S2L.BROADCAST
# DIS: BSTART.TLSU{{.*}}TMOV.S2L.EXTRACT
# DIS: BSTART.TLSU{{.*}}GMOV
# DIS: BSTART.TLSU{{.*}}TSTORE.SPART
