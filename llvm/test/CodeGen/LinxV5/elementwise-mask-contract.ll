; RUN: llc -mtriple=linx64v5-unknown-linux-gnu -mcpu=janus -O2 < %s | FileCheck %s
;
; The element-wise frontend contract is logical only. Carrier selection is
; recorded in IR metadata and must not select the SIMT register file.

; CHECK-LABEL: elementwise_mask_gpr:
; CHECK-NOT: ri[0-9]
; CHECK-NOT: vt[0-9]
; CHECK-NOT: vu[0-9]
; CHECK-NOT: vm[0-9]
; CHECK-NOT: vn[0-9]
; CHECK: C.BSTART.STD RET

; CHECK-LABEL: elementwise_mask_tile:
; CHECK-NOT: ri[0-9]
; CHECK-NOT: vt[0-9]
; CHECK-NOT: vu[0-9]
; CHECK-NOT: vm[0-9]
; CHECK-NOT: vn[0-9]
; CHECK: C.BSTART.STD RET

define i32 @elementwise_mask_gpr(i32 %x, i32 %y) #0 {
entry:
  %cond = icmp ult i32 %x, %y
  br i1 %cond, label %then, label %else
then:
  ret i32 %x
else:
  ret i32 %y
}

define i32 @elementwise_mask_tile(i32 %x, i32 %y) #1 {
entry:
  %cond = icmp ult i32 %x, %y
  br i1 %cond, label %then, label %else
then:
  ret i32 %x
else:
  ret i32 %y
}

attributes #0 = { "linx.elementwise" "linx.elementwise.lanes"="128" }
attributes #1 = { "linx.elementwise" "linx.elementwise.lanes"="129" }
