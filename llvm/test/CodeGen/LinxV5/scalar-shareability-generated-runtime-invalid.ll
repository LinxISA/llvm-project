; RUN: split-file %s %t
; RUN: not llc -mtriple=linx64v5 %t/callee.ll 2>&1 | FileCheck %s --check-prefix=CALLEE
; RUN: not llc -mtriple=linx64v5 %t/caller.ll 2>&1 | FileCheck %s --check-prefix=CALLER
; CALLEE: external memory call 'private_callee' needs
; CALLER: memory-accessing callee needs a matching shareability contract

;--- callee.ll
declare void @private_callee(ptr) "linx-scalar-shareability"="private"
define void @runtime(ptr %p) "linx-runtime-intra-abi" {
  call void @private_callee(ptr %p)
  ret void
}

;--- caller.ll
declare void @external(ptr)
define void @explicit_scope(ptr %p) "linx-runtime-intra-abi" "linx-scalar-shareability"="inter" {
  call void @external(ptr %p)
  ret void
}
