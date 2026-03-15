; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

declare ptr @llvm.stacksave()
declare void @llvm.stackrestore(ptr)

define i64 @stacksave_roundtrip(i64 %n) {
; CHECK-LABEL: stacksave_roundtrip:
; CHECK: FENTRY
; CHECK: c.movr	sp,	->s7
; CHECK: c.movr	s7,	->sp
; CHECK: FRET.STK
entry:
  %sp = call ptr @llvm.stacksave()
  %buf = alloca i8, i64 %n, align 16
  call void @llvm.stackrestore(ptr %sp)
  %ret = ptrtoint ptr %buf to i64
  ret i64 %ret
}
