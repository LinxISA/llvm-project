; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/fp64.ll 2>&1 | FileCheck %s --check-prefix=FP64
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/tf32.ll 2>&1 | FileCheck %s --check-prefix=TF32
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/fp16.ll 2>&1 | FileCheck %s --check-prefix=FP16
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/hif8.ll 2>&1 | FileCheck %s --check-prefix=HIF8
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/e2m1x2.ll 2>&1 | FileCheck %s --check-prefix=E2M1X2
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/reserved.ll 2>&1 | FileCheck %s --check-prefix=RESERVED
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/none.ll 2>&1 | FileCheck %s --check-prefix=NONE

; FP64: tile.tload tile-byte check failed: bytes=2048B
; TF32: tile.tload tile-byte check failed: bytes=1024B
; FP16: tile.tload tile-byte check failed: bytes=512B
; HIF8: tile.tload tile-byte check failed: bytes=256B
; E2M1X2: tile.tload tile-byte check failed: bytes=256B
; RESERVED: tile.tload requires an assigned concrete tile dtype
; NONE: tile.tload requires an assigned concrete tile dtype

;--- fp64.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
define void @fp64(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 0, i64 0, i64 16, i64 16, i64 0)
  ret void
}

;--- tf32.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
define void @tf32(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 2, i64 0, i64 16, i64 16, i64 0)
  ret void
}

;--- fp16.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
define void @fp16(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 4, i64 0, i64 16, i64 16, i64 0)
  ret void
}

;--- hif8.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
define void @hif8(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 6, i64 0, i64 16, i64 16, i64 0)
  ret void
}

;--- e2m1x2.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
define void @e2m1x2(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 11, i64 0, i64 32, i64 16, i64 0)
  ret void
}

;--- reserved.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
define void @reserved(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 15, i64 0, i64 1, i64 1, i64 0)
  ret void
}

;--- none.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
define void @none(ptr %src) {
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 1, i32 31, i64 0, i64 1, i64 1, i64 0)
  ret void
}
