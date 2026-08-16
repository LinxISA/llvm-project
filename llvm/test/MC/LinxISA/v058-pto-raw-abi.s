# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
# RUN: llvm-readelf -S -x .note.pto.isa %t | FileCheck %s --check-prefix=NOTE
# RUN: llvm-mc -triple=linx32 -filetype=obj %s -o %t.32
# RUN: llvm-readelf -S -x .note.pto.isa %t.32 | FileCheck %s --check-prefix=NOTE

# PTO ISA 0.58 raw producer ABI. CUBE reads and writes explicit Local tiles;
# B.IOT therefore uses only the architectural t/u/m/n Local banks.

B.CATR trap, atomic, aqrl, far, dr
B.DATR ND2NZ.normal, FP16, Max, cmode5, rmode6, sat, canonicalize
B.FPATR 1, 2, 3, 1, 0, 1, 1
B.IOT t#1, mask=1111, last, ->u<1KB>
B.IOT t#1, u#2, mask=0011, last, ->m<8KB>
B.IOT m#3, mask=1100, last
B.IOT mask=0001, last, ->n<128B>
B.IOT t#4, n#5, mask=1111
BSTART.TEPL 0, 1, FP16
BSTART.TEPL 3, 14, FP32
BSTART.SFU TTRANS, FP16
BSTART.SFU TSORT, FP16
L.BSTOP

# ENC: B.CATR trap, atomic, aqrl, far, dr{{.*}}encoding: [0x23,0x80,0x0f,0x04]
# ENC: B.DATR ND2NZ.normal, FP16, Max, cmode5, rmode6, sat, canonicalize{{.*}}encoding: [0x23,0x11,0x43,0xb6]
# ENC: B.FPATR 1, 2, 3, 1, 0, 1, 1{{.*}}encoding: [0x23,0xa0,0x1d,0x05]
# ENC: B.IOT t#1, mask=1111, last, ->u<1KB>{{.*}}encoding:
# ENC: B.IOT t#1, u#2, mask=0011, last, ->m<8KB>{{.*}}encoding:
# ENC: B.IOT m#3, mask=1100, last{{.*}}encoding:
# ENC: B.IOT mask=0001, last, ->n<128B>{{.*}}encoding:
# ENC: B.IOT t#4, n#5, mask=1111{{.*}}encoding:
# ENC: BSTART.VEC TSUB, FP16{{.*}}encoding:
# ENC: BSTART.SFU TTRANS, FP32{{.*}}encoding:
# ENC: BSTART.SFU TTRANS, FP16{{.*}}encoding:
# ENC: BSTART.SFU TSORT, FP16{{.*}}encoding:
# ENC: L.BSTOP{{.*}}encoding: [0x0f,0x00,0x00,0x00,0x01,0x00,0x00,0x00]

# DIS: B.DATR{{[[:space:]]+}}ND2NZ.normal, FP16, Max, cmode5, rmode6, sat, canonicalize
# DIS: B.FPATR{{[[:space:]]+}}1, 2, 3, 1, 0, 1, 1
# DIS: B.IOT{{[[:space:]]+}}t#1, u#2, mask=0011, last,{{[[:space:]]+}}->m<8KB>
# DIS: BSTART.SFU{{[[:space:]]+}}TTRANS, FP16
# DIS: BSTART.SFU{{[[:space:]]+}}TSORT, FP16
# DIS: L.BSTOP

# NOTE: .note.pto.isa     NOTE
# NOTE-SAME: 0000b8
# NOTE-SAME: A
# NOTE-SAME: 4
# NOTE: 04000000 a5000000 01000000 50544f00
# NOTE: 7b22656e 636f6469 6e675f61 6269223a
# NOTE: 382e3122 7d000000
