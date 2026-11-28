; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %t/external.ll 2>&1 | FileCheck %s --check-prefix=EXTERNAL
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %t/indirect.ll 2>&1 | FileCheck %s --check-prefix=INDIRECT
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %t/masked.ll 2>&1 | FileCheck %s --check-prefix=MASKED
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %t/mismatch.ll 2>&1 | FileCheck %s --check-prefix=MISMATCH
; MISMATCH: memory-accessing callee needs a matching shareability contract
; EXTERNAL: memory-accessing callee needs a matching shareability contract
; INDIRECT: memory-accessing callee needs a matching shareability contract
; MASKED: memory intrinsic 'llvm.masked.load.v4i32.p0' needs an explicit scoped lowering

;--- external.ll
declare void @opaque(ptr)
define void @external(ptr %p) "linx-scalar-shareability"="intra" {
  call void @opaque(ptr %p)
  ret void
}
;--- indirect.ll
define void @indirect(ptr %fn, ptr %p) "linx-scalar-shareability"="intra" {
  call void %fn(ptr %p)
  ret void
}
;--- masked.ll
declare <4 x i32> @llvm.masked.load.v4i32.p0(ptr, i32 immarg, <4 x i1>, <4 x i32>)
define <4 x i32> @masked(ptr %p, <4 x i1> %mask) "linx-scalar-shareability"="intra" {
  %v = call <4 x i32> @llvm.masked.load.v4i32.p0(ptr %p, i32 4, <4 x i1> %mask, <4 x i32> poison)
  ret <4 x i32> %v
}

;--- mismatch.ll
define void @private_callee(ptr %p) "linx-scalar-shareability"="private" {
  store i32 1, ptr %p
  ret void
}
define void @caller(ptr %p) "linx-scalar-shareability"="intra" {
  call void @private_callee(ptr %p)
  ret void
}
