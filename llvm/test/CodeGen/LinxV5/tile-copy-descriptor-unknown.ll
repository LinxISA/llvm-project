; RUN: not --crash llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true %s -o /dev/null 2>&1 | FileCheck %s
; CHECK: unknown Local tile descriptor at COPY/PHI control-flow join

; Runtime descriptor dimensions cannot be reconstructed from SizeCode.  Until
; the dynamic descriptor has an SSA carrier of its own, a generic Tile PHI
; must reject instead of manufacturing static TMOV dimensions.

target triple = "linx64v5-unknown-linux-musl"

define void @unknown_copy_phi(i1 %cond, ptr %base, i64 %stride, i64 %rows) {
entry:
  br i1 %cond, label %left, label %right

left:
  %left.tile = call <128 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M32.normal, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM $4, 0, ->lb1\0AB.IOT mask=1111, last, ->$0<${5:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,r,i,~{memory}"(ptr %base, i64 %stride, i32 4, i64 %rows, i32 2)
  br label %join

right:
  %right.tile = call <128 x half> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0AB.DATR ND2M32.normal, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM $4, 0, ->lb1\0AB.IOT mask=1111, last, ->$0<${5:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,r,i,~{memory}"(ptr %base, i64 %stride, i32 4, i64 %rows, i32 2)
  br label %join

join:
  %tile = phi <128 x half> [ %left.tile, %left ], [ %right.tile, %right ]
  call void asm sideeffect "BSTART.TLSU TSTORE, ${1:D}\0AB.DIM zero, 4, ->lb0\0AB.DIM $2, 0, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [$3,$4], []\0A", "@2Tr,i,r,r,r,~{memory}"(<128 x half> %tile, i32 4, i64 %rows, ptr %base, i64 %stride)
  ret void
}

