// RUN: split-file %s %t
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/named.s -o %t/named.o
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/numeric.s -o %t/numeric.o
// RUN: cmp %t/named.o %t/numeric.o
// RUN: llvm-mc -triple=linx64v5 -show-encoding %t/named.s | FileCheck %s
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t/named.o | FileCheck %s

// CHECK: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: B.FPATR PreQuantMode=F322BF16, ReluMode=VectorPReLU, GroupN=32, RowMaxEn=1, GroupMaxEn=1, RowMaxInit=1, MaxAbsEn=1, TransA=1, TransB=1, CScaleEn=1
// CHECK: B.FPATR PreQuantMode=VQS322BF16_PRE, ReluMode=ScalarLReLU, GroupN=128, RowMaxEn=0, GroupMaxEn=1, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=1, CScaleEn=0
// CHECK: B.FPATR PreQuantMode=F322F16, ReluMode=ReLU, GroupN=16, RowMaxEn=1, GroupMaxEn=1, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0

//--- named.s
B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
B.FPATR CScaleEn=1, TransB=1, TransA=1, MaxAbsEn=1, RowMaxInit=1, GroupMaxEn=1, RowMaxEn=1, GroupN=16+16, ReluMode=VectorPReLU, PreQuantMode=F322BF16
b.fpatr prequantmode=vqs322bf16_pre, relumode=scalarlrelu, groupncode=9, rowmaxen=0, groupmaxen=1, rowmaxinit=0, maxabsen=0, transa=0, transb=1, cscaleen=0
B.FPATR PreQuantMode=1, ReluMode=1, GroupNCode=2, RowMaxEn=1, GroupMaxEn=1, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0

//--- numeric.s
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
B.FPATR 16, 3, 3, 1, 1, 1, 1, 1, 1, 1
B.FPATR 39, 2, 9, 0, 1, 0, 0, 0, 1, 0
B.FPATR 1, 1, 2, 1, 1, 0, 0, 0, 0, 0
