# RUN: rm -rf %t && split-file %s %t
# RUN: llvm-mc -triple=linx64 -filetype=obj %t/calls.s -o %t/calls.o
# RUN: llvm-readobj -r %t/calls.o | FileCheck %s --check-prefix=RELOC
# RUN: llvm-mc -triple=linx64 -filetype=obj %t/defs.s -o %t/defs.o
# RUN: ld.lld -e fused_external %t/calls.o %t/defs.o -o %t/linked
# RUN: llvm-objdump -d %t/linked | FileCheck %s --check-prefix=DIS
# RUN: llvm-objdump -s -j .text %t/linked | FileCheck %s --check-prefix=BYTES
# RUN: llvm-mc -triple=linx64 -filetype=obj %t/independent.s -o %t/independent.o
# RUN: llvm-objdump -s -j .text %t/independent.o | FileCheck %s --check-prefix=INDEPENDENT
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/bad.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

# RELOC: 0x0 R_LINX_CBSTART12_PCREL ext_call32 0x0
# RELOC-NEXT: 0x2 R_LINX_CSETRET5_PCREL ext_ret32 0x0
# RELOC-NEXT: 0x4 R_LINX_B25_PCREL ext_call48 0x0
# RELOC-NEXT: 0x8 R_LINX_CSETRET5_PCREL ext_ret48 0x0
# RELOC-NEXT: 0xC R_LINX_CSETRET5_PCREL ext_icall_ret 0x0

# DIS-LABEL: <fused_external>:
# DIS: {{[[:xdigit:]]+}}: 92 00 d6 51{{[[:space:]]+}}BSTART.CALL{{[[:space:]]+}}0x{{[[:xdigit:]]+}}, 0x{{[[:xdigit:]]+}},{{[[:space:]]+}}->ra
# DIS-NEXT: {{[[:xdigit:]]+}}: 91 04 00 00 96 51{{[[:space:]]+}}HL.BSTART.CALL{{[[:space:]]+}}0x{{[[:xdigit:]]+}}, 0x{{[[:xdigit:]]+}},{{[[:space:]]+}}->ra
# DIS-NEXT: {{[[:xdigit:]]+}}: 01 60 96 51{{[[:space:]]+}}BSTART.ICALL{{[[:space:]]+}}0x{{[[:xdigit:]]+}},{{[[:space:]]+}}->ra
# DIS-LABEL: <ext_ret32>:
# DIS-LABEL: <ext_call32>:
# DIS-LABEL: <ext_ret48>:
# DIS-LABEL: <ext_call48>:
# DIS-LABEL: <ext_icall_ret>:

# The linked encodings use independent call and return relocations.
# BYTES: 9200d651 91040000 96510160 9651

# Changing each raw immediate independently changes only its own field.
# INDEPENDENT: 32005650 42005650 32009650 11020000
# INDEPENDENT-NEXT: 56509102 00005650 11020000 9650

# ERR: error: expected call target and return target for fused BSTART.CALL
# ERR: error: fused BSTART.CALL destination must be ->ra

#--- calls.s
.text
.globl fused_external
fused_external:
  BSTART.CALL ext_call32, ext_ret32, ->ra
  HL.BSTART.CALL ext_call48, ext_ret48, ->ra
  BSTART.ICALL ext_icall_ret, ->ra

#--- defs.s
.text
.globl ext_ret32
ext_ret32:
  .2byte 0
.globl ext_call32
ext_call32:
  .2byte 0
.globl ext_ret48
ext_ret48:
  .2byte 0
.globl ext_call48
ext_call48:
  .2byte 0
.globl ext_icall_ret
ext_icall_ret:
  .2byte 0

#--- independent.s
.text
  BSTART.CALL 3, 1, ->ra
  BSTART.CALL 4, 1, ->ra
  BSTART.CALL 3, 2, ->ra
  HL.BSTART.CALL 4, 1, ->ra
  HL.BSTART.CALL 5, 1, ->ra
  HL.BSTART.CALL 4, 2, ->ra

#--- bad.s
.text
  BSTART.CALL only_one_target, ->ra
  HL.BSTART.CALL call_target, return_target, ->sp
