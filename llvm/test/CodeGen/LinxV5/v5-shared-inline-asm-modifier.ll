; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=asm %s -o - | FileCheck %s --check-prefix=ASM
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -stop-after=linxv5-emit-header %s -o - | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"

; Exercise the C++ SharedTile inline-asm path directly: the Sr constraint
; creates a Shared_ABS value and the S modifier must print its absolute S0
; register name for both the producer and consumer. PTO v0.58 reissue: the
; binder mnemonic is B.IOS (source form, mask=1111), not the retired C.B.IOS.
;
; ASM-LABEL: shared_inline_asm_modifier:
; ASM: B.IOS S0, mask=1111
; ASM: B.IOS S0, mask=1111
; MIR: INLINEASM &"B.IOS ${0:S}, mask=1111", {{.*}}regdef:Shared_ABS{{.*}}, def renamable $shared_s0
; MIR: INLINEASM &"B.IOS ${0:S}, mask=1111", {{.*}}reguse:Shared_ABS{{.*}}, killed renamable $shared_s0
; OBJ-LABEL: <shared_inline_asm_modifier>:
; OBJ: B.IOS S0, mask=1111
; OBJ: B.IOS S0, mask=1111
define void @shared_inline_asm_modifier() {
  %shared = call i64 asm sideeffect "B.IOS ${0:S}, mask=1111", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}
