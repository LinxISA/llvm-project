; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false \
; RUN:   -stop-after=linxv5-treg-to-offset %s -o - | FileCheck %s
;
; Finite-set joins must preserve the exact equality between the phase counter
; and its sign-preserving OR_SW copies. Filtering a copied counter at LAST
; must also narrow its source, or an infeasible OPEN path reaches the store.
; CHECK-LABEL: name: assemble_parent_loop
; CHECK: BSTART.TLSU TSTORE
target triple = "linx64v5-unknown-linux-musl"

define void @assemble_parent_loop(ptr %out) {
entry:
  %writer = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  br label %loop

loop:
  %part = phi i32 [ 0, %entry ], [ %next, %latch ]
  %parent = phi <512 x i32> [ poison, %entry ], [ %next.parent, %latch ]
  %is.init = icmp eq i32 %part, 0
  br i1 %is.init, label %init, label %not.init

not.init:
  %is.last = icmp eq i32 %part, 15
  br i1 %is.last, label %last, label %middle

init:
  %created = call <512 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${4:c}, ${5:c}, $3, 0, 1\0A",
      "=@2Tr,@2Tr,i,r,i,i"(<32 x i32> %writer, i32 5, i32 %part, i1 true, i1 false)
  br label %latch

middle:
  %middle.scratch = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${3:c}, ${4:c}, $2, 0, 1\0A",
      "@2Tr,@2Tr,r,i,i"(<512 x i32> %parent, <32 x i32> %middle.scratch, i32 %part, i1 false, i1 false)
  br label %latch

last:
  %last.scratch = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${3:c}, ${4:c}, $2, 0, 1\0A",
      "@2Tr,@2Tr,r,i,i"(<512 x i32> %parent, <32 x i32> %last.scratch, i32 %part, i1 false, i1 true)
  br label %latch

latch:
  %next.parent = phi <512 x i32> [ %created, %init ],
                                      [ %parent, %middle ],
                                      [ %parent, %last ]
  %next = add nuw nsw i32 %part, 1
  %done = icmp eq i32 %next, 16
  br i1 %done, label %exit, label %loop

exit:
  call void asm sideeffect
      "BSTART.TLSU TSTORE, 17\0AB.DATR M322ND.normal, Null\0AB.DIM zero, 16, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOR [$0,zero], []\0A",
      "r,@2Tr"(ptr %out, <512 x i32> %next.parent)
  ret void
}
