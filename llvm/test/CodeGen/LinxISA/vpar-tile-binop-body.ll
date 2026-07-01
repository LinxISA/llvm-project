; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.vpar.tadd(%linx.tile, %linx.tile, i32 immarg)
declare %linx.tile @llvm.linx.vpar.tsub(%linx.tile, %linx.tile, i32 immarg)

define void @add_tile(ptr %a, ptr %b, ptr %out) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %r = call %linx.tile @llvm.linx.vpar.tadd(%linx.tile %ta, %linx.tile %tb, i32 8)
  call void @llvm.linx.tile.tstore(ptr %out, %linx.tile %r, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

define void @sub_tile(ptr %a, ptr %b, ptr %out) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %r = call %linx.tile @llvm.linx.vpar.tsub(%linx.tile %ta, %linx.tile %tb, i32 8)
  call void @llvm.linx.tile.tstore(ptr %out, %linx.tile %r, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: add_tile:
; CHECK:      BSTART.VPAR
; CHECK:      B.TEXT {{\.__linx_vtile_add_body\.[0-9]+}}
; CHECK:      {{^\.__linx_vtile_add_body\.[0-9]+:}}
; CHECK:      v.add {{.*}}->vt#2
; CHECK-NEXT: v.sw.brg.local vt#2, [to, lc0<<2, lc1<<8]
; CHECK-NEXT: C.BSTOP

; CHECK-LABEL: sub_tile:
; CHECK:      BSTART.VPAR
; CHECK:      B.TEXT {{\.__linx_vtile_sub_body\.[0-9]+}}
; CHECK:      {{^\.__linx_vtile_sub_body\.[0-9]+:}}
; CHECK:      v.sub {{.*}}->vt#2
; CHECK-NEXT: v.sw.brg.local vt#2, [to, lc0<<2, lc1<<8]
; CHECK-NEXT: C.BSTOP
