; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-generic-element=true %s -o - | FileCheck %s

; Equal descriptor predecessors share a canonical queue contract, so PHI
; coalescing need not republish the Tile. Check the typed producers/consumer;
; forced physical COPY preservation is covered in tile-copy-descriptor-forced.mir.
; CHECK-LABEL: copy_phi:
; CHECK: BSTART.TLSU TLOAD, FP16
; CHECK-NEXT: B.DATR ND2M32.normal, Zero
; CHECK-NEXT: C.B.DIMI 4, ->lb0
; CHECK-NEXT: C.B.DIMI 32, ->lb1
; CHECK-NEXT: B.IOT {{.*}}<256B>
; CHECK: BSTART.TLSU TLOAD, FP16
; CHECK-NEXT: B.DATR ND2M32.normal, Zero
; CHECK-NEXT: C.B.DIMI 4, ->lb0
; CHECK-NEXT: C.B.DIMI 32, ->lb1
; CHECK-NEXT: B.IOT {{.*}}<256B>
; CHECK: BSTART.TLSU TSTORE, FP16
; CHECK-NEXT: B.DATR M322ND.normal, Null
; CHECK-NEXT: C.B.DIMI 4, ->lb0
; CHECK-NEXT: C.B.DIMI 32, ->lb1
; CHECK-NEXT: B.IOT t#1, mask=1111, last

target triple = "linx64v5-unknown-linux-musl"

define void @copy_phi(i1 %cond, ptr %base, i64 %stride) {
entry:
  br i1 %cond, label %left, label %right

left:
  %left.tile = call <128 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M32.normal, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,~{memory}"(ptr %base, i64 %stride, i32 4, i32 2)
  br label %join

right:
  %right.tile = call <128 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M32.normal, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,~{memory}"(ptr %base, i64 %stride, i32 4, i32 2)
  br label %join

join:
  %tile = phi <128 x half> [ %left.tile, %left ], [ %right.tile, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, ${1:D}\0AB.DATR M322ND.normal, Null\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [$2,$3], []\0A", "@2Tr,i,r,r,~{memory}"(<128 x half> %tile, i32 4, ptr %base, i64 %stride)
  ret void
}

