# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
# RUN: llvm-readelf -S -x .note.pto.isa %t | FileCheck %s --check-prefix=NOTE
# RUN: llvm-mc -triple=linx32 -filetype=obj %s -o %t.32
# RUN: llvm-readelf -S -x .note.pto.isa %t.32 | FileCheck %s --check-prefix=NOTE

# PTO ISA 0.57.1 raw producer ABI. ACC is selected by the CUBE opcode and is
# therefore intentionally absent from the B.IOT destination encoding.

B.CATR trap, atomic, aqrl, far, dr
B.DATR ND2NZ.normal, FP16, Max, cmode5, rmode6, sat, canonicalize
B.IOT t#1.reuse, last, ->u<1KB>
B.IOT t#1.reuse, u#2, last, ->m<8KB>
B.IOT m#3.reuse, last
B.IOT last, ->n<128B>
B.IOT t#4, n#5.reuse
BSTART.TEPL 0, 1, FP16
BSTART.TEPL 3, 14, FP32
BSTART.TTRANS FP16
BSTART.TSORT FP16

# ENC: B.CATR trap, atomic, aqrl, far, dr{{.*}}encoding: [0x23,0x80,0x0f,0x04]
# ENC: B.DATR ND2NZ.normal, FP16, Max, cmode5, rmode6, sat, canonicalize{{.*}}encoding: [0x23,0x11,0x23,0xb6]
# ENC: B.IOT t#1.reuse, last, ->u<1KB>{{.*}}encoding: [0x93,0x54,0x0b,0x00]
# ENC: B.IOT t#1.reuse, u#2, last, ->m<8KB>{{.*}}encoding: [0x13,0xc5,0x0c,0x44]
# ENC: B.IOT m#3.reuse, last{{.*}}encoding: [0x13,0x54,0x28,0x02]
# ENC: B.IOT last, ->n<128B>{{.*}}encoding: [0x93,0xe1,0x09,0x00]
# ENC: B.IOT t#4, n#5.reuse{{.*}}encoding: [0x13,0x48,0x30,0xd0]
# ENC: BSTART.TSUB FP16{{.*}}encoding: [0x81,0x91,0x11,0x10]
# ENC: BSTART.TTRANS FP32{{.*}}encoding: [0x81,0x91,0xe1,0x0e]
# ENC: BSTART.TTRANS FP16{{.*}}encoding: [0x81,0x91,0xe1,0x16]
# ENC: BSTART.TSORT FP16{{.*}}encoding: [0x81,0x91,0xc1,0x16]

# DIS: B.DATR{{[[:space:]]+}}ND2NZ.normal, FP16, Max, cmode5, rmode6, sat, canonicalize
# DIS: B.IOT{{[[:space:]]+}}t#1.reuse, u#2, last,{{[[:space:]]+}}->m<8KB>
# DIS: BSTART.TTRANS{{[[:space:]]+}}FP16
# DIS: BSTART.TSORT{{[[:space:]]+}}FP16

# NOTE: .note.pto.isa     NOTE
# NOTE-SAME: 0000b8
# NOTE-SAME: A
# NOTE-SAME: 4
# NOTE: 04000000 a5000000 01000000 50544f00
# NOTE: 7b22656e 636f6469 6e675f61 6269223a
# NOTE: 372e3122 7d000000
