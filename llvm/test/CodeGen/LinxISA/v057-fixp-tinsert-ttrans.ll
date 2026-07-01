; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tile.tinsert(%linx.tile, %linx.tile, i32, i32, i32, i32, i32, i32, i64)
declare %linx.tile @llvm.linx.tile.ttrans(%linx.tile, %linx.tile, i32, i32, i32, i32, i32, i32)

define void @fixp_tinsert(ptr %base, ptr %src, ptr %dst, i64 %meta) {
entry:
  %t0 = call %linx.tile @llvm.linx.tile.tload(ptr %base, i32 8, i32 1, i64 0, i64 16, i64 32, i64 0)
  %t1 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 1, i64 0, i64 16, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.tile.tinsert(%linx.tile %t0, %linx.tile %t1, i32 8, i32 1, i32 16, i32 32, i32 16, i32 8, i64 %meta)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %out, i32 8, i32 1, i64 0, i64 16, i64 32, i64 0)
  ret void
}

; CHECK-LABEL: fixp_tinsert:
; CHECK: BSTART.TINSERT
; CHECK: B.META
; CHECK: B.ITP
; CHECK: B.OTA
; CHECK: BSTART.TSTORE

define void @fixp_tinsert_const_meta(ptr %base, ptr %src, ptr %dst) {
entry:
  %t0 = call %linx.tile @llvm.linx.tile.tload(ptr %base, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %t1 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 1, i64 0, i64 4, i64 4, i64 0)
  %out = call %linx.tile @llvm.linx.tile.tinsert(%linx.tile %t0, %linx.tile %t1, i32 8, i32 1, i32 8, i32 8, i32 4, i32 4, i64 8589934596)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %out, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: fixp_tinsert_const_meta:
; CHECK: BSTART.TINSERT
; CHECK: B.META
; CHECK: B.ITP
; CHECK: B.OTA
; CHECK: BSTART.TSTORE

define void @fixp_ttrans(ptr %src, ptr %tmp, ptr %dst) {
entry:
  %t0 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 1, i64 0, i64 16, i64 32, i64 0)
  %t1 = call %linx.tile @llvm.linx.tile.tload(ptr %tmp, i32 8, i32 1, i64 0, i64 16, i64 32, i64 0)
  %out = call %linx.tile @llvm.linx.tile.ttrans(%linx.tile %t0, %linx.tile %t1, i32 8, i32 1, i32 32, i32 16, i32 16, i32 32)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %out, i32 8, i32 1, i64 0, i64 32, i64 16, i64 0)
  ret void
}

; CHECK-LABEL: fixp_ttrans:
; CHECK: BSTART.TTRANS
; CHECK: B.ITP
; CHECK: B.OTA
; CHECK: BSTART.TSTORE
