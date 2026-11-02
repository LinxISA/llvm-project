# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | sed -n 's/^[[:space:]]*[[:xdigit:]][[:xdigit:]]*:[[:space:]]*//p' | llvm-mc -triple=linx64v5 -filetype=obj -o %t.roundtrip
# RUN: llvm-objdump -d --no-show-raw-insn %t.roundtrip | FileCheck %s

# Regression coverage for the TileOp macro folding gaps fixed together with
# the binder compaction work:
#  * PTO #291 elementwise carrier layouts: B.DATR CUBE_M32/CUBE_M16 with an
#    ordinary elementwise op must still fold (the catalog form now carries an
#    optional Layout configuration).
#  * ADR-0098 multi-cell B.SUBVIEW (size 2..12) must fold and round-trip the
#    explicit ", size=N" annotation.
#  * A B.DATR placed after the binders (row-expand layout selection follows
#    B.IOT/B.SUBVIEW) must not block folding.

BSTART.TEPL 0, BF16
B.DATR CUBE_M32.normal, Null
C.B.DIMI 1, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 2, ->lb2
B.IOT t#1, u#1, mask=1111, last, ->u<1>
B.SUBVIEW 1, zero, 0, 1
BSTOP

# CHECK: TADD <Row=32, Col=2, ValidCol=1, BF16, CUBE_M32>, T#1, U#1[base=zero, offset=0], ->U<128B>
# CHECK-NOT: BSTART.TEPL

BSTART.TEPL 0x41, BF16
B.DATR CUBE_M32.normal, Null
C.B.DIMI 32, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 32, ->lb2
B.IOT u#3, mask=1111, last, ->t<5>
B.SUBVIEW 0, zero, 0, 5
BSTOP

# Multi-cell (16 x 128B) source view: the folded macro must spell the size.
# CHECK: TROWMAX <Row=32, Col=32, BF16, CUBE_M32>, U#3[base=zero, offset=0, size=5], ->T<2KB>

BSTART.TEPL 0x5b, BF16
C.B.DIMI 32, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 2, ->lb2
B.IOT u#3, t#1, mask=1111, last, ->t<1>
B.SUBVIEW 0, zero, 0, 5
B.DATR CUBE_M32.normal, Null
BSTOP

# The layout B.DATR follows the binders; folding must still happen.
# CHECK: TCOLEXPANDEXPDIF <Row=32, Col=2, ValidCol=32, BF16, CUBE_M32>, U#3[base=zero, offset=0, size=5], T#1, ->T<128B>

# Direct CUBE geometry must come from LB1/LB2, not from RowMajor capacity.
BSTART.TEPL 2, U64
B.DATR CUBE_M32.normal, Zero
C.B.DIMI 4, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 4, ->lb2
B.IOT u#1, t#1, mask=1111, last, ->t<4>
BSTOP

# CHECK: TMUL <Row=32, Col=4, U64, Zero, CUBE_M32>, U#1, T#1, ->T<1KB>

BSTART.TEPL 8, U8
B.DATR CUBE_M32.normal, Zero
C.B.DIMI 4, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 4, ->lb2
B.IOT u#2, u#5, mask=1111, last, ->t<1>
BSTOP

# CHECK: TXOR <Row=32, Col=4, U8, Zero, CUBE_M32>, U#2, U#5, ->T<128B>

# The code generator compacts offset + PredicateCell + destination into one
# two-source-destination B.IOT. Macro folding must consume all three schema roles.
BSTART.TLSU MGATHER.MASK, U64
B.DATR CUBE_M32.normal, Zero
C.B.DIMI 4, ->lb0
C.B.DIMI 32, ->lb1
B.IOT t#2, t#10, mask=1111, last, ->t<4>
B.IOR [a0], []
BSTOP

# CHECK: MGATHER_MASK <Col=4, ValidRow=32, U64, Zero, CUBE_M32>, [base=a0], T#2, T#10, ->T<1KB>


