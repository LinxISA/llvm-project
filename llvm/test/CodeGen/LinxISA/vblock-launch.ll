; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

declare void @llvm.linx.vblock.launch(i32, ptr, i64, i64, i64, i32,
                                      i64, i64, i64, i64, i64, i64,
                                      i64, i64, i64, i64, i64, i64)

define void @vseq() {
entry:
  call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 2, i64 3, i64 4, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

define void @vseq_rdc() #0 {
entry:
  call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 8, i64 1, i64 1, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

define void @vseq_tile() {
entry:
  call void @llvm.linx.vblock.launch(i32 2, ptr null, i64 2, i64 1, i64 1, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

define void @vpar_tile() {
entry:
  call void @llvm.linx.vblock.launch(i32 3, ptr null, i64 2, i64 1, i64 1, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

define void @vseq_local_scratch() #1 {
entry:
  call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 8, i64 1, i64 1, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

define void @vseq_typed_body() #2 {
entry:
  call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 8, i64 2, i64 1, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

define void @vseq_local_scratch_dword() #3 {
entry:
  call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 8, i64 1, i64 1, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

define void @vseq_reuse_body() #4 {
entry:
  call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 8, i64 1, i64 1, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

; CHECK-LABEL: vseq:
; CHECK:      BSTART.MSEQ
; CHECK-NEXT: B.TEXT {{\.__linx_vblock_body\.[0-9]+}}
; CHECK:      C.B.DIMI{{[[:space:]]+}}2,{{.*->lb0}}
; CHECK:      C.B.DIMI{{[[:space:]]+}}3,{{.*->lb1}}
; CHECK:      C.B.DIMI{{[[:space:]]+}}4,{{.*->lb2}}
; CHECK:      {{^\.__linx_vblock_body\.[0-9]+:}}
; CHECK:      C.BSTOP
; CHECK:      {{^\.__linx_vblock_body\.[0-9]+\.end:}}

; CHECK-LABEL: vseq_rdc:
; CHECK:      BSTART.MSEQ
; CHECK:      B.TEXT {{\.__linx_vblock_body\.[0-9]+}}
; CHECK:      {{^\.__linx_vblock_body\.[0-9]+:}}
; CHECK:      v.rdadd vt#1, ->a0
; CHECK:      C.BSTOP
; CHECK:      {{^\.__linx_vblock_body\.[0-9]+\.end:}}

; CHECK-LABEL: vseq_tile:
; CHECK:      BSTART.VSEQ
; CHECK:      B.TEXT {{\.__linx_vblock_body\.[0-9]+}}

; CHECK-LABEL: vpar_tile:
; CHECK:      BSTART.VPAR
; CHECK:      B.TEXT {{\.__linx_vblock_body\.[0-9]+}}

; CHECK-LABEL: vseq_local_scratch:
; CHECK:      BSTART.MSEQ
; CHECK-NEXT: B.TEXT {{\.__linx_vblock_body\.[0-9]+}}
; CHECK:      B.IOT{{[[:space:]]+}}mask=1111,{{[[:space:]]+}}->t<128B>
; CHECK:      B.IOT{{[[:space:]]+}}mask=1111, last,{{[[:space:]]+}}->u<128B>
; CHECK:      {{^\.__linx_vblock_body\.[0-9]+:}}
; CHECK:      v.swi.u.local zero, [ts, lc0<<2, 0]
; CHECK:      v.lwi.u.local [ts, lc0<<2, 4], ->vt#1
; CHECK:      C.BSTOP

; CHECK-LABEL: vseq_typed_body:
; CHECK:      BSTART.MSEQ
; CHECK:      B.TEXT {{\.__linx_vblock_body\.[0-9]+}}
; CHECK:      {{^\.__linx_vblock_body\.[0-9]+:}}
; CHECK:      v.add lc0, lc1.uw<<5, ->vt#1
; CHECK:      v.sub vt#1, lc0.uw, ->vt#2
; CHECK:      v.lw.brg [ri1, lc0<<2, vt#2<<2], ->vt#3
; CHECK:      C.BSTOP

; CHECK-LABEL: vseq_local_scratch_dword:
; CHECK:      BSTART.MSEQ
; CHECK:      B.TEXT {{\.__linx_vblock_body\.[0-9]+}}
; CHECK:      {{^\.__linx_vblock_body\.[0-9]+:}}
; CHECK:      v.sdi.u.local zero, [ts, lc0<<3, 8]
; CHECK:      v.ldi.u.local [ts, lc0<<3, 16], ->vn#1
; CHECK:      C.BSTOP

; CHECK-LABEL: vseq_reuse_body:
; CHECK:      BSTART.MSEQ
; CHECK:      B.TEXT {{\.__linx_vblock_body\.[0-9]+}}
; CHECK:      {{^\.__linx_vblock_body\.[0-9]+:}}
; CHECK:      v.lwi.u.local [ts, lc0<<2, 8], ->vt#1
; CHECK:      v.add vt#1, lc0.uw, ->vu#1
; CHECK:      v.swi.u.local vu#1, [ts, lc0<<2, 12]
; CHECK:      C.BSTOP

attributes #0 = { "linx-vblock-body-asm"="  v.rdadd vt#1.sw, ->a0\0A  C.BSTOP\0A" }
attributes #1 = { "linx-vblock-body-asm"="  v.swi.u.local zero, [ts, lc0.uh<<2, 0]\0A  v.lwi.u.local [ts, lc0.uh<<2, 4], ->vt.w\0A  C.BSTOP\0A" "linx-vblock-ts-bytes"="64" }
attributes #2 = { "linx-vblock-body-asm"="  v.add lc0.uh, lc1.uh<<5, ->vt.w\0A  v.sub vt#1.sw, lc0.uh, ->vt.w\0A  v.lw.brg [ri1, lc0.uh<<2, vt#2.sw<<2], ->vt#3.w\0A  C.BSTOP\0A" }
attributes #3 = { "linx-vblock-body-asm"="  v.sdi.u.local zero, [ts, lc0.uh<<3, 8]\0A  v.ldi.u.local [ts, lc0.uh<<3, 16], ->vn.d\0A  C.BSTOP\0A" "linx-vblock-ts-bytes"="64" }
attributes #4 = { "linx-vblock-body-asm"="  v.lwi.u.local [ts, lc0.uh<<2, 8], ->vt.w\0A  v.add vt#1.reuse.sw, lc0.uh, ->vu.w\0A  v.swi.u.local vu#1.reuse.uw, [ts, lc0.uh<<2, 12]\0A  C.BSTOP\0A" "linx-vblock-ts-bytes"="64" }
