; RUN: split-file %s %t
; RUN: not llc -mtriple=linx64v5 -mcpu=janus %t/unaligned.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALIGN
; RUN: not llc -mtriple=linx64v5 -mcpu=janus %t/wide.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=WIDE
; Unknown-scope runtime calls must not silently replace shared atomics.
; ALIGN: Scalar shareability: unaligned atomic requires a scoped runtime implementation
; WIDE: Scalar shareability: unsupported atomic width

;--- unaligned.ll
target triple = "linx64v5"
define i64 @unaligned_load(ptr %p) "linx-scalar-shareability"="intra" {
  %v = load atomic i64, ptr %p acquire, align 1
  ret i64 %v
}
;--- wide.ll
target triple = "linx64v5"
define {i128, i1} @wide_cas(ptr %p, i128 %a, i128 %b) "linx-scalar-shareability"="intra" {
  %r = cmpxchg ptr %p, i128 %a, i128 %b seq_cst seq_cst
  ret {i128, i1} %r
}
