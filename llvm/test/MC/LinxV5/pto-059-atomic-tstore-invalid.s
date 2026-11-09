# RUN: not llvm-mc -triple=linx64v5 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s
TSTORE.ATOMIC_INC <Encoding=E21, U64>, T#1, [a0, a1]
TSTORE.ATOMIC_ADD <Encoding=E3, S64>, T#1, [a0, a1]
TSTORE.ATOMIC_MIN <Encoding=E6, FP32>, T#1, [a0, a1]
TSTORE.ATOMIC_ADD <Encoding=E3, U32, Canonicalize=0>, T#1, [a0, a1]
TSTORE.ATOMIC_ADD <Encoding=E3, U32, Canonicalize=1, CMode=MIN>, T#1, [a0, a1]
TSTORE.ATOMIC_ADD <Encoding=E5, U32, N82ND>, T#1, [a0, a1]
# CHECK-COUNT-6: error: TileOp operands/configuration do not match any exact PTO 0.59 form
