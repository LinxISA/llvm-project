; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5-unknown-linux-musl -O2 -enable-all-vector-as-tilereg=true -linxv5-enable-generic-element=true %t/different-dtype.ll -o /dev/null 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5-unknown-linux-musl -O2 -enable-all-vector-as-tilereg=true -linxv5-enable-generic-element=true %t/unknown-field.ll -o /dev/null 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5-unknown-linux-musl -O2 -enable-all-vector-as-tilereg=true -linxv5-enable-generic-element=true %t/literal-header.ll -o /dev/null 2>&1 | FileCheck %s
; CHECK: unknown Local tile descriptor at COPY/PHI control-flow join

;--- different-dtype.ll
target triple = "linx64v5-unknown-linux-musl"

define void @identity_rounding_phi(i1 %condition, ptr %base) {
entry:
  %source = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br i1 %condition, label %left, label %right
left:
  %converted = call <128 x i32> asm sideeffect "BSTART.TEPL 27, ${1:D}\0A.if ${5:c} == 0\0AB.DATR ${2:D}, RNONE\0A.else\0AB.DATR ${2:D}, RNE\0A.endif\0AB.DIM zero, 128, ->lb0\0AB.DIM zero, 128, ->lb2\0AB.IOT $3, mask=1111, last, ->$0<${4:Z}>\0A", "=@2Tr,i,i,@2Tr,i,i"(i32 25, i32 1, <128 x i32> %source, i32 3, i32 0)
  br label %join
right:
  %loaded = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br label %join
join:
  %result = phi <128 x i32> [ %converted, %left ], [ %loaded, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DATR NORM, Null\0AB.DIM zero, 128, ->lb0\0AB.IOT $0, mask=1111, last\0AB.IOR [$1], []\0A", "@2Tr,r,~{memory}"(<128 x i32> %result, ptr %base)
  ret void
}

;--- unknown-field.ll
target triple = "linx64v5-unknown-linux-musl"

define void @identity_rounding_phi(i1 %condition, ptr %base) {
entry:
  %source = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br i1 %condition, label %left, label %right
left:
  %converted = call <128 x i32> asm sideeffect "BSTART.TEPL 27, ${1:D}\0A.if ${5:c} == 0\0AB.DATR ${2:D}, NOT_A_ROUNDING\0A.else\0AB.DATR ${2:D}, NOT_A_ROUNDING\0A.endif\0AB.DIM zero, 128, ->lb0\0AB.DIM zero, 128, ->lb2\0AB.IOT $3, mask=1111, last, ->$0<${4:Z}>\0A", "=@2Tr,i,i,@2Tr,i,i"(i32 25, i32 25, <128 x i32> %source, i32 3, i32 0)
  br label %join
right:
  %loaded = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br label %join
join:
  %result = phi <128 x i32> [ %converted, %left ], [ %loaded, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DATR NORM, Null\0AB.DIM zero, 128, ->lb0\0AB.IOT $0, mask=1111, last\0AB.IOR [$1], []\0A", "@2Tr,r,~{memory}"(<128 x i32> %result, ptr %base)
  ret void
}


;--- literal-header.ll
target triple = "linx64v5-unknown-linux-musl"
define void @identity_rounding_phi(i1 %condition, ptr %base) {
entry:
  %source = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br i1 %condition, label %left, label %right
left:
  %converted = call <128 x i32> asm sideeffect "BSTART.TEPL 27, FP32\0A.if ${5:c} == 0\0AB.DATR ${2:D}, RNONE\0A.else\0AB.DATR ${2:D}, RNE\0A.endif\0AB.DIM zero, 128, ->lb0\0AB.DIM zero, 128, ->lb2\0AB.IOT $3, mask=1111, last, ->$0<${4:Z}>\0A", "=@2Tr,i,i,@2Tr,i,i"(i32 25, i32 25, <128 x i32> %source, i32 3, i32 0)
  br label %join
right:
  %loaded = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DATR NORM, Zero\0AB.DIM zero, 128, ->lb0\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,~{memory}"(ptr %base, i32 25, i32 3)
  br label %join
join:
  %result = phi <128 x i32> [ %converted, %left ], [ %loaded, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DATR NORM, Null\0AB.DIM zero, 128, ->lb0\0AB.IOT $0, mask=1111, last\0AB.IOR [$1], []\0A", "@2Tr,r,~{memory}"(<128 x i32> %result, ptr %base)
  ret void
}
