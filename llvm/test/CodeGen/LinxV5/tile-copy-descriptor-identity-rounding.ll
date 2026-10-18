; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-generic-element=true %s -o - | FileCheck %s
;
; The dtype-only B.DATR spelling has default NORM layout in every rounding
; branch. Its identity-conversion result agrees with the explicit NORM input
; at the join. Forced-copy descriptor preservation is covered by the MIR test.
; CHECK-LABEL: identity_rounding_phi:
; CHECK: BSTART.TEPL TCVT, U32
; CHECK-NEXT: B.DATR U32, byte0, Null, RNONE, nosat
; CHECK-NEXT: C.B.DIMI 128, ->lb0
; CHECK-NEXT: C.B.DIMI 128, ->lb2
; CHECK-NEXT: B.IOT {{.*}}<512B>
; CHECK: BSTART.TLSU TLOAD, U32
; CHECK-NEXT: B.DATR NORM.normal, Zero
; CHECK: BSTART.TLSU TSTORE, U32
; CHECK-NEXT: B.DATR NORM.normal, Null
; CHECK: C.B.DIMI 128, ->lb0
; CHECK-NEXT: B.IOT t#1, mask=1111, last
target triple = "linx64v5-unknown-linux-musl"

define void @identity_rounding_phi(i1 %condition, ptr %base) {
entry:
  %source = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br i1 %condition, label %left, label %right
left:
  %converted = call <128 x i32> asm sideeffect "BSTART.TEPL 27, ${1:D}\0A.if ${5:c} == 0\0AB.DATR ${2:D}, RNONE\0A.else\0AB.DATR ${2:D}, RNE\0A.endif\0AB.DIM zero, 128, ->lb0\0AB.DIM zero, 128, ->lb2\0AB.IOT $3, mask=1111, last, ->$0<${4:Z}>\0A", "=@2Tr,i,i,@2Tr,i,i"(i32 25, i32 25, <128 x i32> %source, i32 3, i32 0)
  br label %join
right:
  %loaded = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br label %join
join:
  %result = phi <128 x i32> [ %converted, %left ], [ %loaded, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DATR NORM, Null\0AB.DIM zero, 128, ->lb0\0AB.IOT $0, mask=1111, last\0AB.IOR [$1], []\0A", "@2Tr,r,~{memory}"(<128 x i32> %result, ptr %base)
  ret void
}

; CHECK-LABEL: identity_literal_rounding_phi:
; CHECK: BSTART.TEPL TCVT, U32
; CHECK-NEXT: B.DATR U32, byte0, Null, RNONE, nosat
; CHECK-NEXT: C.B.DIMI 128, ->lb0
; CHECK-NEXT: C.B.DIMI 128, ->lb2
; CHECK-NEXT: B.IOT {{.*}}<512B>
; CHECK: BSTART.TLSU TLOAD, U32
; CHECK-NEXT: B.DATR NORM.normal, Zero
; CHECK: BSTART.TLSU TSTORE, U32
; CHECK-NEXT: B.DATR NORM.normal, Null
define void @identity_literal_rounding_phi(i1 %condition, ptr %base) {
entry:
  %source = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br i1 %condition, label %left, label %right
left:
  %converted = call <128 x i32> asm sideeffect "BSTART.TEPL 27, U32\0A.if ${5:c} == 0\0AB.DATR ${2:D}, RNONE\0A.else\0AB.DATR ${2:D}, RNE\0A.endif\0AB.DIM zero, 128, ->lb0\0AB.DIM zero, 128, ->lb2\0AB.IOT $3, mask=1111, last, ->$0<${4:Z}>\0A", "=@2Tr,i,i,@2Tr,i,i"(i32 25, i32 25, <128 x i32> %source, i32 3, i32 0)
  br label %join
right:
  %loaded = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br label %join
join:
  %result = phi <128 x i32> [ %converted, %left ], [ %loaded, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DATR NORM, Null\0AB.DIM zero, 128, ->lb0\0AB.IOT $0, mask=1111, last\0AB.IOR [$1], []\0A", "@2Tr,r,~{memory}"(<128 x i32> %result, ptr %base)
  ret void
}
