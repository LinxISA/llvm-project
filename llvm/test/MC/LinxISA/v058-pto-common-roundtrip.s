# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --triple=linx64 %t.o | FileCheck %s --check-prefix=DIS

	BSTART.VEC TADD, FP16
	BSTART.SFU TEXP, FP16
	BSTART.VEC TFMA, FP16
	B.IOR [zero, sp, a0], ->x3
	B.IOS S1, mask=1000
	B.IOS mask=1111, ->S255<256KB>
	B.IOT t#1, mask=1100, last, ->u<128B>
	B.IOT t#1, u#2, mask=1111
	B.IOT mask=1100, last, ->n<8KB>

# ENC: BSTART.VEC TADD, FP16{{.*}}encoding: [0x81,0x91,0x01,0x20]
# ENC: BSTART.SFU TEXP, FP16{{.*}}encoding: [0x81,0x91,0x21,0x21]
# ENC: BSTART.VEC TFMA, FP16{{.*}}encoding: [0x81,0x91,0xc1,0x21]
# ENC: B.IOR [zero, sp, a0], ->x3{{.*}}encoding: [0x93,0x0b,0x10,0x10]
# ENC: B.IOS S1, mask=1000{{.*}}encoding: [0x13,0x12,0x10,0x00]
# ENC: B.IOS mask=1111, ->S255<256KB>{{.*}}encoding: [0x13,0x1e,0xf6,0x0f]
# ENC: B.IOT t#1, mask=1100, last, ->u<128B>{{.*}}encoding: [0x93,0xda,0x08,0x00]
# ENC: B.IOT t#1, u#2, mask=1111{{.*}}encoding: [0x13,0x4e,0x00,0x44]
# ENC: B.IOT mask=1100, last, ->n<8KB>{{.*}}encoding: [0x93,0xeb,0x0b,0x00]

# DIS: BSTART.VEC{{[[:space:]]+}}TADD, FP16
# DIS: BSTART.SFU{{[[:space:]]+}}TEXP, FP16
# DIS: BSTART.VEC{{[[:space:]]+}}TFMA, FP16
# DIS: B.IOR{{[[:space:]]+}}[zero, sp, a0], ->x3
# DIS: B.IOS{{[[:space:]]+}}S1, mask=1000
# DIS: B.IOS{{[[:space:]]+}}mask=1111, ->S255<256KB>
# DIS: B.IOT{{[[:space:]]+}}t#1, mask=1100, last, ->u<128B>
# DIS: B.IOT{{[[:space:]]+}}t#1, u#2, mask=1111
# DIS: B.IOT{{[[:space:]]+}}mask=1100, last, ->n<8KB>
