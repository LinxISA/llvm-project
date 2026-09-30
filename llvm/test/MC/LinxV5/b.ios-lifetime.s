# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=DIS

# Bare Shared sources are last-use (bit 26 one); `.reuse` retains the same
# Shared vtag generation (bit 26 zero). Existing bit-26-zero words therefore
# remain valid and disassemble with the explicit retain suffix.
B.IOS S3, mask=1111
B.IOS S3.reuse, mask=1111

# Destination B.IOS keeps bit 26 clear and does not accept a lifetime suffix.
B.IOS mask=1111, ->S3<512B>

# Raw destination with bit 26 set is reserved.
.byte 0x13, 0x9e, 0x31, 0x04

# ENC: B.IOS{{.*}}S3, mask=1111{{.*}}encoding: [0x13,0x1e,0x30,0x04]
# ENC: B.IOS{{.*}}S3.reuse, mask=1111{{.*}}encoding: [0x13,0x1e,0x30,0x00]
# ENC: B.IOS{{.*}}->S3<512B>{{.*}}encoding: [0x13,0x9e,0x31,0x00]
# DIS: B.IOS{{.*}}S3, mask=1111
# DIS: B.IOS{{.*}}S3.reuse, mask=1111
# DIS: B.IOS{{.*}}->S3<512B>
# DIS-NEXT: {{.*}}<unknown>
