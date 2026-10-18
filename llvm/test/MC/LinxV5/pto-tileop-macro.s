# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/valid.s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/invalid.s -o %t/invalid.o
# RUN: not llvm-objdump -d --no-show-raw-insn %t/invalid.o >/dev/null 2>%t.err
# RUN: FileCheck %s --check-prefix=ERROR < %t.err
# RUN: llvm-objdump -d --no-show-raw-insn -M no-tile-macros %t/invalid.o | FileCheck %s --check-prefix=PHYSICAL

#--- valid.s

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
B.IOT T#1.reuse, T#2.reuse, mask=1100, last, ->T<2KB>

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

#--- invalid.s
# An incomplete bundle must report an error in the default macro view.
# ERROR: LinxV5 error: TileOp header 'BSTART.TEPL' at 0x0 has no complete macro instruction form
BSTART.TEPL TADD, FP32
C.B.DIMI 64, ->lb0
C.B.DIMI 8, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1.reuse, T#2.reuse, mask=1111, ->T<2KB>
addi zero, 0, ->zero

# PHYSICAL: BSTART.TEPL	TADD, FP32
# PHYSICAL-NEXT: C.B.DIMI	64, 	->lb0
# PHYSICAL-NEXT: C.B.DIMI	8, 	->lb1
# PHYSICAL-NEXT: C.B.DIMI	64, 	->lb2
# PHYSICAL-NEXT: B.IOT	t#1.reuse, t#2.reuse, mask=1111, 	->t<2KB>
