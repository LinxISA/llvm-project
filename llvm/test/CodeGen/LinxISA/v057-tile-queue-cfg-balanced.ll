; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tepl.unary(%linx.tile, i32, i32, i32)

define void @equal_state_diamond(ptr %src, ptr %dst, i1 %choose_left) {
entry:
  %tile = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  br i1 %choose_left, label %left, label %right

left:
  br label %join

right:
  br label %join

join:
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tile, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: equal_state_diamond:
; CHECK: BSTART.TLOAD
; CHECK: BSTART.TSTORE
; CHECK: B.IOT{{[[:space:]]+}}t#1, last, ->t<4KB>

define void @net_zero_tile_loop(ptr %src, ptr %dst, i32 %count) {
entry:
  %seed = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  br label %head

head:
  %index = phi i32 [ 0, %entry ], [ %next_index, %body ]
  %current = phi %linx.tile [ %seed, %entry ], [ %next, %body ]
  %done = icmp uge i32 %index, %count
  br i1 %done, label %exit, label %body

body:
  %next = call %linx.tile @llvm.linx.tepl.unary(%linx.tile %current, i32 2, i32 8, i32 17)
  %next_index = add i32 %index, 1
  br label %head

exit:
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %current, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; The loop consumes and republishes the same physical tile identity, so its
; backedge state equals its entry state and the worklist reaches a fixed point.
; CHECK-LABEL: net_zero_tile_loop:
; CHECK: BSTART.TLOAD
; CHECK: BSTART.TMUL
; CHECK: B.IOT{{[[:space:]]+}}t#1, last, ->t<4KB>
; CHECK: BSTART.TSTORE
; CHECK: B.IOT{{[[:space:]]+}}t#1, last, ->t<4KB>
