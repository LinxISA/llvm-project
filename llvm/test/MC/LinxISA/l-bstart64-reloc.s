# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -triple=linx64 -filetype=obj %t/good.s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: llvm-readelf -r %t.o | FileCheck %s --check-prefix=RELOCNUM
# RUN: llvm-objdump -d --triple=linx64 %t.o | FileCheck %s --check-prefix=BOUND
# RUN: llvm-mc -triple=linx64 -show-encoding %t/good.s | FileCheck %s --check-prefix=EXPR
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/odd.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ODD
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/overflow-positive.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=POSRANGE
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/overflow-negative.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=NEGRANGE
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/invalid-kind.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=KIND
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/missing-label.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=LABEL
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/legacy.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=LEGACY

#--- good.s
	.text
	.globl l_bstart64_reloc
l_bstart64_reloc:
	L.BSTART.STD DIRECT, .Llocal
	L.BSTART.FP CALL, cross_section
	L.BSTART.SYS FALL, external_target
	L.BSTART.STD FALL, 4398046511102
	L.BSTART.FP FALL, -4398046511104
.Llocal:

	.section .text.cross,"ax",@progbits
cross_section:
	C.BSTOP

# RELOC: Relocations [
# RELOC-NEXT: Section {{.*}} .rela.text {
# RELOC-NEXT: 0x8 R_LINX_L_BSTART42_PCREL .text.cross 0x0
# RELOC-NEXT: 0x10 R_LINX_L_BSTART42_PCREL external_target 0x0
# RELOC-NEXT: }
# RELOC-NEXT: ]
# RELOCNUM: {{[0-9a-f]+1a}} R_LINX_L_BSTART42_PCREL
# BOUND: 8f ff ff ff 01 90 ff 7f{{[[:space:]]+}}L.BSTART.STD{{[[:space:]]+}}FALL, 4398046511102
# BOUND: 0f 00 00 00 81 10 00 80{{[[:space:]]+}}L.BSTART.FP{{[[:space:]]+}}FALL, -4398046511104
# EXPR: L.BSTART.SYS{{[[:space:]]+}}FALL, external_target
# EXPR: value: external_target, kind: FIXUP_LINX_L_BSTART42_PCREL

#--- odd.s
	L.BSTART.STD FALL, 1
# ODD: error: L.BSTART target is not 2-byte aligned

#--- overflow-positive.s
	L.BSTART.STD FALL, 4398046511104
# POSRANGE: error: L.BSTART target out of range

#--- overflow-negative.s
	L.BSTART.STD FALL, -4398046511106
# NEGRANGE: error: L.BSTART target out of range

#--- invalid-kind.s
	L.BSTART.SYS DIRECT, 0
# KIND: error: branch kind does not match BSTART encoding

#--- missing-label.s
	L.BSTART.STD DIRECT
# LABEL: error:

#--- legacy.s
	L.ADD a0, a1, ->a2
# LEGACY: error: legacy 'L.*' mnemonics are not allowed in canonical PTO 0.58
