; RUN: llc -mtriple=linx64v5-unknown-linux-gnu -mcpu=janus -O2 < %s | FileCheck %s
;
; This is only the frontend/control-flow contract. It must not select the
; SIMT register file or SIMT instruction path.

; CHECK-LABEL: elementwise_mask_framework:
; CHECK-NOT: v.
; CHECK-NOT: l.
; CHECK-NOT: ri[0-9]
; CHECK-NOT: vt[0-9]
; CHECK-NOT: vu[0-9]
; CHECK-NOT: vm[0-9]
; CHECK-NOT: vn[0-9]
; CHECK: C.BSTART.STD RET

define i32 @elementwise_mask_framework(i32 %x, i32 %y) #0 {
entry:
  %cond = icmp ult i32 %x, %y
  br i1 %cond, label %then, label %else
then:
  ret i32 %x
else:
  ret i32 %y
}

attributes #0 = { "linx.elementwise" }
