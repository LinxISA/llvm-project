# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
# Assigned ignored fields and discard destinations decode canonically.
.word 0x00317f89
# CHECK: prf [a0, a1]
.word 0xfff17fa9
# CHECK: prfi.u [a0, -1]
.word 0x19f1018b
# CHECK: lr.d [a0], ->a1
.word 0x00314e0b
# CHECK: ld.add [a0], a1, ->zero
.word 0x00314e8b
# CHECK: ld.add [a0], a1, ->zero
.word 0x20310e9b
# CHECK: casb [a0], a1, a2, ->zero
# Nonblocking wait retains its own normative spelling.
bwt a0
# CHECK: bwt a0
# Acquire is reserved in store-only AMOs; invalid PRF modifiers stay invalid.
.word 0x0431300b
# CHECK: <unknown>
.word 0x06317009
# CHECK: <unknown>
.word 0x0330a0ab
# CHECK: <unknown>
