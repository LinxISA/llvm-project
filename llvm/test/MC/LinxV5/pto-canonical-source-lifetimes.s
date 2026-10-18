# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -show-encoding %s > %t.roundtrip.s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t.roundtrip.s -o %t.roundtrip.o
# RUN: clang --target=linx64v5 -c %s -o %t.ias.o
# RUN: llvm-objcopy --only-section=.text -O binary %t.o %t.bin
# RUN: llvm-objcopy --only-section=.text -O binary %t.roundtrip.o %t.roundtrip.bin
# RUN: llvm-objcopy --only-section=.text -O binary %t.ias.o %t.ias.bin
# RUN: cmp %t.bin %t.roundtrip.bin
# RUN: cmp %t.bin %t.ias.bin
# RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=DIS

# PTO f0a876e B.IOT: dual-source Func=2/3/4/7 is last/last,
# reuse/last, reuse/reuse, last/reuse. Single-source last-use sets bit26.
# B.IOS source last-use also sets bit26; a destination reserves that bit.
B.IOT t#1.reuse, mask=1111, last
# ENC: encoding: [0x13,0x5e,0x08,0x00]
# DIS: B.IOT{{[ \t]+}}t#1.reuse,{{[ \t]*}}mask=1111,{{[ \t]*}}last
B.IOT t#1, mask=1111, last
# ENC: encoding: [0x13,0x5e,0x08,0x04]
# DIS: B.IOT{{[ \t]+}}t#1,{{[ \t]*}}mask=1111,{{[ \t]*}}last
B.IOT t#1, u#1, mask=1111, last
# ENC: encoding: [0x13,0x2e,0x08,0x40]
# DIS: B.IOT{{[ \t]+}}t#1,{{[ \t]*}}u#1,{{[ \t]*}}mask=1111,{{[ \t]*}}last
B.IOT t#1.reuse, u#1, mask=1111, last
# ENC: encoding: [0x13,0x3e,0x08,0x40]
# DIS: B.IOT{{[ \t]+}}t#1.reuse,{{[ \t]*}}u#1,{{[ \t]*}}mask=1111,{{[ \t]*}}last
B.IOT t#1.reuse, u#1.reuse, mask=1111, last
# ENC: encoding: [0x13,0x4e,0x08,0x40]
# DIS: B.IOT{{[ \t]+}}t#1.reuse,{{[ \t]*}}u#1.reuse,{{[ \t]*}}mask=1111,{{[ \t]*}}last
B.IOT t#1, u#1.reuse, mask=1111, last
# ENC: encoding: [0x13,0x7e,0x08,0x40]
# DIS: B.IOT{{[ \t]+}}t#1,{{[ \t]*}}u#1.reuse,{{[ \t]*}}mask=1111,{{[ \t]*}}last
B.IOT t#1.reuse, mask=1111, last, ->t<128B>
# ENC: encoding: [0x13,0xde,0x08,0x00]
# DIS: B.IOT{{[ \t]+}}t#1.reuse,{{[ \t]*}}mask=1111,{{[ \t]*}}last,{{[ \t]*}}->t<128B>
B.IOT t#1, mask=1111, last, ->t<128B>
# ENC: encoding: [0x13,0xde,0x08,0x04]
# DIS: B.IOT{{[ \t]+}}t#1,{{[ \t]*}}mask=1111,{{[ \t]*}}last,{{[ \t]*}}->t<128B>
B.IOT t#1, u#1, mask=1111, last, ->t<128B>
# ENC: encoding: [0x13,0xae,0x08,0x40]
# DIS: B.IOT{{[ \t]+}}t#1,{{[ \t]*}}u#1,{{[ \t]*}}mask=1111,{{[ \t]*}}last,{{[ \t]*}}->t<128B>
B.IOT t#1.reuse, u#1, mask=1111, last, ->t<128B>
# ENC: encoding: [0x13,0xbe,0x08,0x40]
# DIS: B.IOT{{[ \t]+}}t#1.reuse,{{[ \t]*}}u#1,{{[ \t]*}}mask=1111,{{[ \t]*}}last,{{[ \t]*}}->t<128B>
B.IOT t#1.reuse, u#1.reuse, mask=1111, last, ->t<128B>
# ENC: encoding: [0x13,0xce,0x08,0x40]
# DIS: B.IOT{{[ \t]+}}t#1.reuse,{{[ \t]*}}u#1.reuse,{{[ \t]*}}mask=1111,{{[ \t]*}}last,{{[ \t]*}}->t<128B>
B.IOT t#1, u#1.reuse, mask=1111, last, ->t<128B>
# ENC: encoding: [0x13,0xfe,0x08,0x40]
# DIS: B.IOT{{[ \t]+}}t#1,{{[ \t]*}}u#1.reuse,{{[ \t]*}}mask=1111,{{[ \t]*}}last,{{[ \t]*}}->t<128B>
B.IOS S3.reuse, mask=1111
# ENC: encoding: [0x13,0x1e,0x30,0x00]
# DIS: B.IOS{{[ \t]+}}S3.reuse,{{[ \t]*}}mask=1111
B.IOS S3, mask=1111
# ENC: encoding: [0x13,0x1e,0x30,0x04]
# DIS: B.IOS{{[ \t]+}}S3,{{[ \t]*}}mask=1111
B.IOS mask=1111, ->S3<128B>
# ENC: encoding: [0x13,0x9e,0x30,0x00]
# DIS: B.IOS{{[ \t]+}}mask=1111,{{[ \t]*}}->S3<128B>
B.IOT mask=1111, last, ->t<64KB>
# ENC: encoding: [0x13,0x6e,0x0d,0x00]
# DIS: B.IOT{{.*}}->t<64KB>
B.IOS mask=1111, ->S3<256KB>
# ENC: encoding: [0x13,0x1e,0x36,0x00]
# DIS: B.IOS{{.*}}->S3<256KB>
