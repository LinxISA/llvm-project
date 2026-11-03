; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false %s -o - | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump --disassembler-options=no-tile-macros -d --no-show-raw-insn %t | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -linxv5-enable-inline-asm-tile-lifetime=false %s -o - | FileCheck %s --check-prefix=OFF
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -linxv5-enable-generic-element=true %s -o - | FileCheck %s --check-prefix=OFF

; OFF-LABEL: shared_live_then_dead
; OFF: B.IOS{{[ \t]+}}S0.reuse, mask=1111
; OFF: B.IOS{{[ \t]+}}S0.reuse, mask=1111

; CHECK-LABEL: shared_live_then_dead
; CHECK: B.IOS{{[ \t]+}}S0.reuse, mask=1111
; CHECK: B.IOS{{[ \t]+}}S0, mask=1111
define void @shared_live_then_dead() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  call void asm sideeffect "B.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}

; CHECK-LABEL: shared_same_bundle
; CHECK: B.IOS{{[ \t]+}}S0.reuse, mask=1111
; CHECK: B.IOS{{[ \t]+}}S0.reuse, mask=1111
define void @shared_same_bundle() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:S}, mask=1111\0AB.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}

; CHECK-LABEL: shared_explicit_retain
; CHECK: B.IOS{{[ \t]+}}S0.reuse, mask=1111
define void @shared_explicit_retain() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:R}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}

; CHECK-LABEL: shared_partial
; CHECK: B.IOS{{[ \t]+}}S0.reuse, mask=0001
define void @shared_partial() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:S}, mask=0001", "@2Sr"(i64 %shared)
  ret void
}

; CHECK-LABEL: shared_associated_load
; CHECK: B.IOS{{[ \t]+}}S0.reuse, mask=1111
define void @shared_associated_load() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}

; CHECK-LABEL: shared_unknown_template
; CHECK: B.IOS{{[ \t]+}}S0.reuse, mask=1111
define void @shared_unknown_template() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:S}, mask=1111\0A# unknown instruction", "@2Sr"(i64 %shared)
  ret void
}

; CHECK-LABEL: local_live_then_dead
; CHECK: B.IOT{{[ \t]+}}{{[tumn]}}#{{[0-9]+}}.reuse, mask=1111, last
; CHECK: B.IOT{{[ \t]+}}{{[tumn]}}#{{[0-9]+}}, mask=1111, last
define void @local_live_then_dead() {
  %tile = call <16 x double> asm sideeffect "BSTART.TEPL 102, FP64\0AB.DATR NORM, RNONE\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 4, ->lb1\0AB.DIM zero, 4, ->lb2\0AB.IOT mask=1111, last, ->$0<${1:Z}>", "=@2Tr,i"(i64 1)
  call void asm sideeffect "B.IOT $0, mask=1111, last", "@2Tr"(<16 x double> %tile)
  call void asm sideeffect "B.IOT $0, mask=1111, last", "@2Tr"(<16 x double> %tile)
  ret void
}

; CHECK-LABEL: shared_constant_pe_mask
; CHECK: B.IOS{{[ \t]+}}S0.reuse, mask=1111
; CHECK: B.IOS{{[ \t]+}}S0, mask=1111
define void @shared_constant_pe_mask() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect ".if ${1:c} == 1\0AB.IOS ${0:S}, mask=0001\0A.elseif ${1:c} == 14\0AB.IOS ${0:S}, mask=1110\0A.elseif ${1:c} == 15\0AB.IOS ${0:S}, mask=1111\0A.endif\0A", "@2Sr,i"(i64 %shared, i32 15)
  call void asm sideeffect ".if ${1:c} == 1\0AB.IOS ${0:S}, mask=0001\0A.elseif ${1:c} == 14\0AB.IOS ${0:S}, mask=1110\0A.elseif ${1:c} == 15\0AB.IOS ${0:S}, mask=1111\0A.endif\0A", "@2Sr,i"(i64 %shared, i32 15)
  ret void
}
