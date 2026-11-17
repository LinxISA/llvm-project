# RUN: llvm-mc -triple=linx64v5 -mattr=+relax -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s
# RUN: ld.lld %t.o --entry=entry -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=DIS
.text
.globl entry
entry:
ld.pcr.intra [target], ->a0
# CHECK: 0x2 R_LinxV5_Load_Symbol target 0x2
# DIS: ld.pcr.intra
sd.pcr.inter a0, [target]
# CHECK: 0xA R_LinxV5_Store_Symbol target 0x2
# DIS: sd.pcr.inter
hl.ld.pcr.private [target], ->a1
# CHECK: 0x12 R_LinxV5_Load_Symbol_Target_29 target 0x2
# DIS: hl.ld.pcr.private
hl.sd.pcr.intra a1, [target]
# CHECK: 0x1A R_LinxV5_Store_Symbol_Target_29 target 0x2
# DIS: hl.sd.pcr.intra
# CHECK-NOT: R_LinxV5_RELAX
.p2align 3
target:
.quad 0x123456789abcdef0
