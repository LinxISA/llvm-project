; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)

define void @tma_desc_roundtrip(ptr %src, ptr %dst) {
entry:
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: tma_desc_roundtrip:
; CHECK: BSTART.TLOAD{{[[:space:]]+}}
; CHECK: C.B.DIMI{{.*}}->lb0
; CHECK: C.B.DIMI{{.*}}->lb1
; CHECK: B.IOR
; CHECK: B.IOT
; CHECK: BSTART.TSTORE{{[[:space:]]+}}
; CHECK: B.IOR
; CHECK: B.IOT
; CHECK-NOT: B.ARG
