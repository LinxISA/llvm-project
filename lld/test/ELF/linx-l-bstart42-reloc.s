# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=linx64 a.s -o a.o
# RUN: llvm-mc -filetype=obj -triple=linx64 b.s -o b.o
# RUN: ld.lld --no-relax a.o b.o -o linked
# RUN: llvm-objdump -d --triple=linx64 linked | FileCheck %s --check-prefix=LINKED
# RUN: llvm-readobj -r linked | FileCheck %s --check-prefix=NORELOC
# RUN: not ld.lld -shared --no-relax a.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=PREEMPT
# RUN: llvm-mc -filetype=obj -triple=linx64 boundary.s -o boundary.o
# RUN: ld.lld --no-relax -e boundary -T positive-boundary.lds boundary.o -o positive-boundary
# RUN: ld.lld --no-relax -e boundary -T negative-boundary.lds boundary.o -o negative-boundary
# RUN: llvm-objdump -d --triple=linx64 positive-boundary | FileCheck %s --check-prefix=POSBOUND
# RUN: llvm-objdump -d --triple=linx64 negative-boundary | FileCheck %s --check-prefix=NEGBOUND
# RUN: not ld.lld --no-relax -e boundary -T odd.lds boundary.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ODD
# RUN: not ld.lld --no-relax -e boundary -T positive-overflow.lds boundary.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=POSRANGE
# RUN: not ld.lld --no-relax -e boundary -T negative-overflow.lds boundary.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=NEGRANGE

# LINKED-LABEL: <_start>:
# LINKED: 0f 06 00 00 01 20 00 00{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}DIRECT, 24
# LINKED-NEXT: 0f 04 00 00 81 40 00 00{{[[:space:]]+}}L.BSTART.FP{{[[:space:]]+}}CALL, 16
# LINKED-NEXT: 0f 02 00 00 11 10 00 00{{[[:space:]]+}}L.BSTART.SYS{{[[:space:]]+}}FALL, 8
# NORELOC: Relocations [
# NORELOC-NEXT: ]
# POSBOUND: 8f ff ff ff 01 a0 ff 7f{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}DIRECT, 4398046511102
# NEGBOUND: 0f 00 00 00 01 20 00 80{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}DIRECT, -4398046511104
# ODD: error: {{.*}}unaligned L.BSTART target
# POSRANGE: error: {{.*}}relocation R_LINX_L_BSTART42_PCREL out of range
# NEGRANGE: error: {{.*}}relocation R_LINX_L_BSTART42_PCREL out of range
# PREEMPT: error: relocation R_LINX_L_BSTART42_PCREL cannot be used against symbol 'target'
# PREEMPT-NOT: R_LINX_NONE

#--- a.s
	.text
	.globl _start
_start:
	L.BSTART.STD DIRECT, target
	L.BSTART.FP CALL, target
	L.BSTART.SYS FALL, target

#--- b.s
	.text
	.globl target
target:
	C.BSTOP

#--- boundary.s
	.text
	.globl boundary
boundary:
	L.BSTART.STD DIRECT, boundary_target

#--- positive-boundary.lds
SECTIONS { . = 0; .text : { *(.text) } boundary_target = 0x3fffffffffe; }

#--- negative-boundary.lds
SECTIONS { . = 0x40000000000; .text : { *(.text) } boundary_target = 0; }

#--- odd.lds
SECTIONS { . = 0; .text : { *(.text) } boundary_target = 1; }

#--- positive-overflow.lds
SECTIONS { . = 0; .text : { *(.text) } boundary_target = 0x40000000000; }

#--- negative-overflow.lds
SECTIONS { . = 0x40000000002; .text : { *(.text) } boundary_target = 0; }
