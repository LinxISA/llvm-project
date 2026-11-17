; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %t/external.ll 2>&1 | FileCheck %s --check-prefix=EXTERNAL
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %t/indirect.ll 2>&1 | FileCheck %s --check-prefix=INDIRECT
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %t/masked.ll 2>&1 | FileCheck %s --check-prefix=MASKED
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %t/mismatch.ll 2>&1 | FileCheck %s --check-prefix=MISMATCH
; MISMATCH: external memory call 'private_callee' needs linx_scalar_shareability("intra")
; EXTERNAL: external memory call 'opaque' needs linx_scalar_shareability("intra") or a visible definition
; INDIRECT: indirect memory call needs a scoped callee contract
; MASKED: memory intrinsic 'llvm.masked.load.v4i32.p0' needs an explicit scoped lowering

;--- external.ll
declare void @opaque(ptr)
define void @external(ptr %p) {
  call void @opaque(ptr %p)
  ret void
}
;--- indirect.ll
define void @indirect(ptr %fn, ptr %p) {
  call void %fn(ptr %p)
  ret void
}
;--- masked.ll
declare <4 x i32> @llvm.masked.load.v4i32.p0(ptr, i32 immarg, <4 x i1>, <4 x i32>)
define <4 x i32> @masked(ptr %p, <4 x i1> %mask) {
  %v = call <4 x i32> @llvm.masked.load.v4i32.p0(ptr %p, i32 4, <4 x i1> %mask, <4 x i32> poison)
  ret <4 x i32> %v
}

;--- mismatch.ll
define void @private_callee(ptr %p) "linx-scalar-shareability"="private" {
  store i32 1, ptr %p
  ret void
}
define void @caller(ptr %p) {
  call void @private_callee(ptr %p)
  ret void
}
