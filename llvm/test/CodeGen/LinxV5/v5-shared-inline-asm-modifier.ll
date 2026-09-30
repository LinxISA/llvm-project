; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=asm %s -o - | FileCheck %s --check-prefix=ASM
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -stop-after=linxv5-emit-header %s -o - | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"

; Exercise the C++ SharedTile inline-asm path directly: the Sr constraint
; creates a Shared_ABS value and the S modifier must print its absolute S0
; register name for both the producer and consumers. Ordinary input `%S` is
; conservatively retained, `%K` names one exact final-use source occurrence,
; and a destination `%S` carries no lifetime suffix. PTO v0.58 reissue: the
; binder mnemonic is B.IOS (source form, mask=1111), not the retired C.B.IOS.
;
; ASM-LABEL: shared_inline_asm_modifier:
; ASM: B.IOS mask=1111, ->S0<128B>
; ASM: B.IOS S0.reuse, mask=1111
; ASM: B.IOS S0, mask=1111
; MIR: INLINEASM &"B.IOS mask=1111, ->${0:S}<128B>", {{.*}}regdef:Shared_ABS{{.*}}, def renamable $shared_s0
; MIR: INLINEASM &"B.IOS ${0:S}, mask=1111", {{.*}}reguse:Shared_ABS{{.*}}, renamable $shared_s0
; MIR: INLINEASM &"B.IOS ${0:K}, mask=1111", {{.*}}reguse:Shared_ABS{{.*}}, killed renamable $shared_s0
; OBJ-LABEL: <shared_inline_asm_modifier>:
; OBJ: B.IOS mask=1111, ->S0<128B>
; OBJ: B.IOS S0.reuse, mask=1111
; OBJ: B.IOS S0, mask=1111
define void @shared_inline_asm_modifier() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  call void asm sideeffect "B.IOS ${0:K}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}

; One killed MachineOperand can appear more than once in the asm text. `%S`
; must retain every occurrence instead of treating each one as last-use.
; ASM-LABEL: shared_repeated_placeholder:
; ASM: B.IOS mask=1111, ->S0<128B>
; ASM: B.IOS S0.reuse, mask=1111
; ASM: B.IOS S0.reuse, mask=1111
; MIR: INLINEASM &"B.IOS ${0:S}, mask=1111\0AB.IOS ${0:S}, mask=1111", {{.*}}reguse:Shared_ABS{{.*}}, killed renamable $shared_s0
; OBJ-LABEL: <shared_repeated_placeholder>:
; OBJ: B.IOS mask=1111, ->S0<128B>
; OBJ: B.IOS S0.reuse, mask=1111
; OBJ: B.IOS S0.reuse, mask=1111
define void @shared_repeated_placeholder() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:S}, mask=1111\0AB.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}

; `%R` explicitly retains a tied output/input occurrence even though operand
; zero is represented as a definition. `%K` then names the exact final use.
; ASM-LABEL: shared_tied_retain:
; ASM: B.IOS mask=1111, ->S0<128B>
; ASM: B.IOS S0.reuse, mask=1111
; ASM: B.IOS S0, mask=1111
; OBJ-LABEL: <shared_tied_retain>:
; OBJ: B.IOS mask=1111, ->S0<128B>
; OBJ: B.IOS S0.reuse, mask=1111
; OBJ: B.IOS S0, mask=1111
define void @shared_tied_retain() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  %same = call i64 asm sideeffect "B.IOS ${0:R}, mask=1111", "=@2Sr,0"(i64 %shared)
  call void asm sideeffect "B.IOS ${0:K}, mask=1111", "@2Sr"(i64 %same)
  ret void
}
