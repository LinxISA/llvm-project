# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s --check-prefix=MACRO
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=PHYSICAL
# RUN: llvm-objdump -d --no-show-raw-insn -M no-tile-macros %t.o | FileCheck %s --check-prefix=PHYSICAL

# The default view folds a complete physical TileOp bundle into one macro.
TADD <Row=8, Col=64, FP32>, T#1, T#2, ->T<2KB>

# The option selects the original physical micro-instruction view.
# MACRO: TADD{{ +}}<Row=8, Col=64, FP32>, T#1, T#2, ->T<2KB>
# PHYSICAL: BSTART.TEPL{{.*}}TADD, FP32
# PHYSICAL-NEXT: C.B.DIMI 64,{{.*}}->lb0
# PHYSICAL-NEXT: C.B.DIMI 8,{{.*}}->lb1
# PHYSICAL-NEXT: C.B.DIMI 64,{{.*}}->lb2
# PHYSICAL-NEXT: B.IOT t#1, t#2, mask=1111,{{.*}}->t<2KB>
