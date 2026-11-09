# RUN: llvm-mc -triple=linx64v5 -linxv5-emit-pto-isa-identity -filetype=obj %s -o %t.o
# RUN: llvm-readobj --sections --notes %t.o | FileCheck %s
# RUN: %python %S/../../../utils/linxv5/verify_pto_identity_object.py %t.o
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.legacy.o
# RUN: llvm-readobj --sections %t.legacy.o | FileCheck %s --check-prefix=LEGACY
# RUN: not llvm-mc -triple=linx64v5 -linxv5-emit-pto-isa-identity -defsym=DUPLICATE=1 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=DUPLICATE
# RUN: not llvm-mc -triple=linx64v5be -linxv5-emit-pto-isa-identity -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ENDIAN
.text
.byte 0
.ifdef DUPLICATE
.section .note.pto.isa,"a",@note
.long 0
.endif
# CHECK: Name: .note.pto.isa
# CHECK: Type: SHT_NOTE
# CHECK: SHF_ALLOC
# CHECK: AddressAlignment: 4
# CHECK: Owner: PTO
# CHECK: Data size: 0xA5
# LEGACY-NOT: .note.pto.isa
# DUPLICATE: PTO ISA identity section already exists
# ENDIAN: PTO ISA identity requires ELF64 little-endian LinxV5
