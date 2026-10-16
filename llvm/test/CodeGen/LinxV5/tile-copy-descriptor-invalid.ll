; RUN: not --crash llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s
; CHECK: invalid Local tile descriptor for typed COPY

target triple = "linx64v5-unknown-linux-musl"

; A known descriptor must be rejected before TCOPY/TMOV emission when its
; explicit physical column count cannot contain the valid columns.
define void @invalid_copy_phi(i1 %cond, ptr %base, i64 %stride) {
entry:
  br i1 %cond, label %left, label %right
left:
  %a = call <128 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M32.normal, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 1, ->lb2\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,~{memory}"(ptr %base, i64 %stride, i32 4, i32 2)
  br label %join
right:
  %b = call <128 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M32.normal, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 1, ->lb2\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,~{memory}"(ptr %base, i64 %stride, i32 4, i32 2)
  br label %join
join:
  %tile = phi <128 x half> [ %a, %left ], [ %b, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, ${1:D}\0AB.IOT $0, mask=1111, last\0A", "@2Tr,i,~{memory}"(<128 x half> %tile, i32 4)
  ret void
}

