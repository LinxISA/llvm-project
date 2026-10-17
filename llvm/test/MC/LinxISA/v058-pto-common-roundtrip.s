# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --triple=linx64 %t.o | FileCheck %s --check-prefix=DIS

	BSTART.VEC TADD, FP16
	BSTART.SFU TEXP, FP16
	BSTART.VEC TFMA, FP16
	B.IOR [zero, sp, a0], ->x3
	B.IOR [a0, a1, a2], ->a3, ExecMaskPresent
	B.DATR NORM, FP16, Null, predinv, zero
	B.IOS S1, mask=1000
	B.IOS S2.reuse, mask=0001
	B.IOS mask=1111, ->S63<256KB>
	B.IOT t#1, mask=1100, last, ->u<128B>
	B.IOT t#1, u#2, mask=1111
	B.IOT t#1.reuse, u#2.reuse, mask=1111, last, ->m<1KB>
	B.IOT mask=1100, last, ->n<8KB>

# ENC: BSTART.VEC TADD, FP16{{.*}}encoding: [0x81,0x91,0x01,0x20]
# ENC: BSTART.SFU TEXP, FP16{{.*}}encoding: [0x81,0x91,0x21,0x21]
# ENC: BSTART.VEC TFMA, FP16{{.*}}encoding: [0x81,0x91,0xc1,0x21]
# ENC: B.IOR [zero, sp, a0], ->x3{{.*}}encoding: [0x93,0x0b,0x10,0x10]
# ENC: B.IOR [a0, a1, a2], ->a3, ExecMaskPresent{{.*}}encoding: [0x93,0x02,0x31,0x24]
# ENC: B.DATR NORM, FP16, Null, cmode0, rmode0, predinv, zero{{.*}}encoding: [0x23,0x70,0x40,0x18]
# ENC: B.IOS S1, mask=1000{{.*}}encoding: [0x13,0x12,0x10,0x04]
# ENC: B.IOS S2.reuse, mask=0001{{.*}}encoding: [0x13,0x18,0x20,0x00]
# ENC: B.IOS mask=1111, ->S63<256KB>{{.*}}encoding: [0x13,0x1e,0xf6,0x03]
# ENC: B.IOT t#1, mask=1100, last, ->u<128B>{{.*}}encoding: [0x93,0xda,0x08,0x04]
# ENC: B.IOT t#1, u#2, mask=1111{{.*}}encoding: [0x13,0x2e,0x00,0x44]
# ENC: B.IOT t#1.reuse, u#2.reuse, mask=1111, last, ->m<1KB>{{.*}}encoding: [0x13,0x4f,0x0a,0x44]
# ENC: B.IOT mask=1100, last, ->n<8KB>{{.*}}encoding: [0x93,0xeb,0x0b,0x00]

# DIS: BSTART.VEC{{[[:space:]]+}}TADD, FP16
# DIS: BSTART.SFU{{[[:space:]]+}}TEXP, FP16
# DIS: BSTART.VEC{{[[:space:]]+}}TFMA, FP16
# DIS: B.IOR{{[[:space:]]+}}[zero, sp, a0], ->x3
# DIS: B.IOR{{[[:space:]]+}}[a0, a1, a2], ->a3, ExecMaskPresent
# DIS: B.DATR{{[[:space:]]+}}NORM, FP16, Null, cmode0, rmode0, predinv, zero
# DIS: B.IOS{{[[:space:]]+}}S1, mask=1000
# DIS: B.IOS{{[[:space:]]+}}S2.reuse, mask=0001
# DIS: B.IOS{{[[:space:]]+}}mask=1111, ->S63<256KB>
# DIS: B.IOT{{[[:space:]]+}}t#1, mask=1100, last, ->u<128B>
# DIS: B.IOT{{[[:space:]]+}}t#1, u#2, mask=1111
# DIS: B.IOT{{[[:space:]]+}}t#1.reuse, u#2.reuse, mask=1111, last,{{[[:space:]]+}}->m<1KB>
# DIS: B.IOT{{[[:space:]]+}}mask=1100, last, ->n<8KB>
