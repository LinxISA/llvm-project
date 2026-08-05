; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

target triple = "linx64v5"

; Overlapping Shared SSA values need different absolute registers.
; CHECK-LABEL: <overlap>:
; CHECK: BSTART.TLSU TMOV.L2S.PUBLISH, FP32
; CHECK-NEXT: C.B.IOS S#0
; CHECK: BSTART.TLSU TMOV.L2S.PUBLISH, FP32
; CHECK-NEXT: C.B.IOS S#1
; CHECK: BSTART.TLSU TMOV.S2L.EXTRACT, FP32
; CHECK-NEXT: C.B.IOS S#0
; CHECK: BSTART.TLSU TMOV.S2L.EXTRACT, FP32
; CHECK-NEXT: C.B.IOS S#1
define void @overlap(ptr %in0, ptr %in1, ptr %out0, ptr %out1) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in0, i64 0)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in1, i64 0)
  %s0 = call i64 @llvm.linx.v5.shared.l2s.publish.v128f32(i64 1, i64 15, <128 x float> %a)
  %s1 = call i64 @llvm.linx.v5.shared.l2s.publish.v128f32(i64 1, i64 15, <128 x float> %b)
  %r0 = call <128 x float> @llvm.linx.v5.shared.s2l.extract.v128f32(i64 %s0, i64 1, i64 15)
  %r1 = call <128 x float> @llvm.linx.v5.shared.s2l.extract.v128f32(i64 %s1, i64 1, i64 15)
  call void @llvm.linx.blk.tstore.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out0, i64 0, <128 x float> %r0)
  call void @llvm.linx.blk.tstore.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out1, i64 0, <128 x float> %r1)
  ret void
}

; Non-overlapping Shared SSA values may reuse the same absolute register.
; CHECK-LABEL: <reuse>:
; CHECK: BSTART.TLSU TMOV.L2S.PUBLISH, FP32
; CHECK-NEXT: C.B.IOS S#0
; CHECK: BSTART.TLSU TMOV.S2L.EXTRACT, FP32
; CHECK-NEXT: C.B.IOS S#0
; CHECK: BSTART.TLSU TMOV.L2S.PUBLISH, FP32
; CHECK-NEXT: C.B.IOS S#0
; CHECK: BSTART.TLSU TMOV.S2L.EXTRACT, FP32
; CHECK-NEXT: C.B.IOS S#0
define void @reuse(ptr %in0, ptr %in1, ptr %out0, ptr %out1) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in0, i64 0)
  %s0 = call i64 @llvm.linx.v5.shared.l2s.publish.v128f32(i64 1, i64 15, <128 x float> %a)
  %r0 = call <128 x float> @llvm.linx.v5.shared.s2l.extract.v128f32(i64 %s0, i64 1, i64 15)
  call void @llvm.linx.blk.tstore.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out0, i64 0, <128 x float> %r0)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in1, i64 0)
  %s1 = call i64 @llvm.linx.v5.shared.l2s.publish.v128f32(i64 1, i64 15, <128 x float> %b)
  %r1 = call <128 x float> @llvm.linx.v5.shared.s2l.extract.v128f32(i64 %s1, i64 1, i64 15)
  call void @llvm.linx.blk.tstore.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out1, i64 0, <128 x float> %r1)
  ret void
}

; Shared-right TMATMUL consumes the Shared register through C.B.IOS. The B.IOT
; stream contains only local A and the ordinary local-tile result.
; CHECK-LABEL: <matmul_shared>:
; CHECK: BSTART.TLSU TMOV.L2S.PUBLISH, FP32
; CHECK-NEXT: C.B.IOS S#0
; CHECK: BSTART.CUBE TMATMUL, FP32
; CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0
; CHECK-NEXT: C.B.IOS S#0
; CHECK-NEXT: B.IOT {{[^,]+}}, mask=1111, TSize={{[0-9]+}}, last
define void @matmul_shared(ptr %in_a, ptr %in_b, ptr %out) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_a, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %in_b, i64 16)
  %shared = call i64 @llvm.linx.v5.shared.l2s.publish.v128f32(i64 1, i64 15, <128 x float> %b)
  %result = call <128 x float> @llvm.linx.blk.matmul.shared.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %a, i64 %shared)
  call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <128 x float> %result)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
declare i64 @llvm.linx.v5.shared.l2s.publish.v128f32(i64, i64, <128 x float>)
declare <128 x float> @llvm.linx.v5.shared.s2l.extract.v128f32(i64, i64, i64)
declare <128 x float> @llvm.linx.blk.matmul.shared.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, i64)
