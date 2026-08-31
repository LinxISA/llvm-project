# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-readobj --file-headers %t.o | FileCheck %s --check-prefix=OBJ
# RUN: ld.lld -e _start %t.o -o %t
# RUN: llvm-readobj --file-headers %t | FileCheck %s --check-prefix=EXEC

# OBJ: Type: Relocatable
# OBJ: Machine: EM_LinxV5 (0xE9)

# EXEC: Type: Executable
# EXEC: Machine: EM_LinxV5 (0xE9)

.globl _start
_start:
  c.bstop
