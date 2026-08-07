# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

# v5 shared/tlsu encoding round-trip. The deleted TMATMUL*_FIXP mnemonics
# (Function 9-14) are no longer parsed; active CUBE functions (TMATMUL 0,
# TGEMV 16) and the TLSU/GMOV functions remain. PTO v0.58 reissue: the retired
# 16-bit C.B.IOS is replaced by the 32-bit B.IOS (source form, TSize=0).

B.IOS S0, mask=1111
B.IOS S255, mask=1111
BSTART.CUBE TMATMUL, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TGEMV, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0
BSTART.TLSU TMOV.L2S.INSERT, FP32
BSTART.TLSU TMOV.L2S.PUBLISH, FP32
BSTART.TLSU TMOV.S2L.BROADCAST, FP32
BSTART.TLSU TMOV.S2L.EXTRACT, FP32
BSTART.TLSU TSTORE.SPART, FP32
BSTART.TLSU GMOV, FP32

# CHECK: B.IOS{{.*}}S0, mask=1111
# CHECK: B.IOS{{.*}}S255, mask=1111
# CHECK: BSTART.CUBE{{.*}}TMATMUL
# CHECK: B.FPATR{{.*}}0, 0, 0, 0, 0, 0, 0
# CHECK: BSTART.CUBE{{.*}}TGEMV
# CHECK: BSTART.TLSU{{.*}}TMOV.L2S.INSERT
# CHECK: BSTART.TLSU{{.*}}TMOV.L2S.PUBLISH
# CHECK: BSTART.TLSU{{.*}}TMOV.S2L.BROADCAST
# CHECK: BSTART.TLSU{{.*}}TMOV.S2L.EXTRACT
# CHECK: BSTART.TLSU{{.*}}TSTORE.SPART
# CHECK: BSTART.TLSU{{.*}}GMOV

# DIS: B.IOS{{.*}}S0, mask=1111
# DIS: B.IOS{{.*}}S255, mask=1111
# DIS: BSTART.CUBE{{.*}}TMATMUL
# DIS: BSTART.TLSU{{.*}}GMOV
