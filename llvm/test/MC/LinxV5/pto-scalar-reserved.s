# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s
# Reserved FSU types and ACRE request selectors must not decode.
.word 0x0600007b
# CHECK: <unknown>
.word 0x0400007b
# CHECK: <unknown>
.word 0x0600004b
# CHECK: <unknown>
.word 0x0400004b
# CHECK: <unknown>
.word 0x0600304b
# CHECK: <unknown>
.word 0x0400304b
# CHECK: <unknown>
.word 0x0600005b
# CHECK: <unknown>
.word 0x0400005b
# CHECK: <unknown>
.word 0x0e00005b
# CHECK: <unknown>
.word 0x0c00005b
# CHECK: <unknown>
.word 0x0600307b
# CHECK: <unknown>
.word 0x0400307b
# CHECK: <unknown>
.word 0x0600305b
# CHECK: <unknown>
.word 0x0400305b
# CHECK: <unknown>
.word 0x0e00305b
# CHECK: <unknown>
.word 0x0c00305b
# CHECK: <unknown>
.word 0x0600205b
# CHECK: <unknown>
.word 0x0400205b
# CHECK: <unknown>
.word 0x0e00205b
# CHECK: <unknown>
.word 0x0c00205b
# CHECK: <unknown>
.word 0x0600404b
# CHECK: <unknown>
.word 0x0400404b
# CHECK: <unknown>
.word 0x0600605b
# CHECK: <unknown>
.word 0x0400605b
# CHECK: <unknown>
.word 0x0600705b
# CHECK: <unknown>
.word 0x0400705b
# CHECK: <unknown>
.word 0x0600504b
# CHECK: <unknown>
.word 0x0400504b
# CHECK: <unknown>
.word 0x0600204b
# CHECK: <unknown>
.word 0x0400204b
# CHECK: <unknown>
.word 0x0600105b
# CHECK: <unknown>
.word 0x0400105b
# CHECK: <unknown>
.word 0x0e00105b
# CHECK: <unknown>
.word 0x0c00105b
# CHECK: <unknown>
.word 0x0600604b
# CHECK: <unknown>
.word 0x0400604b
# CHECK: <unknown>
.word 0x0600704b
# CHECK: <unknown>
.word 0x0400704b
# CHECK: <unknown>
.word 0x0600207b
# CHECK: <unknown>
.word 0x0400207b
# CHECK: <unknown>
.word 0x0600107b
# CHECK: <unknown>
.word 0x0400107b
# CHECK: <unknown>
.word 0x0600104b
# CHECK: <unknown>
.word 0x0400104b
# CHECK: <unknown>
.word 0x0120302b
# CHECK: <unknown>
.word 0x0130302b
# CHECK: <unknown>
.word 0x0140302b
# CHECK: <unknown>
.word 0x0150302b
# CHECK: <unknown>
.word 0x0160302b
# CHECK: <unknown>
.word 0x0170302b
# CHECK: <unknown>
.word 0x0180302b
# CHECK: <unknown>
.word 0x0190302b
# CHECK: <unknown>
.word 0x01a0302b
# CHECK: <unknown>
.word 0x01b0302b
# CHECK: <unknown>
.word 0x01c0302b
# CHECK: <unknown>
.word 0x01d0302b
# CHECK: <unknown>
.word 0x01e0302b
# CHECK: <unknown>
.word 0x01f0302b
# CHECK: <unknown>
