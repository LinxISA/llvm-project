# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=OK
# RUN: not llvm-mc -triple=linx64 -filetype=obj %s --defsym=ERR=1 -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR
# RUN: echo "0x0e 0x00 0x09 0xf0 0x20 0x06" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=RESERVED

	.text
	HL.PRF.L1 [r1, r2]
	HL.PRF.L2 [r3, r4.sw]
	HL.PRF.A.L3 [r5, r6.uw], ->r7

# ENC: hl.prf.l1 [sp, a0]{{.*}}encoding: [0x0e,0x00,0x09,0xf0,0x20,0x00]
# ENC: hl.prf.l2 [a1, a2.sw]{{.*}}encoding: [0x0e,0x08,0x09,0xf0,0x41,0x02]
# ENC: hl.prf.a.l3 [a3, a4.uw], ->a5{{.*}}encoding: [0x1e,0x10,0x89,0xf3,0x62,0x04]
# OK: hl.prf.l1{{[[:space:]]+}}[sp, a0]
# OK: hl.prf.l2{{[[:space:]]+}}[a1, a2.sw]
# OK: hl.prf.a.l3{{[[:space:]]+}}[a3, a4.uw], ->a5

.ifdef ERR
	HL.PRF.L1 [r1, r2.neg]
	HL.PRF.A.L2 [r3, r4.not], ->r5
.endif

# ERR-COUNT-2: error: HL.PRF register offset does not allow .neg or .not
# RESERVED: warning: invalid instruction encoding
