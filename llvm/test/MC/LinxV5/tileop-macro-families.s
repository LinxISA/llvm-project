# RUN: llvm-mc %s --triple=linx64v5 --show-encoding \
# RUN:   | FileCheck %s --dump-input=always
# RUN: not llvm-mc %s --triple=linx64v5 --defsym=NEG=1 2>&1 \
# RUN:   | FileCheck %s --check-prefix=NEG

# PTO 0.58.6 single-line TileOp macro assembly (issue #90): SFU, TLSU, and
# CUBE families alongside the VEC family covered by tileop-macro-vec.s.

.ifdef NEG
# NEG: TileOp macro: operand shape does not match
TROWMAX <32, FP32>, t#1, t#2, ->t#3<1KB>
.else

# --- SFU: row reduction, single source, split records.
TROWMAX <32, 16, FP32>, t#1, ->t#2<1KB>
# CHECK: BSTART.TEPL TROWMAX, FP32
# CHECK: C.B.DIMI 32, ->lb0
# CHECK: C.B.DIMI 16, ->lb1
# CHECK: B.IOT t#1, mask=1111
# CHECK: B.IOT mask=1111, last, ->t#2<1KB>
# CHECK: BSTOP

# --- SFU: two-source gather.
TGATHER <32, FP32>, t#1, t#2, ->t#3<2KB>
# CHECK: BSTART.TEPL TGATHER, FP32
# CHECK: B.IOT t#1, mask=1111
# CHECK: B.IOT t#2, mask=1111
# CHECK: B.IOT mask=1111, last, ->t#3<2KB>
# CHECK: BSTOP

# --- TLSU: TLOAD with GM addressing [base, stride].
TLOAD <32, 16, 32, FP32>, [a0, a1], ->t#2<2KB>
# CHECK: BSTART.TLSU TLOAD, FP32
# CHECK: B.IOT mask=1111, last, ->t#2<2KB>
# CHECK: B.IOR [a0], []
# CHECK: BSTOP

# --- TLSU: TSTORE (source-only; no destination record).
TSTORE <32, 16, 32, FP32>, t#1, [a0, a1]
# CHECK: BSTART.TLSU TSTORE, FP32
# CHECK: B.IOT t#1, mask=1111, last
# CHECK: B.IOR [a0], []
# CHECK: BSTOP

# --- TLSU: TPREFETCH with zero base (omitted-slot encoding).
TPREFETCH <32, 16, 32, FP32>, [zero]
# CHECK: BSTART.TLSU TPREFETCH, FP32
# CHECK-NOT: B.IOR
# CHECK: BSTOP

# --- TLSU: atomic RMW (function code beyond the legacy name table).
MGATHER_ADD <FP32>, [a0], t#1, t#2, ->t#3<2KB>
# CHECK: BSTART.TLSU {{.*}}, FP32
# CHECK: B.IOT t#1, mask=1111
# CHECK: B.IOT t#2, mask=1111
# CHECK: B.IOR [a0], []
# CHECK: B.IOT mask=1111, last, ->t#3<2KB>
# CHECK: BSTOP

# --- CUBE: plain TMATMUL (M/N/K dims + all-zero B.FPATR).
TMATMUL <16, 32, 64, FP32>, t#1, t#2, ->t#3<2KB>
# CHECK: BSTART.CUBE TMATMUL, FP32
# CHECK: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
# CHECK: C.B.DIMI 16, ->lb0
# CHECK: C.B.DIMI 32, ->lb1
# CHECK: C.B.DIMI 64, ->lb2
# CHECK: B.IOT t#1, mask=1111
# CHECK: B.IOT t#2, mask=1111
# CHECK: B.IOT mask=1111, last, ->t#3<2KB>
# CHECK: BSTOP

.endif
