# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/tepl.s -o %t/tepl.o
# RUN: not --crash llvm-objdump -d %t/tepl.o 2>&1 | FileCheck %s --check-prefix=TEPL
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/tlsu.s -o %t/tlsu.o
# RUN: not --crash llvm-objdump -d %t/tlsu.o 2>&1 | FileCheck %s --check-prefix=TLSU
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/cube.s -o %t/cube.o
# RUN: not --crash llvm-objdump -d %t/cube.o 2>&1 | FileCheck %s --check-prefix=CUBE
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/gmov.s -o %t/gmov.o
# RUN: not --crash llvm-objdump -d %t/gmov.o 2>&1 | FileCheck %s --check-prefix=GMOV
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/timg2col.s -o %t/timg2col.o
# RUN: not --crash llvm-objdump -d %t/timg2col.o 2>&1 | FileCheck %s --check-prefix=TIMG2COL
# RUN: llvm-objdump -d -M no-tile-macros %t/tepl.o | FileCheck %s --check-prefix=PHYSICAL

# All TileOp header families must report failed macro folding, even when the
# selector has no catalog entry. Ordinary instructions continue to print.
# TEPL: LLVM ERROR: LinxV5: cannot print BSTART_TEPL_NoMode at 0x2 in macro instruction form
# TLSU: LLVM ERROR: LinxV5: cannot print BSTART_TMA at 0x0 in macro instruction form
# CUBE: LLVM ERROR: LinxV5: cannot print BSTART_CUBE at 0x0 in macro instruction form
# GMOV: LLVM ERROR: LinxV5: cannot print BSTART_GMOV at 0x0 in macro instruction form
# TIMG2COL: LLVM ERROR: LinxV5: cannot print BSTART_TIMG2COL at 0x0 in macro instruction form
# PHYSICAL: C.BSTART.STD
# PHYSICAL: BSTART.TEPL TADD, FP32

#--- tepl.s
C.BSTART.STD
BSTART.TEPL TADD, FP32

#--- tlsu.s
BSTART.TLSU TLOAD, FP32

#--- cube.s
BSTART.CUBE TMATMUL, FP32

#--- gmov.s
BSTART.GMOV FP32

#--- timg2col.s
BSTART.TIMG2COL FP32
