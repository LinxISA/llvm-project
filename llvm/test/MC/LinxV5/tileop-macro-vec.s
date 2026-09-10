# RUN: llvm-mc %s --triple=linx64v5 --show-encoding \
# RUN:   | FileCheck %s --dump-input=always
# RUN: not llvm-mc %s --triple=linx64v5 --defsym=NEG=1 2>&1 \
# RUN:   | FileCheck %s --check-prefix=NEG

# PTO 0.58.6 single-line TileOp macro assembly (LinxISA/llvm-project#90,
# pto-spec #261/#263): VEC elementwise family. One source line per TileOp;
# the macro expands to the physical bundle in canonical order.

.ifdef NEG
# Continuation is illegal: the newline terminates the instruction.
# NEG: TileOp macro: operand shape does not match
TADD <32, FP32>, t#1, t#2,
     ->t#3<8KB>
# Unknown attribute value is rejected closed.
# NEG: TileOp macro: unknown DataType
TADD <32, WAT>, t#1, t#2, ->t#3<8KB>
.else

# Full form: three dims + dtype + pad + PEMask.
TADD <32, 16, 32, FP32, Zero, 0b1111>, t#1, t#2, ->t#3<8KB>
# CHECK: BSTART.TEPL TADD, FP32
# CHECK: B.DATR NORM.normal, Zero
# CHECK: C.B.DIMI 32, ->lb0
# CHECK: C.B.DIMI 16, ->lb1
# CHECK: C.B.DIMI 32, ->lb2
# CHECK: B.IOT t#1, t#2, mask=1111, last, ->t#3<8KB>
# CHECK: BSTOP

# Minimal form: LB0 + dtype only; optional dims/pad default.
TADD <32, FP32>, t#1, t#2, ->t#3<8KB>
# CHECK: BSTART.TEPL TADD, FP32
# CHECK-NOT: B.DATR
# CHECK: C.B.DIMI 32, ->lb0
# CHECK-NOT: ->lb1
# CHECK-NOT: ->lb2
# CHECK: B.IOT t#1, t#2, mask=1111, last, ->t#3<8KB>
# CHECK: BSTOP

# Scalar variant: the scalar rides in B.IOR.
TADDS <32, FP32>, t#1, a0, ->t#2<2KB>
# CHECK: BSTART.TEPL TADDS, FP32
# CHECK: B.IOT t#1, mask=1111, last, ->t#2<2KB>
# CHECK: B.IOR [a0], []
# CHECK: BSTOP

# PadValue-only optional attribute.
TMUL <64, 32, FP32, Max>, t#1, t#2, ->t#3<4KB>
# CHECK: BSTART.TEPL TMUL, FP32
# CHECK: B.DATR NORM.normal, Max
# CHECK: C.B.DIMI 64, ->lb0
# CHECK: C.B.DIMI 32, ->lb1
# CHECK: B.IOT t#1, t#2, mask=1111, last, ->t#3<4KB>
# CHECK: BSTOP

# Unary form: single source.
TNEG <32, 16, FP32>, t#1, ->t#2<1KB>
# CHECK: BSTART.TEPL TNEG, FP32
# CHECK: B.IOT t#1, mask=1111, last, ->t#2<1KB>
# CHECK: BSTOP

# Physical assembly remains accepted (macro surface is additive; the two
# source tile operands ride their own binding records in the physical form).
BSTART.TEPL TADD, FP32
C.B.DIMI 32, ->lb0
B.IOT t#1, mask=1111
B.IOT t#2, mask=1111
B.IOT mask=1111, last, ->t<8KB>
BSTOP
# CHECK: BSTART.TEPL TADD, FP32

.endif
