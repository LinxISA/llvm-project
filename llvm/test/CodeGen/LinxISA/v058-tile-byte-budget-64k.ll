; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)

define void @tile_exactly_64k(ptr %src, ptr %dst) {
entry:
  ; 128*128*1*4B = 65536B, the largest legal logical Local tile.
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 10, i32 17, i64 0, i64 128, i64 128, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t, i32 10, i32 17, i64 0, i64 128, i64 128, i64 0)
  ret void
}

; CHECK-LABEL: tile_exactly_64k:
; CHECK: B.IOT{{.*}}->t<64KB>
; CHECK: B.IOT{{[[:space:]]+}}t#{{[0-9]+}}, mask=1111, last
