// RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

// TGEMV permits the neutral CScaleEn value.
BSTART.CUBE TGEMV, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTOP

// ADR-0101 permits CScale for the two FP32 accumulator operations.
BSTART.CUBE TMATMUL.ACC, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
BSTOP
BSTART.CUBE TMATMULMX.ACC, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
BSTOP

// CHECK: BSTART.CUBE TGEMV, FP32
// CHECK-NEXT: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: BSTART.CUBE TMATMUL.ACC, FP32
// CHECK-NEXT: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=1
// CHECK: BSTART.CUBE TMATMULMX.ACC, FP32
// CHECK-NEXT: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=1
// DIS: BSTART.CUBE TGEMV, FP32
// DIS: BSTART.CUBE TMATMUL.ACC, FP32
// DIS: BSTART.CUBE TMATMULMX.ACC, FP32
