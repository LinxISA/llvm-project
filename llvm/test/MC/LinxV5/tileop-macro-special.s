# RUN: llvm-mc %s --triple=linx64v5 --show-encoding \
# RUN:   | FileCheck %s --dump-input=always
# RUN: not llvm-mc %s --triple=linx64v5 --defsym=NEG=1 2>&1 \
# RUN:   | FileCheck %s --check-prefix=NEG

# PTO 0.58.6 single-line TileOp macro assembly (issue #90): special
# operand carriers — Shared groups (bare S<n>), predicate tile and
# predicate GPR destinations, and the multi-form carrier re-resolution.

.ifdef NEG
# NEG: TileOp macro: operand shape does not match
TCMP <32, FP32, LT>, t#1, t#2, ->t#4<512B>, ->a0
.else

# Predicate-tile destination carrier.
TCMP <32, FP32, LT>, t#1, t#2, ->t#4<512B>
# CHECK: BSTART.TEPL TCMP, FP32
# CHECK: B.DATR NORM.normal, Null, LT
# CHECK: B.IOT t#1, t#2, mask=1111, last, ->t#4<512B>
# CHECK: BSTOP

# Predicate-GPR destination carrier (re-resolution by operand shape).
TCMP <32, FP32, LT>, t#1, t#2, ->a0
# CHECK: BSTART.TEPL TCMP, FP32
# CHECK: B.IOT t#1, mask=1111
# CHECK: B.IOT t#2, mask=1111
# CHECK: B.IOR [], ->a0
# CHECK: BSTOP

# Scalar compare with GPR predicate.
TCMPS <32, FP32, LT>, t#1, a1, ->a0
# CHECK: BSTART.TEPL TCMPS, FP32
# CHECK: B.IOT t#1, mask=1111
# CHECK: B.IOR [], ->a0
# CHECK: BSTOP

# Shared right group: bare S<n> rides a source B.IOS record.
TMATMUL.SHARED_RIGHT <16, 32, 64, FP32>, t#1, S2, ->t#3<2KB>
# CHECK: BSTART.CUBE TMATMUL, FP32
# CHECK: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
# CHECK: B.IOT t#1, mask=1111
# CHECK: B.IOS S2, mask=1111
# CHECK: B.IOT mask=1111, last, ->t#3<2KB>
# CHECK: BSTOP

# Shared both groups.
TMATMUL.SHARED_BOTH <16, 32, 64, FP32>, S1, S2, ->t#3<2KB>
# CHECK: BSTART.CUBE TMATMUL, FP32
# CHECK: B.IOS S1, mask=1111
# CHECK: B.IOS S2, mask=1111
# CHECK: B.IOT mask=1111, last, ->t#3<2KB>
# CHECK: BSTOP

# Shared destination (TLOAD.SHARED): B.IOS destination form.
TLOAD.SHARED <32, 16, 32, FP32>, [a0, a1], ->S3<2KB>
# CHECK: BSTART.TLSU TLOAD, FP32
# CHECK: B.IOS mask=1111, ->S3<2KB>
# CHECK: B.IOR [a0, a1], []
# CHECK: BSTOP

.endif
