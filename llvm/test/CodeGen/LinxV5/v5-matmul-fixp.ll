; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

target triple = "linx64v5"

; CHECK-LABEL: <fixp>:
; CHECK: BSTART.CUBE TMATMUL.FIXP, FP32
; CHECK-NEXT: B.DATR FP32, byte0, Null
; CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0
; CHECK-NEXT: B.IOT {{.*}}, {{.*}}, mask=1111
; CHECK-NEXT: B.IOT mask=1111, last, {{.*}}->{{.*}}<{{(512B|1KB|2KB|4KB|8KB|16KB|32KB)}}>
define void @fixp(ptr %in_a, ptr %in_b, ptr %out) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_a, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %in_b, i64 16)
  %result = call <128 x float> @llvm.linx.blk.matmul.fixp.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %a, <128 x float> %b)
  call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <128 x float> %result)
  ret void
}

; CHECK-LABEL: <acc_fixp>:
; CHECK: BSTART.CUBE TMATMUL, FP32
; CHECK: BSTART.CUBE TMATMUL.ACC.FIXP, FP32
; CHECK-NEXT: B.DATR FP32, byte0, Null
; CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0
; CHECK-NEXT: B.IOT {{.*}}, {{.*}}, mask=1111
; v5 unified: accumulator (Acc) is now an ordinary source in the B.IOT
; stream (no longer bypassed via implicit ACC). The last B.IOT carries it
; together with the ordinary Tile destination.
; CHECK-NEXT: B.IOT {{.*}}, mask=1111, TSize=1, last
define void @acc_fixp(ptr %in_a, ptr %in_b, ptr %out) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_a, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %in_b, i64 16)
  %acc = call <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %a, <128 x float> %b)
  %result = call <128 x float> @llvm.linx.blk.matmul.acc.fixp.v128f32.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %a, <128 x float> %b, <128 x float> %acc)
  call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <128 x float> %result)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
declare <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, <128 x float>)
declare <128 x float> @llvm.linx.blk.matmul.fixp.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, <128 x float>)
declare <128 x float> @llvm.linx.blk.matmul.acc.fixp.v128f32.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, <128 x float>, <128 x float>)
