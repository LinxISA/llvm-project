# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

	B.IOD t#1, last, ->u<128B>
	BSTART.PAR 0
	BSTART.TMA 0, FP16
	BSTART.ACCCVT FP32
	BSTART.TLSU TLOAD, FP16
	BSTART.VEC TEXP, FP16
	BSTART.VEC TDIV, FP16
	BSTART.VEC TREM, FP16
	BSTART.VEC TDIVS, FP16
	BSTART.VEC TREMS, FP16
	BSTART.VEC TEXPDIF, FP16
	BSTART.SFU TADD, FP16
	BSTART.SFU TLEA, FP16
	BSTART.VEC TALLOC, FP16
	BSTART.SFU TCONCAT, FP16
	BSTART.SFU TEXTRACT, FP16
	BSTART.SFU TINSERT, FP16
	BSTART.SFU TIMG2COL, FP16
	BSTART.SFU THISTOGRAM, FP16
	BSTART.SFU TQUANT, FP16
	BSTART.SFU TDEQUANT, FP16
	BSTART.SFU TSORT, FP16
	BSTART.SFU TMRGSORT, FP16
	BSTART.TEPL 3, 0, FP16
	BSTART.TEPL 3, 2, FP16
	BSTART.TEPL 3, 3, FP16
	BSTART.TEPL 3, 4, FP16
	BSTART.TEPL 3, 8, FP16
	BSTART.TEPL 3, 10, FP16
	BSTART.TEPL 3, 11, FP16
	BSTART.TEPL 3, 12, FP16
	BSTART.TEPL 3, 13, FP16
	B.IOT t#1, mask=001
	B.IOT t#1, mask=10000
	B.IOT t#1, mask=0011
	B.IOT t#1, mask=0001, ->u<0>
	B.IOT t#1, mask=1111, ->u<128KB>
	B.IOS S64, mask=0001
	B.IOS S1, mask=0001, ->S2<128B>
	B.IOR [r24]

# CHECK: error: unrecognized instruction 'b.iod'
# CHECK: error: unrecognized instruction 'bstart.par'
# CHECK: error: unrecognized instruction 'bstart.tma'
# CHECK: error: unrecognized instruction 'bstart.acccvt'
# CHECK: error: unrecognized instruction 'bstart.tlsu'
# CHECK: error: TEXP is classified as SFU, not VEC
# CHECK: error: TDIV is classified as SFU, not VEC
# CHECK: error: TREM is classified as SFU, not VEC
# CHECK: error: TDIVS is classified as SFU, not VEC
# CHECK: error: TREMS is classified as SFU, not VEC
# CHECK: error: TEXPDIF is classified as SFU, not VEC
# CHECK: error: TADD is classified as VEC, not SFU
# CHECK: error: TLEA is classified as VEC, not SFU
# CHECK: error: unknown PTO 0.58 tile operation 'TALLOC'
# CHECK: error: unknown PTO 0.58 tile operation 'TCONCAT'
# CHECK: error: unknown PTO 0.58 tile operation 'TEXTRACT'
# CHECK: error: unknown PTO 0.58 tile operation 'TINSERT'
# CHECK: error: unknown PTO 0.58 tile operation 'TIMG2COL'
# CHECK: error: unknown PTO 0.58 tile operation 'THISTOGRAM'
# CHECK: error: unknown PTO 0.58 tile operation 'TQUANT'
# CHECK: error: unknown PTO 0.58 tile operation 'TDEQUANT'
# CHECK: error: unknown PTO 0.58 tile operation 'TSORT'
# CHECK: error: unknown PTO 0.58 tile operation 'TMRGSORT'
# CHECK-COUNT-9: error: BSTART.TEPL Mode/Function is reserved in PTO ISA 0.58
# CHECK: error: PE mask must contain exactly four binary digits
# CHECK: error: PE mask must contain exactly four binary digits
# CHECK: error: PE mask must be one of 0000, 1000, 0100, 0010, 0001, 1100, 1110, or 1111
# CHECK-COUNT-2: error: tile size must be in strict range 128B..64KB
# CHECK: error: B.IOS Shared register must be S0..S63
# CHECK: error: B.IOS must be either a Shared source or a Shared destination
# CHECK: error: B.IOR register must be one of the 24 absolute GPRs
