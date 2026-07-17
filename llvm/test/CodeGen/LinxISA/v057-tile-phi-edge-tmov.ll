; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s
; UNSUPPORTED: asserts

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)

define void @tile_phi_edge(ptr %a, ptr %b, ptr %out, i1 %cond) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  br i1 %cond, label %then, label %else

then:
  br label %merge

else:
  br label %merge

merge:
  %phi = phi %linx.tile [ %ta, %then ], [ %tb, %else ]
  call void @llvm.linx.tile.tstore(ptr %out, %linx.tile %phi, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: tile_phi_edge:
; CHECK: BSTART.TLOAD
; CHECK: BSTART.TLOAD
; CHECK: BSTART.TMOV
; CHECK: BSTART.TSTORE
