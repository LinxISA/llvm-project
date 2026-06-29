; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

@table = external global [0 x ptr], align 8

declare ptr @sink(ptr)

define ptr @call_indirect_vararg_shape(i64 %code, ptr %x, ptr %y) {
entry:
  %slot = getelementptr inbounds [0 x ptr], ptr @table, i64 0, i64 %code
  %fn = load ptr, ptr %slot, align 8
  %raw = call ptr (ptr, ...) %fn(ptr %x, ptr %y)
  %ret = call ptr @sink(ptr %raw)
  ret ptr %ret
}

; CHECK-LABEL: call_indirect_vararg_shape:
; CHECK: C.BSTART.STD{{[[:space:]]+}}ICALL
; CHECK: {{(hl\.)?}}ld{{.*}},{{[[:space:]]+}}->{{(t|x[0-3])}}
; CHECK: c.setc.tgt{{[[:space:]]+}}{{(t#1|x[0-3])}}
; CHECK-NOT: c.setc.tgt{{[[:space:]]+}}a0
