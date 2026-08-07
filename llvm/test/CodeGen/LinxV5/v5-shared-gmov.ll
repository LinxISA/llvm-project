; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

target triple = "linx64v5"

; CHECK-LABEL: <gmov>:
; CHECK: BSTART.TLSU GMOV, FP32
; CHECK-NEXT: B.IOT {{.*}}mask=1111, TSize=3, last
; CHECK-NEXT: B.IOR [a2],[]
define void @gmov(ptr %in, ptr %out, i64 %peer) {
  %src = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in, i64 0)
  %result = call <128 x float> @llvm.linx.v5.gmov.v128f32(i64 1, i64 15, i64 %peer, <128 x float> %src)
  call void @llvm.linx.blk.tstore.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out, i64 0, <128 x float> %result)
  ret void
}

; CHECK-LABEL: <insert_broadcast>:
; CHECK: BSTART.TLSU TMOV.L2S.INSERT, FP32
; CHECK-NEXT: C.B.IOS S#0
; CHECK: BSTART.TLSU TMOV.S2L.BROADCAST, FP32
; CHECK-NEXT: C.B.IOS S#0
define void @insert_broadcast(ptr %in, ptr %out) {
  %src = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in, i64 0)
  %shared = call i64 @llvm.linx.v5.shared.l2s.insert.v128f32(i64 1, i64 15, <128 x float> %src)
  %result = call <128 x float> @llvm.linx.v5.shared.s2l.broadcast.v128f32(i64 %shared, i64 1, i64 15)
  call void @llvm.linx.blk.tstore.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out, i64 0, <128 x float> %result)
  ret void
}

; CHECK-LABEL: <publish_extract>:
; CHECK: BSTART.TLSU TMOV.L2S.PUBLISH, FP32
; CHECK-NEXT: C.B.IOS S#0
; CHECK: BSTART.TLSU TMOV.S2L.EXTRACT, FP32
; CHECK-NEXT: C.B.IOS S#0
define void @publish_extract(ptr %in, ptr %out) {
  %src = call <256 x float> @llvm.linx.blk.tload.v256f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in, i64 0)
  %shared = call i64 @llvm.linx.v5.shared.l2s.publish.v256f32(i64 1, i64 3, <256 x float> %src)
  %result = call <256 x float> @llvm.linx.v5.shared.s2l.extract.v256f32(i64 %shared, i64 1, i64 8)
  call void @llvm.linx.blk.tstore.v256f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out, i64 0, <256 x float> %result)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare <256 x float> @llvm.linx.blk.tload.v256f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
declare void @llvm.linx.blk.tstore.v256f32(i64, i64, i64, i64, i64, ptr, i64, <256 x float>)
declare <128 x float> @llvm.linx.v5.gmov.v128f32(i64, i64, i64, <128 x float>)
declare i64 @llvm.linx.v5.shared.l2s.insert.v128f32(i64, i64, <128 x float>)
declare i64 @llvm.linx.v5.shared.l2s.publish.v256f32(i64, i64, <256 x float>)
declare <128 x float> @llvm.linx.v5.shared.s2l.broadcast.v128f32(i64, i64, i64)
declare <256 x float> @llvm.linx.v5.shared.s2l.extract.v256f32(i64, i64, i64)
