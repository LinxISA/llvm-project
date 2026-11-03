# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/tepl.s -o %t/tepl.o
# RUN: not llvm-objdump -d %t/tepl.o >%t/tepl.out 2>%t/tepl.err
# RUN: FileCheck %s --check-prefix=TEPL-OUT < %t/tepl.out
# RUN: FileCheck %s --check-prefix=TEPL-ERR < %t/tepl.err
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/tlsu.s -o %t/tlsu.o
# RUN: not llvm-objdump -d %t/tlsu.o >%t/tlsu.out 2>%t/tlsu.err
# RUN: FileCheck %s --check-prefix=TLSU-OUT < %t/tlsu.out
# RUN: FileCheck %s --check-prefix=TLSU-ERR < %t/tlsu.err
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/cube.s -o %t/cube.o
# RUN: not llvm-objdump -d %t/cube.o >%t/cube.out 2>%t/cube.err
# RUN: FileCheck %s --check-prefix=CUBE-OUT < %t/cube.out
# RUN: FileCheck %s --check-prefix=CUBE-ERR < %t/cube.err
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/gmov.s -o %t/gmov.o
# RUN: not llvm-objdump -d %t/gmov.o >%t/gmov.out 2>%t/gmov.err
# RUN: FileCheck %s --check-prefix=GMOV-OUT < %t/gmov.out
# RUN: FileCheck %s --check-prefix=GMOV-ERR < %t/gmov.err
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/timg2col.s -o %t/timg2col.o
# RUN: not llvm-objdump -d %t/timg2col.o >%t/timg2col.out 2>%t/timg2col.err
# RUN: FileCheck %s --check-prefix=TIMG2COL-OUT < %t/timg2col.out
# RUN: FileCheck %s --check-prefix=TIMG2COL-ERR < %t/timg2col.err
# RUN: llvm-objdump -d -M no-tile-macros %t/tepl.o 2>&1 | FileCheck %s --check-prefix=PHYSICAL

# All TileOp header families must report failed macro folding, even when the
# selector has no catalog entry. Each report names the header and its
# address, the physical listing still prints, and the tool exits non-zero
# after the whole image has been scanned. Ordinary instructions keep
# printing, and no-tile-macros stays report-free.
# TEPL-OUT: C.BSTART.STD
# TEPL-OUT: BSTART.TEPL TADD, FP32
# TEPL-ERR: LinxV5 error: TileOp header 'BSTART.TEPL' at 0x2 has no complete macro instruction form
# TEPL-ERR: LinxV5 error: 1 TileOp bundle(s) have no complete macro instruction form
# TLSU-OUT: BSTART.{{(TLSU )?}}TLOAD{{,?}} FP32
# TLSU-ERR: LinxV5 error: TileOp header 'BSTART.TLSU' at 0x0 has no complete macro instruction form
# TLSU-ERR: LinxV5 error: 1 TileOp bundle(s) have no complete macro instruction form
# CUBE-OUT: BSTART.{{(CUBE )?}}TMATMUL{{,?}} FP32
# CUBE-ERR: LinxV5 error: TileOp header 'BSTART.CUBE' at 0x0 has no complete macro instruction form
# CUBE-ERR: LinxV5 error: 1 TileOp bundle(s) have no complete macro instruction form
# GMOV-OUT: BSTART.GMOV FP32
# GMOV-ERR: LinxV5 error: TileOp header 'BSTART.GMOV' at 0x0 has no complete macro instruction form
# GMOV-ERR: LinxV5 error: 1 TileOp bundle(s) have no complete macro instruction form
# TIMG2COL-OUT: BSTART.TIMG2COL FP32
# TIMG2COL-ERR: LinxV5 error: TileOp header 'BSTART.TIMG2COL' at 0x0 has no complete macro instruction form
# TIMG2COL-ERR: LinxV5 error: 1 TileOp bundle(s) have no complete macro instruction form
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
