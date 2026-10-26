// RUN: llvm-mc -triple=linx64v5 -show-encoding %s > %t.s
// RUN: FileCheck %s < %t.s
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %t.s -o %t.roundtrip.o
// RUN: cmp %t.o %t.roundtrip.o
// RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s --implicit-check-not="//"
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s
// RUN: llvm-mc -triple=linx64v5 -linxv5-no-aliases -show-encoding %s | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn %t.o | sed -n 's/.*B.FPATR/B.FPATR/p' > %t.dis.s
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %t.dis.s -o %t.dis.o
// RUN: cmp %t.o %t.dis.o

.text

B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0

B.FPATR 16, 3, 3, 1, 1, 1, 1, 1, 1, 1
// CHECK: B.FPATR PreQuantMode=F322BF16, ReluMode=VectorPReLU, GroupN=32, RowMaxEn=1, GroupMaxEn=1, RowMaxInit=1, MaxAbsEn=1, TransA=1, TransB=1, CScaleEn=1

B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=NoQuant, ReluMode=None, GroupN=0

B.FPATR 1, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=F322F16, ReluMode=None, GroupN=0

B.FPATR 2, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VREQ8, ReluMode=None, GroupN=0

B.FPATR 3, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=REQ8, ReluMode=None, GroupN=0

B.FPATR 4, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VDEQF16, ReluMode=None, GroupN=0

B.FPATR 5, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=DEQF16, ReluMode=None, GroupN=0

B.FPATR 12, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VSHIFTS322S16, ReluMode=None, GroupN=0

B.FPATR 13, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=SHIFTS322S16, ReluMode=None, GroupN=0

B.FPATR 16, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=F322BF16, ReluMode=None, GroupN=0

B.FPATR 17, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=REQ4, ReluMode=None, GroupN=0

B.FPATR 18, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VREQ4, ReluMode=None, GroupN=0

B.FPATR 19, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=DEQS16, ReluMode=None, GroupN=0

B.FPATR 20, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VDEQS16, ReluMode=None, GroupN=0

B.FPATR 23, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VQF322B8_PRE, ReluMode=None, GroupN=0

B.FPATR 24, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=QF322B8_PRE, ReluMode=None, GroupN=0

B.FPATR 25, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=QF322HIF8_PRE, ReluMode=None, GroupN=0

B.FPATR 26, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=QF322FP8_PRE, ReluMode=None, GroupN=0

B.FPATR 27, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=QF322F32_PRE, ReluMode=None, GroupN=0

B.FPATR 28, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VQF322HIF8_PRE, ReluMode=None, GroupN=0

B.FPATR 32, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=QF322F16_PRE, ReluMode=None, GroupN=0

B.FPATR 33, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VQF322F16_PRE, ReluMode=None, GroupN=0

B.FPATR 34, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=QF322BF16_PRE, ReluMode=None, GroupN=0

B.FPATR 35, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=QS322BF16_PRE, ReluMode=None, GroupN=0

B.FPATR 36, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VQF322BF16_PRE, ReluMode=None, GroupN=0

B.FPATR 37, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VQF322FP8_PRE, ReluMode=None, GroupN=0

B.FPATR 38, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VQF322F32_PRE, ReluMode=None, GroupN=0

B.FPATR 39, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: PreQuantMode=VQS322BF16_PRE, ReluMode=None, GroupN=0

B.FPATR 0, 1, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: ReluMode=ReLU

B.FPATR 0, 2, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: ReluMode=ScalarLReLU

B.FPATR 0, 3, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: ReluMode=VectorPReLU

B.FPATR 0, 0, 1, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=8, RowMaxEn=0, GroupMaxEn=1

B.FPATR 0, 0, 2, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=16, RowMaxEn=0, GroupMaxEn=1

B.FPATR 0, 0, 3, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=32, RowMaxEn=0, GroupMaxEn=1

B.FPATR 0, 0, 4, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=48, RowMaxEn=0, GroupMaxEn=1

B.FPATR 0, 0, 5, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=64, RowMaxEn=0, GroupMaxEn=1

B.FPATR 0, 0, 6, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=80, RowMaxEn=0, GroupMaxEn=1

B.FPATR 0, 0, 7, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=96, RowMaxEn=0, GroupMaxEn=1

B.FPATR 0, 0, 8, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=112, RowMaxEn=0, GroupMaxEn=1

B.FPATR 0, 0, 9, 0, 1, 0, 0, 0, 0, 0
// CHECK: GroupN=128, RowMaxEn=0, GroupMaxEn=1
