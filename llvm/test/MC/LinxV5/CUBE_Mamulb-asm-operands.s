// RUN: llvm-mc %s --triple=linx64v5 --show-encoding -linxv5-enable-compress-inst=false| FileCheck %s --dump-input always -vv
// RUN: llvm-mc %s --triple=linx64v5 -filetype=obj -linxv5-enable-compress-inst=false -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

// imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX, FP32
// CHECK: B.DATR           FP16, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT            t#1, t#2, mask=1111
// CHECK: B.IOT            t#3, t#4, mask=1111, last, ->acc<64KB>
TMATMULMX <M:32, N:24, K:16, FP32, FP16> T#1, T#2, T#3, T#4, ->ACC<64KB>

// imm <Size>, same datatype
// CHECK: BSTART.CUBE      TMATMULMX, FP16
// CHECK-NOT: B.ATTR
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT            t#1, t#3, mask=1111
// CHECK: B.IOT            t#4, mask=1111, last, ->acc<64KB>
TMATMULMX <M:32, N:24, K:16, FP16, FP16> T#1, T#3, T#4, ->ACC<64KB>

// imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX.BIAS, FP32
// CHECK: B.DATR           FP16, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT            t#1, t#2, mask=1111
// CHECK: B.IOT            t#3, t#4, mask=1111
// CHECK: B.IOT            u#1, mask=1111, last, ->acc<64KB>
mx_bias_five_sources:
TMATMULMX.BIAS <M:32, N:24, K:16, FP32, FP16> T#1, T#2, T#3, T#4, U#1 ->ACC<64KB>
// OBJ-LABEL: <mx_bias_five_sources>:
// OBJ: B.IOT t#1, t#2, mask=1111
// OBJ-NEXT: {{.*}}B.IOT t#3, t#4, mask=1111
// OBJ-NEXT: {{.*}}B.IOT u#1, mask=1111, last, ->t<64KB>

// imm <Size>, ACC source
// CHECK: BSTART.CUBE      TMATMULMX.ACC, FP32
// CHECK-NOT: B.DATR
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT            t#1, t#2, mask=1111
// CHECK: B.IOT            t#3, t#4, mask=1111, last, ->acc<64KB>
TMATMULMX.ACC <M:32, N:24, K:16, FP32, FP32> T#1, T#2, T#3, T#4, ACC#1 ->ACC<64KB>

// imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX.BIAS, FP32
// CHECK: B.DATR           FP16, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT            t#1, t#3, mask=1111
// CHECK: B.IOT            t#4, u#1, mask=1111, last, ->acc<64KB>
mx_bias_four_sources:
TMATMULMX.BIAS <M:32, N:24, K:16, FP32, FP16> T#1, T#3, T#4, U#1 ->ACC<64KB>
// OBJ-LABEL: <mx_bias_four_sources>:
// OBJ: B.IOT t#1, t#3, mask=1111
// OBJ-NEXT: {{.*}}B.IOT t#4, u#1, mask=1111, last, ->t<64KB>

// imm <Size>, ACC source
// CHECK: BSTART.CUBE      TMATMULMX.ACC, FP32
// CHECK: B.DATR           FP16, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT            t#1, t#3, mask=1111
// CHECK: B.IOT            t#4, mask=1111, last, ->acc<64KB>
TMATMULMX.ACC <M:32, N:24, K:16, FP32, FP16> T#1, T#3, T#4, ACC#1 ->ACC<64KB>

// Reg+imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX, FP32
// CHECK: B.DATR           FP16, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
// CHECK: B.DIM            a0, 32,          ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT            t#1, t#2, mask=1111
// CHECK: B.IOT            t#3, t#4, mask=1111, last, ->acc<64KB>
TMATMULMX <M:R2+32, N:24, K:16, FP32, FP16> T#1, T#2, T#3, T#4, ->ACC<64KB>

// imm <Size>
// CHECK: BSTART.CUBE      TMATMULMX, FP32
// CHECK: B.DATR           FP16, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
// CHECK: B.DIM            zero, 32,        ->lb0
// CHECK: B.DIM            zero, 24,        ->lb1
// CHECK: B.DIM            zero, 16,        ->lb2
// CHECK: B.IOT            t#1, t#2, mask=1111
// CHECK: B.IOT            t#3, t#4, mask=1111, last, ->acc<64KB>
TMATMULMX <M:32, N:24, K:16, FP32, FP16> T#1, T#2, T#3, T#4, ->ACC<64KB>

// imm <Size>
// CHECK-NOT: BSTART.CUBE      TCVT, FP32
// CHECK-NOT: B.ATTR   ZN2ZZ, e5m2
// CHECK-NOT: B.DIM   zero, 32,       ->lb0
// CHECK-NOT: B.DIM   zero, 24,       ->lb1
// CHECK-NOT: B.IOTI  [t#1], last,  ->t<64KB>
//TCVT e5m2, zn2zz, <Row:32, Col:24, FP32> T#1, ->t<64KB>

//Reg+imm <Size>
// CHECK-NOT: BSTART.CUBE      TCVT, FP32
// CHECK-NOT: B.ATTR   ZN2ZZ, e5m2
// CHECK-NOT: B.DIM   a0, 32,         ->lb0
// CHECK-NOT: B.DIM   zero, 24,       ->lb1
// CHECK-NOT: B.IOTI  [t#1], last,  ->t<64KB>
//TCVT e5m2, zn2zz, <Row:R2+32, Col:24, FP32> T#1, ->t<64KB>

matmul_bias_sources:
TMATMUL.BIAS <M:32, N:24, K:16, FP32, FP16> T#5.reuse, T#7, U#3.reuse, ->ACC<8KB>
// CHECK: BSTART.CUBE TMATMUL.BIAS, FP32
// CHECK: B.IOT t#5.reuse, t#7, mask=1111, ->acc<8KB>
// CHECK-NEXT: B.IOT u#3.reuse, mask=1111, last
// OBJ-LABEL: <matmul_bias_sources>:
// OBJ: B.IOT t#5.reuse, t#7, mask=1111, ->t<8KB>
// OBJ-NEXT: {{.*}}B.IOT u#3.reuse, mask=1111, last

matmul_acc_sources:
TMATMUL.ACC <M:32, N:24, K:16, FP32, FP16> T#5, T#7.reuse, ACC#1, ->ACC<16KB>
// CHECK: BSTART.CUBE TMATMUL.ACC, FP32
// CHECK: B.IOT t#5, t#7.reuse, mask=1111, last, ->acc<16KB>
// OBJ-LABEL: <matmul_acc_sources>:
// OBJ: B.IOT t#5, t#7.reuse, mask=1111, last, ->t<16KB>
