; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false \
; RUN:   -stop-after=linxv5-clockhands < %s -o - \
; RUN:   | FileCheck %s --check-prefixes=POS,SINGLE
; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=true \
; RUN:   -stop-after=linxv5-clockhands < %s -o - \
; RUN:   | FileCheck %s --check-prefix=POS
; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false \
; RUN:   -stop-after=linxv5-treg-to-offset < %s -o - \
; RUN:   | FileCheck %s --check-prefix=OFFSET
; RUN: llc -mtriple=linx64v5-unknown-linux-musl -O2 \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false \
; RUN:   -stop-after=linxv5-clockhands < %s -o - \
; RUN:   | FileCheck %s --check-prefix=POS4
;
; An open B.ASSEMBLE parent has one architectural queue identity.  Even when
; general tile clock-hand coloring is disabled, ordinary loop temporaries must
; use another hand so every continuation and the final store still select the
; INIT allocation as t#1.  The zero-mask CFG marker is not an allocation.

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

; INIT_LAST completes in one writer and must not reserve a whole Local hand.
define void @assemble_init_last() {
entry:
  %writer = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  %parent = call <32 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 1, i1 true, i1 true)
  ret void
}

; Four sequential OPEN generations have nonoverlapping LiveIntervals and may
; reuse one reserved hand while the remaining hands stay available to writers.
define void @four_nonoverlap_parents() {
entry:
  %writer = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  %p0 = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 2, i1 true, i1 false)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %p0, <32 x i32> %writer, i1 false, i1 true)
  %p1 = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 2, i1 true, i1 false)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %p1, <32 x i32> %writer, i1 false, i1 true)
  %p2 = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 2, i1 true, i1 false)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %p2, <32 x i32> %writer, i1 false, i1 true)
  %p3 = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 2, i1 true, i1 false)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %p3, <32 x i32> %writer, i1 false, i1 true)
  ret void
}

; Three simultaneously OPEN parents use the three reservable hands.  A fourth
; completed INIT_LAST tile is the writer fragment (the second tile input) of a
; continuation, so it is ordinary data and must remain eligible for the one
; unreserved writer hand instead of being mistaken for a fourth parent.
define void @completed_writer_is_not_parent() {
entry:
  %writer = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  %p0 = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 2, i1 true, i1 false)
  %p1 = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 2, i1 true, i1 false)
  %p2 = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 2, i1 true, i1 false)
  %completed = call <32 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %writer, i32 1, i1 true, i1 true)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, $0, mask=1111, last\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %p0, <32 x i32> %completed, i1 false, i1 true)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, $2, $0, mask=1111, last\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,@2Tr,i,i"(<64 x i32> %p1, <32 x i32> %completed, <32 x i32> %writer, i1 false, i1 true)
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, $0, mask=1111, last\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %p2, <32 x i32> %writer, i1 false, i1 true)
  ret void
}

; A branch-local ordinary writer is coalesced into one non-SSA virtual register
; with two definitions.  While the parent is OPEN, that residual Tile_ABS value
; must be constrained to the unreserved writer hand.
define void @open_parent_with_multidef_writer(i1 %cond) {
entry:
  %base = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  %parent = call <64 x i32> asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\0A",
      "=@2Tr,@2Tr,i,i,i"(<32 x i32> %base, i32 2, i1 true, i1 false)
  br i1 %cond, label %left, label %right

left:
  %left.writer = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  br label %merge

right:
  %right.writer = call <32 x i32> asm sideeffect
      "BSTART.TEPL 102, 25\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOR zero, zero, zero, ->zero\0AB.IOT mask=1111, last, ->$0<${1:Z}>\0A",
      "=@2Tr,i"(i32 1)
  br label %merge

merge:
  %writer = phi <32 x i32> [ %left.writer, %left ],
                                 [ %right.writer, %right ]
  call void asm sideeffect
      "BSTART.TEPL 27, 17\0AB.DATR CUBE_M32, RNONE\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, mask=1111, last\0AB.IOT $0, mask=1111\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\0A",
      "@2Tr,@2Tr,i,i"(<64 x i32> %parent, <32 x i32> %writer, i1 false, i1 true)
  ret void
}

; POS-LABEL: name: assemble_parent_loop
; POS: - { id: [[PARENT:[0-9]+]], class: tile_tr
; POS: %[[PARENT]]:tile_tr = IMPLICIT_DEF
; POS: INLINEASM {{.*}}B.ASSEMBLE ${4:c}, ${5:c}, $3, 0, 1{{.*}}def %[[PARENT]]{{.*}}%[[WRITER:[0-9]+]]
; POS: INLINEASM {{.*}}B.ASSEMBLE ${3:c}, ${4:c}, $2, 0, 1{{.*}}%[[PARENT]]{{.*}}%[[SCRATCH:[0-9]+]]
; POS: INLINEASM {{.*}}TSTORE{{.*}}%[[PARENT]]
;
; SINGLE-LABEL: name: assemble_init_last
; SINGLE-NOT: class: tile_tr
; SINGLE: B.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1
;
; OFFSET-LABEL: name: assemble_parent_loop
; OFFSET: B.ASSEMBLE ${3:c}, ${4:c}{{.*}}$tile_tos1
; OFFSET: TSTORE{{.*}}$tile_tos1
;
; The INIT_LAST result is an ordinary completed writer.  It must publish on
; the unreserved N hand, acquire a normal queue identity, and then be consumed
; through a relative N offset while the three T/M/U parents stay OPEN.
; OFFSET-LABEL: name: completed_writer_is_not_parent
; OFFSET: B.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1{{.*}}def $tile_n{{.*}}$tile_nos1{{.*}}1{{.*}}1
; OFFSET: B.IOT $1, $0{{.*}}killed $tile_tos1{{.*}}$tile_nos1
;
; POS4-LABEL: name: four_nonoverlap_parents
; POS4-COUNT-4: class: tile_tr
;
; SINGLE-LABEL: name: completed_writer_is_not_parent
; SINGLE-NOT: class: tile_abs,
; SINGLE: - { id: [[P0:[0-9]+]], class: tile_tr
; SINGLE: - { id: [[P1:[0-9]+]], class: tile_mr
; SINGLE: - { id: [[P2:[0-9]+]], class: tile_ur
; SINGLE: BSTART.TEPL 102{{.*}}def %[[BASE:[0-9]+]]
; SINGLE: B.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1{{.*}}def %[[P0]]{{.*}}%[[BASE]]
; SINGLE: B.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1{{.*}}def %[[P1]]{{.*}}%[[BASE]]
; SINGLE: B.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1{{.*}}def %[[P2]]{{.*}}%[[BASE]]
; SINGLE: B.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1{{.*}}def %[[COMPLETED:[0-9]+]]{{.*}}%[[BASE]]
; SINGLE: B.IOT $1, $0{{.*}}%[[P0]]{{.*}}%[[COMPLETED]]
; SINGLE: B.IOT $1, $2, $0{{.*}}%[[P1]]{{.*}}%[[COMPLETED]]{{.*}}%[[BASE]]
; SINGLE: B.IOT $1, $0{{.*}}%[[P2]]{{.*}}%[[BASE]]
;
; SINGLE-LABEL: name: open_parent_with_multidef_writer
; SINGLE-NOT: class: tile_abs,
; SINGLE: class: tile_umn
; SINGLE-NOT: class: tile_nr
; SINGLE: - { id: [[OPEN:[0-9]+]], class: tile_tr
; SINGLE: def %[[OPEN]]
; SINGLE: def %[[WRITER:[0-9]+]]
; SINGLE: def %[[WRITER]]
; SINGLE: %[[OPEN]]{{.*}}%[[WRITER]]
