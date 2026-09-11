# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

# Canonical source expands to the physical bundle and objdump folds the exact
# bundle back to one line. Default ValidRow/ValidCol, PadValue and AllPE are
# omitted from canonical output.
TADD <Row=8, Col=64, FP32>, T#1, T#2, ->T<2KB>

# CHECK: TADD{{ +}}<Row=8, Col=64, FP32>, T#1, T#2, ->T<2KB>

# Physical partial-tile form keeps only the non-default fields and prints the
# PE mask as a named value.
BSTART.TEPL TADD, FP32
C.B.DIMI 60, ->lb0
C.B.DIMI 7, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1100, last, ->T<2KB>

# CHECK: TADD{{ +}}<Row=8, Col=64, ValidRow=7, ValidCol=60, FP32, PE0_1>, T#1, T#2, ->T<2KB>

# Fixed-default scalar fields are accepted explicitly but omitted from
# canonical output because they do not require a B.IOR command.
TCI <Row=32, Col=1, FP32>, Start=0, Direction=ascending, ->T<128B>
TTRI <Row=32, Col=1, FP32>, Diagonal=0, Orientation=lower, ->T<128B>

# CHECK: TCI{{ +}}<Row=32, Col=1, FP32>, ->T<128B>
# CHECK: TTRI{{ +}}<Row=32, Col=1, FP32>, ->T<128B>

# A generic numeric Tile binding preserves its encoded clock hand. U/M/N are
# not predicate carriers unless the selected form assigns that binding kind.
TADD <Row=32, Col=1, FP32>, U#2, M#1, ->N<128B>

# CHECK: TADD{{ +}}<Row=32, Col=1, FP32>, U#2, M#1, ->N<128B>

# An incomplete/non-canonical bundle remains physical assembly.
BSTART.TEPL TADD, FP32
C.B.DIMI 64, ->lb0
C.B.DIMI 8, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, ->T<2KB>
addi zero, 0, ->zero

# CHECK: BSTART.TEPL	TADD, FP32
# CHECK-NEXT: C.B.DIMI	64, 	->lb0
# CHECK-NEXT: C.B.DIMI	8, 	->lb1
# CHECK-NEXT: C.B.DIMI	64, 	->lb2
# CHECK-NEXT: B.IOT	t#1, t#2, mask=1111, 	->t<2KB>
