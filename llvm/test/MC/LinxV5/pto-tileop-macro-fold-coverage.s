# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | llvm-mc -triple=linx64v5 -filetype=obj - | llvm-mc -triple=linx64v5 -filetype=obj - | llvm-objdump -d %t > %t.hex1 && llvm-objdump -d /tmp/does-not-exist-ignore 2>/dev/null; true

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
B.IOT t#1.reuse, u#1.reuse, mask=1111, last, ->u<1>
B.SUBVIEW 1, zero, 0, 1
BSTOP

# CHECK: TADD <Row=32, Col=2, ValidCol=1, BF16, CUBE_M32>, T#1, U#1[base=zero, offset=0], ->U<128B>
# CHECK-NOT: BSTART.TEPL

BSTART.TEPL 0x41, BF16
B.DATR CUBE_M32.normal, Null
C.B.DIMI 32, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 32, ->lb2
B.IOT u#3.reuse, mask=1111, last, ->t<5>
B.SUBVIEW 0, zero, 0, 5
BSTOP

# Multi-cell (16 x 128B) source view: the folded macro must spell the size.
# CHECK: TROWMAX <Row=32, Col=32, BF16, CUBE_M32>, U#3[base=zero, offset=0, size=5], ->T<2KB>

BSTART.TEPL 0x5b, BF16
C.B.DIMI 32, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 2, ->lb2
B.IOT u#3.reuse, t#1.reuse, mask=1111, last, ->t<1>
B.SUBVIEW 0, zero, 0, 5
B.DATR CUBE_M32.normal, Null
BSTOP

# The layout B.DATR follows the binders; folding must still happen.
# CHECK: TCOLEXPANDEXPDIF <Row=32, Col=2, ValidCol=32, BF16, CUBE_M32>, U#3[base=zero, offset=0, size=5], T#1, ->T<128B>


