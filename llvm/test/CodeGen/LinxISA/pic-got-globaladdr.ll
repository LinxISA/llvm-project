; RUN: llc -mtriple=linx64-unknown-linux-gnu -relocation-model=pic -filetype=obj < %s | llvm-readobj -r - | FileCheck %s --check-prefix=RELOC
; RUN: llc -mtriple=linx64-unknown-linux-gnu -relocation-model=pic < %s | FileCheck %s --check-prefix=ASM

@g = external global i8

define ptr @get_g_addr() {
entry:
  ret ptr @g
}

; ASM-LABEL: get_g_addr:
; ASM: @got
; ASM: @got

; RELOC: R_LINX_GOT_HI20{{[[:space:]]+}}g
; RELOC: R_LINX_GOT_LO12{{[[:space:]]+}}.text
; RELOC-NOT: R_LINX_PCR17_LOAD
; RELOC-NOT: R_LINX_HL_PCR29_LOAD
