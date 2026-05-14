; RUN: llc < %s --march=linx64 | FileCheck %s --dump-input always -vv

; CHECK: mul128:
; CHECK-NOT: __multi3
define i128 @mul128(i128 %a, i128 %b) {
  %r = mul i128 %a, %b
  ret i128 %r
}

; CHECK: mulo:
; CHECK-NOT: __multi3
define i1 @mulo(i64 %a, i64 %b) {
  %r = call {i64, i1} @llvm.smul.with.overflow.i64(i64 %a, i64 %b)
  %r1 = extractvalue {i64, i1} %r, 1
  ret i1 %r1
}

declare { i64, i1 } @llvm.smul.with.overflow.i64(i64, i64)
