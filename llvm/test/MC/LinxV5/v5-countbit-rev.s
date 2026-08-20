# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=DIS

# Scalar bit-op family: ctz/clz/bcnt use "ctz SrcL, M, N, ->Rd" over an
# N-bit field starting at M. REV reverses bytes within the same field with
# the same operand layout (lsb26 = raw M, lsb20 = N-1); non-byte widths
# return zero per the ISA contract. All six mnemonics round-trip.

ctz a0, 0, 64, ->a1
clz a1, 3, 16, ->a2
bcnt a2, 8, 8, ->a3
bic a0, 1, 8, ->a1
rev a0, 0, 64, ->a1
rev a2, 8, 16, ->a3

# ENC: ctz{{.*}}0, 64,{{.*}}
# ENC: clz{{.*}}3, 16,{{.*}}
# ENC: bcnt{{.*}}8, 8,{{.*}}
# ENC: rev{{.*}}0, 64,{{.*}}
# ENC: rev{{.*}}8, 16,{{.*}}

# DIS: ctz{{.*}}0, 64,{{.*}}
# DIS: clz{{.*}}3, 16,{{.*}}
# DIS: bcnt{{.*}}8, 8,{{.*}}
# DIS: rev{{.*}}0, 64,{{.*}}
# DIS: rev{{.*}}8, 16,{{.*}}