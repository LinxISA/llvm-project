; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-element-region-verify -disable-output %t/argument.ll 2>&1 | FileCheck %s
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-element-region-verify -disable-output %t/predicate.ll 2>&1 | FileCheck %s
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-element-region-verify -disable-output %t/callsite.ll 2>&1 | FileCheck %s --check-prefix=CALLSITE
; CHECK: Local Tile ABI cannot preserve another live Tile across a call
; CALLSITE: Local Tile ABI call result requires inreg

;--- callsite.ll
declare inreg <128 x i32> @callsite_helper(<128 x i32>) #0
define void @missing_callsite_marker(<128 x i32> %arg) {
  %result = call <128 x i32> @callsite_helper(<128 x i32> %arg)
  ret void
}
attributes #0 = { "linx-local-tile-abi"="0:128:128" }

;--- argument.ll
; Values live across the boundary are not restricted to the element profiles.
declare <128 x i32> @local_helper(<128 x i32>) #0
define void @live_argument(<16384 x float> %wide, <128 x i32> %arg) {
  %r = call inreg <128 x i32> @local_helper(<128 x i32> %arg)
  call void asm sideeffect "", "@2Tr"(<16384 x float> %wide)
  ret void
}
attributes #0 = { "linx-local-tile-abi"="0:128:128" }

;--- predicate.ll
; The physical U32-word carrier contains packed Predicate storage.
declare <128 x i32> @local_helper(<128 x i32>) #0
declare <512 x i32> @llvm.linx.experimental.ew.tcmp.row.v512i32.v2048i32.v2048i32(i64, i64, i64, i64, <2048 x i32>, <2048 x i32>, i64, i64)
define void @live_predicate(<2048 x i32> %a, <128 x i32> %arg) {
  %p = call <512 x i32> @llvm.linx.experimental.ew.tcmp.row.v512i32.v2048i32.v2048i32(i64 1, i64 2048, i64 25, i64 0, <2048 x i32> %a, <2048 x i32> %a, i64 0, i64 2048)
  %r = call inreg <128 x i32> @local_helper(<128 x i32> %arg)
  call void asm sideeffect "", "@2Tr"(<512 x i32> %p)
  ret void
}
attributes #0 = { "linx-local-tile-abi"="0:128:128" }
