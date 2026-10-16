; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false \
; RUN:   -stop-after=linxv5-clockhands %s -o - \
; RUN:   | FileCheck %s
;
; The architectural parent remains OPEN across 100 million logical inner-loop
; iterations.  Phase analysis must converge over joined ranges rather than
; enumerate concrete values up to the 65,536-state proof budget.

target triple = "linx64v5-unknown-linux-musl"

define void @assemble_parent_nested_large_counters() {
entry:
  %writer = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  %parent = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 2, i1 true, i1 false)
  br label %outer

outer:
  %outer.index = phi i32 [ 0, %entry ], [ %outer.next, %outer.latch ]
  br label %inner

inner:
  %inner.index = phi i32 [ 0, %outer ], [ %inner.next, %inner ]
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 0, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %parent, <32 x i32> %writer, i1 false, i1 false)
  %inner.next = add nuw nsw i32 %inner.index, 1
  %inner.done = icmp eq i32 %inner.next, 100000
  br i1 %inner.done, label %outer.latch, label %inner

outer.latch:
  %outer.next = add nuw nsw i32 %outer.index, 1
  %outer.done = icmp eq i32 %outer.next, 1000
  br i1 %outer.done, label %last, label %outer

last:
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %parent, <32 x i32> %writer, i1 false, i1 true)
  ret void
}

; CHECK-LABEL: name: assemble_parent_nested_large_counters
; CHECK: class: tile_tr
; CHECK: B.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1
; CHECK: B.ASSEMBLE ${2:c}, ${3:c}, zero, 0, 1
; CHECK: B.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1
