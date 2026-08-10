# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

	B.IOD t#1, last, ->u<128B>
	BSTART.PAR 0
	BSTART.TMA 0, FP16
	BSTART.ACCCVT FP32
	BSTART.TLSU TLOAD, FP16
	BSTART.VEC TEXP, FP16
	BSTART.SFU TADD, FP16
	BSTART.VEC TALLOC, FP16
	B.IOT t#1.reuse, mask=0001
	B.IOT t#1, mask=001
	B.IOT t#1, mask=10000
	B.IOT t#1, mask=0001, ->u<0>
	B.IOS S256, mask=0001
	B.IOS S1, mask=0001, ->S2<128B>
	B.IOR [r24]

# CHECK: error: unrecognized instruction 'b.iod'
# CHECK: error: unrecognized instruction 'bstart.par'
# CHECK: error: unrecognized instruction 'bstart.tma'
# CHECK: error: unrecognized instruction 'bstart.acccvt'
# CHECK: error: unrecognized instruction 'bstart.tlsu'
# CHECK: error: TEXP is classified as SFU, not VEC
# CHECK: error: TADD is classified as VEC, not SFU
# CHECK: error: unknown PTO 0.58 tile operation 'TALLOC'
# CHECK: error: B.IOT reuse suffix is not part of PTO ISA 0.58
# CHECK: error: PE mask must contain exactly four binary digits
# CHECK: error: PE mask must contain exactly four binary digits
# CHECK: error: tile size must be in strict range 128B..8KB
# CHECK: error: B.IOS Shared register must be S0..S255
# CHECK: error: B.IOS must be either a Shared source or a Shared destination
# CHECK: error: B.IOR register must be one of the 24 absolute GPRs
