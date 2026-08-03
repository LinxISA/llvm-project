; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=asm %s -o - | FileCheck %s --check-prefix=ASM
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"

; Exercise the C++ SharedTile inline-asm path directly: the Sr constraint
; creates a Shared_ABS value and the S modifier must print its absolute S#n
; register name for both the producer and consumer.
;
; ASM-LABEL: shared_inline_asm_modifier:
; ASM: C.B.IOS S#0
; ASM: C.B.IOS S#0
; OBJ-LABEL: <shared_inline_asm_modifier>:
; OBJ: C.B.IOS S#0
; OBJ: C.B.IOS S#0
define void @shared_inline_asm_modifier() {
  %shared = call i64 asm sideeffect "C.B.IOS ${0:S}", "=@2Sr"()
  call void asm sideeffect "C.B.IOS ${0:S}", "@2Sr"(i64 %shared)
  ret void
}
