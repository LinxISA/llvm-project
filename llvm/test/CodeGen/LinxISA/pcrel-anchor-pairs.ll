; RUN: llc -mtriple=linx64 -O0 -filetype=obj %s -o %t.o
; RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
; RUN: llc -mtriple=linx64 -O0 %s -o - | FileCheck %s --check-prefix=ASM
; RUN: llc -mtriple=linx64 -O0 -relocation-model=pic -filetype=obj %s -o %t.pic.o
; RUN: llvm-readobj -r %t.pic.o | FileCheck %s --check-prefix=GOT
; RUN: ld.lld -shared --unresolved-symbols=ignore-all %t.pic.o -o %t.pic.so
; RUN: llvm-readobj -S -r --dyn-relocations %t.pic.so \
; RUN:   | FileCheck %s --check-prefix=GOT-LINK

@target = external global i8

declare void @sink(ptr)

define void @repeated_symbol(i1 %cond) noinline optnone {
entry:
  br i1 %cond, label %left, label %right

left:
  call void asm sideeffect "# left path", ""()
  call void @sink(ptr @target)
  br label %exit

right:
  call void asm sideeffect "# right path", ""()
  call void @sink(ptr @target)
  br label %exit

exit:
  ret void
}

; RELOC:      0x16 R_LINX_PCREL_HI20 target 0x0
; RELOC-NEXT: 0x1A R_LINX_LO12 .text 0x16
; RELOC:      0x28 R_LINX_PCREL_HI20 target 0x0
; RELOC-NEXT: 0x2C R_LINX_LO12 .text 0x28
; GOT:      0x16 R_LINX_GOT_HI20 target 0x0
; GOT-NEXT: 0x1A R_LINX_GOT_LO12 .text 0x16
; GOT:      0x2C R_LINX_GOT_HI20 target 0x0
; GOT-NEXT: 0x30 R_LINX_GOT_LO12 .text 0x2C
; GOT-LINK:      Name: .got
; GOT-LINK:      Size: 8
; GOT-LINK:      R_LINX_64 target
; GOT-LINK-NOT:  pcrel_hi

; ASM:      .Lpcrel_hi0:
; ASM-NEXT: addtpc target, ->a0
; ASM-NEXT: addi a0, .Lpcrel_hi0, ->a0
; ASM:      .Lpcrel_hi1:
; ASM-NEXT: addtpc target, ->a0
; ASM-NEXT: addi a0, .Lpcrel_hi1, ->a0
