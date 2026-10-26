; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s

target triple = "linx64v5"

; A constant forced through an inline-asm register constraint is folded back
; into the B.DIM immediate, and dimensions equal to the block default are
; omitted entirely.
;
; CHECK-LABEL: <inline_asm_dim_constants>:
; CHECK-NOT: addi{{.*}}128
; CHECK: BSTART.TEPL{{.*}}TEXPANDS, FP32
; CHECK-NEXT: C.B.DIMI{{.*}}128,{{.*}}->lb0
; CHECK: BSTART.TEPL{{.*}}TEXPANDS, FP32
; CHECK-NEXT: C.B.DIMI{{.*}}128,{{.*}}->lb0
; CHECK-NOT: C.B.DIMI{{.*}}1,
define void @inline_asm_dim_constants() {
  call void asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DIM $0, 0, ->lb0\0AB.DIM zero, ${1:c}, ->lb1\0AB.DIM zero, ${2:c}, ->lb2\0A", "r,i,i"(i64 128, i32 1, i32 1)
  call void asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DIM $0, 0, ->lb0\0AB.DIM zero, ${1:c}, ->lb1\0AB.DIM zero, ${2:c}, ->lb2\0A", "r,i,i"(i64 128, i32 1, i32 1)
  ret void
}

; CHECK-LABEL: <inline_asm_required_lb0_one>:
; CHECK: BSTART.TLSU{{.*}}TLOAD, S32
; CHECK-NOT: ->lb0
; CHECK: BSTART.TEPL{{.*}}TEXPANDS, S32
; CHECK-NEXT: C.B.DIMI{{.*}}1,{{.*}}->lb0
; CHECK-NOT: ->lb1
; CHECK-NOT: ->lb2
; CHECK: BSTART.TEPL{{.*}}TCMP, S32
; CHECK-NEXT: C.B.DIMI{{.*}}1,{{.*}}->lb0
; CHECK-NOT: ->lb1
; CHECK-NOT: ->lb2
define void @inline_asm_required_lb0_one() {
  call void asm sideeffect "BSTART.TLSU TLOAD, S32\0AB.DIM $0, 0, ->lb0\0A", "r"(i64 1)
  call void asm sideeffect "BSTART.TEPL TEXPANDS, S32\0AB.DIM $0, 0, ->lb0\0AB.DIM $1, 0, ->lb1\0AB.DIM $2, 0, ->lb2\0A", "r,r,r"(i64 1, i64 1, i64 1)
  call void asm sideeffect "BSTART.TEPL TCMP, S32\0AB.DIM $0, 0, ->lb0\0A", "r"(i64 1)
  ret void
}
