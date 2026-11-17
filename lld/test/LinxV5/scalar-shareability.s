# RUN: llvm-mc -triple=linx64v5 -mattr=+relax -filetype=obj %s -o %t.o
# RUN: llvm-readobj -r %t.o | FileCheck %s --check-prefix=RELOC
# RUN: ld.lld %t.o --entry=entry -Ttext=0x10000 -Tdata=0x11040 -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# RUN: ld.lld %t.o --entry=entry --no-relax -Ttext=0x10000 -Tdata=0x11040 -o %t.norelax
# RUN: llvm-objdump -d --no-show-raw-insn %t.norelax | FileCheck %s
.text
.globl entry
entry:
.Lhi:
addtpc %tpcrel_hi(target), ->t
# CHECK: addtpc 1,
ldi.u.intra [t#1, %tpcrel_lo(.Lhi)], ->a0
# RELOC: 0x6 R_LinxV5_TPCREL_LO12_L .Lhi 0x0
# CHECK: ldi.u.intra [t#1, 64],
sdi.u.inter a0, [t#1, %tpcrel_lo(.Lhi)]
# RELOC: 0xE R_LinxV5_TPCREL_LO12_S .Lhi 0x0
# CHECK: sdi.u.inter a0, [t#1, 64]
# A 48-bit payload at the very end of .text must not read/write beyond it.
hl.ld.pcr.private [target], ->a1
# RELOC: 0x16 R_LinxV5_Load_Symbol_Target_29 target 0x2
# CHECK: hl.ld.pcr.private [0x11040]
.data
target:
.quad 0x123456789abcdef0
