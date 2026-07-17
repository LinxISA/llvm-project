# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64 -show-encoding %t/valid.s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %t/valid.s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=DIS
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/invalid.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

# ENC: v.shfl.bfly{{[[:space:]]+}}vt#1, p,{{[[:space:]]+}}->vu#2{{.*}}encoding: [0xff,0x02,0x02,0x10,0x1d,0xa1,0x00,0xe0]
# ENC: v.shfl.down{{[[:space:]]+}}vt#1, ri3, p,{{[[:space:]]+}}->vu#2{{.*}}encoding: [0xff,0x02,0x12,0x10,0x1d,0x91,0x30,0xe0]
# ENC: v.shfl.idx{{[[:space:]]+}}vt#1, ri3, p,{{[[:space:]]+}}->vu#2{{.*}}encoding: [0xff,0x02,0x12,0x10,0x1d,0xb1,0x30,0xe0]
# ENC: v.shfl.up{{[[:space:]]+}}vt#1, ri3, p,{{[[:space:]]+}}->vu#2{{.*}}encoding: [0xff,0x02,0x12,0x10,0x1d,0x81,0x30,0xe0]
# ENC: v.shfli.bfly{{[[:space:]]+}}vt#1, 128,{{[[:space:]]+}}->vu#2{{.*}}encoding: [0xff,0x02,0x02,0x02,0x2d,0xe1,0x00,0x00]
# ENC: v.shfli.down{{[[:space:]]+}}vt#1, ri3, 4660,{{[[:space:]]+}}->vu#2{{.*}}encoding: [0xff,0x02,0x12,0x48,0x2d,0xd1,0x30,0x68]
# ENC: v.shfli.idx{{[[:space:]]+}}vt#1, ri3, 16383,{{[[:space:]]+}}->vu#2{{.*}}encoding: [0xff,0x02,0x12,0xfe,0x2d,0xf1,0x30,0xfe]
# ENC: v.shfli.up{{[[:space:]]+}}vt#1, ri3, 127,{{[[:space:]]+}}->vu#2{{.*}}encoding: [0xff,0x02,0x12,0x00,0x2d,0xc1,0x30,0xfe]

# DIS-LABEL: <shfl_forms>:
# DIS: v.shfl.bfly{{[[:space:]]+}}vt#1, p,{{[[:space:]]+}}->vu#2
# DIS: v.shfl.down{{[[:space:]]+}}vt#1, ri3, p,{{[[:space:]]+}}->vu#2
# DIS: v.shfl.idx{{[[:space:]]+}}vt#1, ri3, p,{{[[:space:]]+}}->vu#2
# DIS: v.shfl.up{{[[:space:]]+}}vt#1, ri3, p,{{[[:space:]]+}}->vu#2
# DIS: v.shfli.bfly{{[[:space:]]+}}vt#1, 128,{{[[:space:]]+}}->vu#2
# DIS: v.shfli.down{{[[:space:]]+}}vt#1, ri3, 4660,{{[[:space:]]+}}->vu#2
# DIS: v.shfli.idx{{[[:space:]]+}}vt#1, ri3, 16383,{{[[:space:]]+}}->vu#2
# DIS: v.shfli.up{{[[:space:]]+}}vt#1, ri3, 127,{{[[:space:]]+}}->vu#2

# ERR: error: V.SHFLI immediate must be an unsigned 14-bit value
# ERR-NEXT: v.shfli.bfly vt#1, -1, ->vu#2
# ERR: error: V.SHFLI immediate must be an unsigned 14-bit value
# ERR-NEXT: v.shfli.up vt#1, ri3, 0x4000, ->vu#2
# ERR: error: V.SHFLI immediate must be a constant
# ERR-NEXT: v.shfli.idx vt#1, ri3, shuffle_amount, ->vu#2

#--- valid.s
	.text
shfl_forms:
	v.shfl.bfly vt#1, p, ->vu#2
	v.shfl.down vt#1, ri3, p, ->vu#2
	v.shfl.idx vt#1, ri3, p, ->vu#2
	v.shfl.up vt#1, ri3, p, ->vu#2
	v.shfli.bfly vt#1, 0x80, ->vu#2
	v.shfli.down vt#1, ri3, 0x1234, ->vu#2
	v.shfli.idx vt#1, ri3, 0x3fff, ->vu#2
	v.shfli.up vt#1, ri3, 0x7f, ->vu#2

#--- invalid.s
	.text
	v.shfli.bfly vt#1, -1, ->vu#2
	v.shfli.up vt#1, ri3, 0x4000, ->vu#2
	v.shfli.idx vt#1, ri3, shuffle_amount, ->vu#2
shuffle_amount:
	.word 1
