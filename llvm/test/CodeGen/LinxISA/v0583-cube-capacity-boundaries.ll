; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/m33.ll 2>&1 | FileCheck %s --check-prefix=M33
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/a-over.ll 2>&1 | FileCheck %s --check-prefix=AOVER
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/b-over.ll 2>&1 | FileCheck %s --check-prefix=BOVER
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/zero.ll 2>&1 | FileCheck %s --check-prefix=ZERO
; RUN: not --crash llc -mtriple=linx64 -O2 < %t/max-plus-one.ll 2>&1 | FileCheck %s --check-prefix=MAX

; M33: cube.tmatmul requires A CUBE_M32 rows <= 32 (got 33)
; AOVER: cube.tmatmul requires A CUBE capacity 65664B
; BOVER: cube.tmatmul requires B CUBE capacity 81920B
; ZERO: cube.tmatmul requires m to be in arbitrary positive range 1..65535
; MAX: cube.tmatmul requires n to be in arbitrary positive range 1..65535

;--- m33.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)
define void @m33(%linx.tile %a, %linx.tile %b) {
  %out = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %a, %linx.tile %b, i32 33, i32 1, i32 1)
  ret void
}

;--- a-over.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)
define void @a_over(%linx.tile %a, %linx.tile %b) {
  %out = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %a, %linx.tile %b, i32 32, i32 1, i32 513)
  ret void
}

;--- b-over.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)
define void @b_over(%linx.tile %a, %linx.tile %b) {
  %out = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %a, %linx.tile %b, i32 32, i32 33, i32 512)
  ret void
}

;--- zero.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)
define void @zero(%linx.tile %a, %linx.tile %b) {
  %out = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %a, %linx.tile %b, i32 0, i32 1, i32 1)
  ret void
}

;--- max-plus-one.ll
%linx.tile = type target("linx.tile")
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)
define void @max_plus_one(%linx.tile %a, %linx.tile %b) {
  %out = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %a, %linx.tile %b, i32 1, i32 65536, i32 1)
  ret void
}
