# RUN: llvm-mc -filetype=obj -triple=linx64v5 %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld -e _start -Ttext=0 %t.o -o %t
# RUN: llvm-objdump -d %t | FileCheck %s
# RUN: llvm-mc -filetype=obj -triple=linx64v5 --defsym=bad_align=1 %s -o %t.align.o
# RUN: not ld.lld -e _start -Ttext=0 %t.align.o -o %t.align 2>&1 | FileCheck %s --check-prefix=ALIGN
# RUN: llvm-mc -filetype=obj -triple=linx64v5 --defsym=bad_range=1 %s -o %t.range.o
# RUN: not ld.lld -e _start -Ttext=0 %t.range.o -o %t.range 2>&1 | FileCheck %s --check-prefix=RANGE
# ALIGN: error: {{.*}}improper alignment
# RANGE: error: {{.*}}out of range
# RELOC: R_LinxV5_C_ADDPC .Lreturn
# CHECK-LABEL: <_start>:
# CHECK: 5096 c.setret 2, ->ra <.Lreturn>
# CHECK-LABEL: <.Lreturn>:
# CHECK: 5096 c.setret 2, ->ra
# CHECK-LABEL: <.Loffset>:

.globl _start
_start:
  c.setret .Lreturn, ->ra
  c.bstop
.Lreturn:
  c.setret .Loffset-2, ->ra
  c.bstop
  c.bstop
.Loffset:
  c.bstop

.ifdef bad_align
  c.setret .Loffset+1, ->ra
.endif
.ifdef bad_range
  c.setret .Loffset+128, ->ra
.endif
