// RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS
// Reserved Function (8, 9-14) negative cases are in v5-matmul-reserved.s.

// v5 contract: B.FPATR is mandatory for every active Matrix CUBE operation
// (TMATMUL 0-2, TMATMULMX 4-6, TGEMV 16-18, TGEMVMX 20-22). Function 8
// (legacy ACCCVT) and 9-14 (deleted TMATMUL*_FIXP) are reserved/illegal and
// must not parse to a public mnemonic.

// active positive cases (Function 0,1,2,4,5,6,16,17,18,20,21,22)
BSTART.CUBE TMATMUL, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TMATMUL.BIAS, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TMATMUL.ACC, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TMATMULMX, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TMATMULMX.BIAS, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TMATMULMX.ACC, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TGEMV, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TGEMV.BIAS, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TGEMV.ACC, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TGEMVMX, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TGEMVMX.BIAS, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
BSTART.CUBE TGEMVMX.ACC, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

// reserved negative cases: the deleted TMATMUL*_FIXP mnemonics (Function
// 9-14) must be rejected by the assembler. They live in v5-matmul-reserved.s.

// CHECK: BSTART.CUBE TMATMUL, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TMATMUL.BIAS, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TMATMUL.ACC, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TMATMULMX, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TMATMULMX.BIAS, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TMATMULMX.ACC, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TGEMV, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TGEMV.BIAS, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TGEMV.ACC, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TGEMVMX, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TGEMVMX.BIAS, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// CHECK: BSTART.CUBE TGEMVMX.ACC, FP32
// CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

// DIS: BSTART.CUBE TMATMUL, FP32
// DIS-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// DIS: BSTART.CUBE TGEMV, FP32
// DIS-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
// DIS: BSTART.CUBE TGEMVMX.ACC, FP32
// DIS-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
