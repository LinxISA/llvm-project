; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=linx64 -O2 -filetype=obj -o - < %s | \
; RUN:   llvm-objdump -d - | FileCheck %s --check-prefix=OBJ

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)

define void @tile_hand_encoding(ptr %src, ptr %dst) {
entry:
  %t0 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %t1 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %t2 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %t3 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %t4 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %t5 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %t6 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %t7 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %u0 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %u0, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t7, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t6, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t5, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t4, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t3, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t2, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t1, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t0, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; The ninth simultaneously live tile occupies physical U0, but B.IOT must
; encode the architectural U-hand base (16) as u#1, never physical id 8/t#9.
; CHECK-LABEL: tile_hand_encoding:
; CHECK:      B.IOT{{[[:space:]]+}}u#1, mask=1111, last
; CHECK-NOT:  t#9
; OBJ-LABEL:  <tile_hand_encoding>:
; OBJ:        B.IOT{{[[:space:]]+}}u#1, mask=1111, last
; OBJ-NOT:    t#9

define void @mn_hand_encoding(ptr %src, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %m0 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %m1 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %m2 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %m3 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %m4 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %m5 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %m6 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %m7 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %n0 = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %n0, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %m7, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %m6, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %m5, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %m4, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %m3, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %m2, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %m1, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %m0, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: mn_hand_encoding:
; CHECK:      B.IOT{{[[:space:]]+}}n#1, mask=1111, last
; CHECK-NOT:  m#9
; OBJ-LABEL:  <mn_hand_encoding>:
; OBJ:        B.IOT{{[[:space:]]+}}n#1, mask=1111, last
; OBJ-NOT:    m#9
