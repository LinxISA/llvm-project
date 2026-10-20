# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

# Local B.IOT destination SizeCode is 1..10 (128B..64KB per PE).
# No-source destination Func=6 must reject zero and reserved codes 11..15.
# Source-only bindings with SizeCode=0 are covered by lifetime tests.
.byte 0x13, 0xee, 0x08, 0x00 // SizeCode=1, Last=1
.byte 0x13, 0x6e, 0x0d, 0x00 // SizeCode=10, Last=1
.byte 0x13, 0xee, 0x0d, 0x00 // SizeCode=11
.byte 0x13, 0x6e, 0x0e, 0x00 // SizeCode=12
.byte 0x13, 0xee, 0x0e, 0x00 // SizeCode=13
.byte 0x13, 0x6e, 0x0f, 0x00 // SizeCode=14
.byte 0x13, 0xee, 0x0f, 0x00 // SizeCode=15
.byte 0x13, 0x6e, 0x00, 0x00 // SizeCode=0, not a destination

# CHECK: B.IOT{{.*}}->t<128B>
# CHECK: B.IOT{{.*}}->t<64KB>
# CHECK-COUNT-6: <unknown>
