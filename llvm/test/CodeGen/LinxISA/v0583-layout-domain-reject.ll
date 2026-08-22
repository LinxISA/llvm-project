; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/tload-reserved.ll 2>&1 | FileCheck %s --check-prefix=TLOAD
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/tmov-hidden.ll 2>&1 | FileCheck %s --check-prefix=HIDDEN
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/tmov-reserved.ll 2>&1 | FileCheck %s --check-prefix=TMOV

; TLOAD: tile.tload requires an assigned B.DATR Layout
; HIDDEN: tile.tmov requires layout=0 when has_layout=0
; TMOV: tile.tmov requires an assigned B.DATR Layout

;--- tload-reserved.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
define void @tload_reserved(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 19, i64 2, i64 8, i64 8, i64 0)
  ret void
}

;--- tmov-hidden.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tile.tmov(%linx.tile, i32, i32, i64, i1)
define void @tmov_hidden(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 19, i64 0, i64 8, i64 8, i64 0)
  %m = call %linx.tile @llvm.linx.tile.tmov(%linx.tile %t, i32 1, i32 31, i64 17, i1 false)
  ret void
}

;--- tmov-reserved.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tile.tmov(%linx.tile, i32, i32, i64, i1)
define void @tmov_reserved(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 19, i64 0, i64 8, i64 8, i64 0)
  %m = call %linx.tile @llvm.linx.tile.tmov(%linx.tile %t, i32 1, i32 31, i64 2, i1 true)
  ret void
}
