; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

target triple = "linx64v5"

; CHECK-LABEL: <gmov>:
; CHECK: BSTART.TLSU GMOV, FP32
; CHECK-NEXT: B.IOT {{.*}}mask=1111, TSize=1, last
; CHECK-NEXT: B.IOR [a2],[]
define void @gmov(ptr %in, ptr %out, i64 %peer) {
  %src = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in, i64 0)
  %result = call <128 x float> @llvm.linx.v5.gmov.v128f32(i64 1, i64 15, i64 %peer, <128 x float> %src)
  call void @llvm.linx.blk.tstore.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out, i64 0, <128 x float> %result)
  ret void
}

; CHECK-LABEL: <insert>:
; CHECK: BSTART.TLSU TMOV.L2S.INSERT, FP32
; CHECK-NEXT: C.B.IOS S#7
; CHECK-NEXT: B.IOT {{.*}}mask=1111, TSize=1, last

define void @insert(ptr %in) {
  %src = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in, i64 0)
  call void @llvm.linx.v5.shared.l2s.insert.v128f32(i64 7, i64 1, i64 15, <128 x float> %src)
  ret void
}

; CHECK-LABEL: <publish>:
; CHECK: BSTART.TLSU TMOV.L2S.PUBLISH, FP32
; CHECK-NEXT: C.B.IOS S#9
; CHECK-NEXT: B.IOT {{.*}}mask=0011, TSize=2, last

define void @publish(ptr %in) {
  %src = call <256 x float> @llvm.linx.blk.tload.v256f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in, i64 0)
  call void @llvm.linx.v5.shared.l2s.publish.v256f32(i64 9, i64 1, i64 3, <256 x float> %src)
  ret void
}

; CHECK-LABEL: <broadcast>:
; CHECK: BSTART.TLSU TMOV.S2L.BROADCAST, FP32
; CHECK-NEXT: C.B.IOS S#11
; CHECK-NEXT: B.IOT mask=1111, TSize=1, last

define void @broadcast(ptr %out) {
  %result = call <128 x float> @llvm.linx.v5.shared.s2l.broadcast.v128f32(i64 11, i64 1, i64 15)
  call void @llvm.linx.blk.tstore.v128f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out, i64 0, <128 x float> %result)
  ret void
}

; CHECK-LABEL: <extract>:
; CHECK: BSTART.TLSU TMOV.S2L.EXTRACT, FP32
; CHECK-NEXT: C.B.IOS S#13
; CHECK-NEXT: B.IOT mask=1000, TSize=2, last

define void @extract(ptr %out) {
  %result = call <256 x float> @llvm.linx.v5.shared.s2l.extract.v256f32(i64 13, i64 1, i64 8)
  call void @llvm.linx.blk.tstore.v256f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out, i64 0, <256 x float> %result)
  ret void
}

; CHECK-LABEL: <max_size>:
; CHECK: BSTART.TLSU TMOV.S2L.EXTRACT, FP32
; CHECK-NEXT: C.B.IOS S#255
; CHECK-NEXT: B.IOT mask=1111, TSize=7, last

define void @max_size(ptr %out) {
  %result = call <8192 x float> @llvm.linx.v5.shared.s2l.extract.v8192f32(i64 255, i64 1, i64 15)
  call void @llvm.linx.blk.tstore.v8192f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out, i64 0, <8192 x float> %result)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare <256 x float> @llvm.linx.blk.tload.v256f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
declare void @llvm.linx.blk.tstore.v256f32(i64, i64, i64, i64, i64, ptr, i64, <256 x float>)
declare void @llvm.linx.blk.tstore.v8192f32(i64, i64, i64, i64, i64, ptr, i64, <8192 x float>)
declare <128 x float> @llvm.linx.v5.gmov.v128f32(i64, i64, i64, <128 x float>)
declare void @llvm.linx.v5.shared.l2s.insert.v128f32(i64, i64, i64, <128 x float>)
declare void @llvm.linx.v5.shared.l2s.publish.v256f32(i64, i64, i64, <256 x float>)
declare <128 x float> @llvm.linx.v5.shared.s2l.broadcast.v128f32(i64, i64, i64)
declare <256 x float> @llvm.linx.v5.shared.s2l.extract.v256f32(i64, i64, i64)
declare <8192 x float> @llvm.linx.v5.shared.s2l.extract.v8192f32(i64, i64, i64)
