; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-generic-element=true %s -o - | FileCheck %s

target triple = "linx64v5-unknown-linux-musl"

; Explicit LB2=1 is descriptor data. No identity copy is needed at an
; equal-descriptor PHI; forced copies are checked in the companion MIR test.
; CHECK-LABEL: explicit_lb2_one:
; CHECK: BSTART.TLSU TLOAD, FP16
; CHECK-NEXT: B.DATR ND2M32.normal, Zero
; CHECK-NEXT: C.B.DIMI 1, ->lb0
; CHECK-NEXT: C.B.DIMI 32, ->lb1
; CHECK-NEXT: C.B.DIMI 1, ->lb2
; CHECK: BSTART.TLSU TLOAD, FP16
; CHECK: BSTART.TLSU TSTORE, FP16
; CHECK-NEXT: B.DATR M322ND.normal, Null
; CHECK-NEXT: C.B.DIMI 1, ->lb0
; CHECK-NEXT: C.B.DIMI 32, ->lb1
; CHECK-NEXT: C.B.DIMI 1, ->lb2
define void @explicit_lb2_one(i1 %cond, ptr %base, i64 %stride) {
entry:
  br i1 %cond, label %left, label %right
left:
  %a = call <64 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M32.normal, Zero\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 1, ->lb2\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,~{memory}"(ptr %base, i64 %stride, i32 4, i32 1)
  br label %join
right:
  %b = call <64 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M32.normal, Zero\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 1, ->lb2\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,~{memory}"(ptr %base, i64 %stride, i32 4, i32 1)
  br label %join
join:
  %tile = phi <64 x half> [ %a, %left ], [ %b, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, ${1:D}\0AB.DATR M322ND.normal, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 1, ->lb2\0AB.IOT $0, mask=1111, last\0A", "@2Tr,i,~{memory}"(<64 x half> %tile, i32 4)
  ret void
}

; Omitted LB2 uses the architectural ValidCol default. Capacity determines
; physical rows and must not silently invent additional columns.
; CHECK-LABEL: omitted_m16_lb2:
; CHECK: BSTART.TLSU TLOAD, FP16
; CHECK-NEXT: B.DATR ND2M16.normal, Zero
; CHECK-NEXT: C.B.DIMI 4, ->lb0
; CHECK-NEXT: C.B.DIMI 8, ->lb1
; CHECK: BSTART.TLSU TLOAD, FP16
; CHECK: BSTART.TLSU TSTORE, FP16
; CHECK-NEXT: B.DATR M162ND.normal, Null
; CHECK-NEXT: C.B.DIMI 4, ->lb0
; CHECK-NEXT: C.B.DIMI 8, ->lb1
define void @omitted_m16_lb2(i1 %cond, ptr %base, i64 %stride) {
entry:
  br i1 %cond, label %left, label %right
left:
  %a = call <64 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M16.normal, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 8, ->lb1\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,~{memory}"(ptr %base, i64 %stride, i32 4, i32 1)
  br label %join
right:
  %b = call <64 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M16.normal, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 8, ->lb1\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,~{memory}"(ptr %base, i64 %stride, i32 4, i32 1)
  br label %join
join:
  %tile = phi <64 x half> [ %a, %left ], [ %b, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, ${1:D}\0AB.DATR M162ND.normal, Null\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 8, ->lb1\0AB.IOT $0, mask=1111, last\0A", "@2Tr,i,~{memory}"(<64 x half> %tile, i32 4)
  ret void
}

