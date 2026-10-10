; RUN: llc < %s -mtriple=linx64v5 -mcpu=janus \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-clock-hand-opt=true \
; RUN:   -stop-after=linxv5-clockhands -o - | FileCheck %s
; RUN: llc < %s -mtriple=linx64v5 -mcpu=janus \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-clock-hand-opt=true -o - | FileCheck %s --check-prefix=ASM
; RUN: llc < %s -mtriple=linx64v5 -mcpu=janus \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-clock-hand-opt=false -o - | FileCheck %s --check-prefix=ASM

target triple = "linx64v5"

; PHI elimination coalesces the branch-local Tile definitions into one virtual
; register with multiple definitions. Clockhands must leave that non-SSA live
; range in Tile_ABS instead of assigning its register class from one definition.
;
; CHECK-LABEL: name: tile_phi
; CHECK: bb.1.left:
; CHECK: %[[TILE:[0-9]+]]:tile_abs = PseudoVCall_PAR_RegListIn_1d0u_SizeI_StackI_noDsrc_noDst @copyin
; CHECK: bb.3.right:
; CHECK: %[[TILE]]:tile_abs = PseudoVCall_PAR_RegListIn_1d0u_SizeI_StackI_noDsrc_noDst @copyin
; CHECK: bb.5.merge:
; CHECK: PseudoVCall_PAR_RegListIn_0d1u_StackI_noDsrc_noDst @copyout{{.*}}%[[TILE]]
;
; ASM-LABEL: tile_phi:
; ASM: VPAR copyin
; ASM: VPAR copyin
; ASM: TCOPY
; ASM: VPAR copyout

define void @tile_phi(ptr %lhs, ptr %rhs, ptr %out, i1 %cond) {
entry:
  br i1 %cond, label %left, label %right

left:
  %left.tile = call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(ptr @copyin, i64 4, i64 4, i64 1, ptr %lhs)
  br label %merge

right:
  %right.tile = call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(ptr @copyin, i64 4, i64 4, i64 1, ptr %rhs)
  br label %merge

merge:
  %tile = phi <64 x double> [ %left.tile, %left ], [ %right.tile, %right ]
  call void (ptr, i64, i64, i64, <64 x double>, ...) @llvm.linx.vcall.par.0d1u.v64f64(ptr @copyout, i64 4, i64 4, i64 1, <64 x double> %tile, ptr %out)
  ret void
}

declare void @copyin(<64 x double>, ptr) #0
declare void @copyout(<64 x double>, ptr) #0
declare <64 x double> @llvm.linx.vcall.par.1d0u.v64f64(ptr, i64, i64, i64, ...)
declare void @llvm.linx.vcall.par.0d1u.v64f64(ptr, i64, i64, i64, <64 x double>, ...)

attributes #0 = { "__vec__" }
