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
