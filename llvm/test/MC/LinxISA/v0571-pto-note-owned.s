# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

.section .note.pto.isa,"a",@note
.byte 0

# CHECK: error: LinxISA: input assembly must not define .note.pto.isa
# CHECK-NOT: PLEASE submit a bug report
