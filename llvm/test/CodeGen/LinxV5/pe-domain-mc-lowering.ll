; RUN: llc -mtriple=linx64v5 -filetype=asm %s -o %t.s
; RUN: FileCheck %s --check-prefix=ASM < %t.s
; RUN: llc -mtriple=linx64v5 -filetype=obj %s -o %t.direct.o
; RUN: llvm-mc -triple=linx64v5 -filetype=obj %t.s -o %t.roundtrip.o
; RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.direct.o | \
; RUN:   sed '1,2d' > %t.direct.dis
; RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.roundtrip.o | \
; RUN:   sed '1,2d' > %t.roundtrip.dis
; RUN: cmp %t.direct.dis %t.roundtrip.dis
; RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.direct.o | \
; RUN:   FileCheck %s --check-prefix=DIS

; ASM-LABEL: domain8_first:
; ASM: .pto_pe_domain 8
; ASM: B.IOT mask=1111, last, ->t<128B>
; ASM: .pto_pe_domain 15
; ASM-LABEL: default15:
; ASM: .pto_pe_domain 15
; ASM: B.IOT mask=1111, last, ->t<128B>
; ASM: .pto_pe_domain 15
; ASM-LABEL: domain8_last:
; ASM: .pto_pe_domain 8
; ASM: B.IOT mask=1111, last, ->t<128B>
; ASM: .pto_pe_domain 15

; DIS-LABEL: <domain8_first>:
; DIS: B.IOT mask=1000, last, ->t<128B>
; DIS-LABEL: <default15>:
; DIS: B.IOT mask=1111, last, ->t<128B>
; DIS-LABEL: <domain8_last>:
; DIS: B.IOT mask=1000, last, ->t<128B>

define void @domain8_first() #0 {
  call void asm sideeffect "BSTART.TEPL TADD, U32\0AB.IOT mask=1111, last, ->t<128B>\0ABSTOP", ""()
  ret void
}

define void @default15() {
  call void asm sideeffect "BSTART.TEPL TADD, U32\0AB.IOT mask=1111, last, ->t<128B>\0ABSTOP", ""()
  ret void
}

define void @domain8_last() #0 {
  call void asm sideeffect "BSTART.TEPL TADD, U32\0AB.IOT mask=1111, last, ->t<128B>\0ABSTOP", ""()
  ret void
}

attributes #0 = { noinline "linx-tile-pe-domain"="8" "linx-tile-pe-domain-lowered" }

!llvm.module.flags = !{!0}
!0 = !{i32 1, !"linx.pe-domain.lowered", i32 1}
