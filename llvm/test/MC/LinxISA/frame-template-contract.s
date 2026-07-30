# RUN: llvm-mc -triple=linx64 -defsym VALID=1 -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d --triple=linx64 - \
# RUN:   | FileCheck %s --check-prefix=VALID
# RUN: llvm-mc -triple=linx32 -defsym VALID=1 -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d --triple=linx32 - \
# RUN:   | FileCheck %s --check-prefix=VALID
# RUN: not llvm-mc -triple=linx64 -defsym ASM_NEG=1 %s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s --check-prefix=ASM-NEG
# RUN: not llvm-mc -triple=linx32 -defsym ASM_NEG=1 %s -o /dev/null 2>&1 \
# RUN:   | FileCheck %s --check-prefix=ASM-NEG
# RUN: llvm-mc -triple=linx64 -defsym DISASM_NEG=1 -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d --triple=linx64 - \
# RUN:   | FileCheck %s --check-prefix=DISASM-NEG
# RUN: llvm-mc -triple=linx32 -defsym DISASM_NEG=1 -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d --triple=linx32 - \
# RUN:   | FileCheck %s --check-prefix=DISASM-NEG

# R975 contract bound by this test:
# - Register ranges are the inclusive modulo-22 ring R2..R23.
# - Encoded F is stack bytes and must be 8-byte aligned with F >= 8 * N.
# - N is the inclusive modulo-22 register count.
# - FRET.STK is additionally constrained to Begin == R10.

.ifdef VALID
	.text
	.globl	frame_template_contract
	.type	frame_template_contract,@function
frame_template_contract:
	FENTRY [r10 ~ r10], sp!, 8
	FENTRY [r23 ~ r2], sp!, 16
	FENTRY [r2 ~ r23], sp!, 176
	FEXIT [r10 ~ r10], sp!, 8
	FEXIT [r23 ~ r2], sp!, 16
	FEXIT [r2 ~ r23], sp!, 176
	FRET.RA [r10 ~ r10], sp!, 8
	FRET.RA [r23 ~ r2], sp!, 16
	FRET.RA [r2 ~ r23], sp!, 176
	FRET.STK [r10 ~ r10], sp!, 8
	FRET.STK [r10 ~ r2], sp!, 120
	FRET.STK [r10 ~ r9], sp!, 176
	.size	frame_template_contract, .-frame_template_contract
.endif

# VALID-LABEL: <frame_template_contract>:
# VALID: 41 00 a5 02  FENTRY{{[[:space:]]+}}[ra ~ ra], sp!, 8
# VALID: 41 80 2b 04  FENTRY{{[[:space:]]+}}[x3 ~ a0], sp!, 16
# VALID: 41 00 71 2d  FENTRY{{[[:space:]]+}}[a0 ~ x3], sp!, 176
# VALID: 41 10 a5 02  FEXIT{{[[:space:]]+}}[ra ~ ra], sp!, 8
# VALID: 41 90 2b 04  FEXIT{{[[:space:]]+}}[x3 ~ a0], sp!, 16
# VALID: 41 10 71 2d  FEXIT{{[[:space:]]+}}[a0 ~ x3], sp!, 176
# VALID: 41 20 a5 02  FRET.RA{{[[:space:]]+}}[ra ~ ra], sp!, 8
# VALID: 41 a0 2b 04  FRET.RA{{[[:space:]]+}}[x3 ~ a0], sp!, 16
# VALID: 41 20 71 2d  FRET.RA{{[[:space:]]+}}[a0 ~ x3], sp!, 176
# VALID: 41 30 a5 02  FRET.STK{{[[:space:]]+}}[ra ~ ra], sp!, 8
# VALID: 41 30 25 1e  FRET.STK{{[[:space:]]+}}[ra ~ a0], sp!, 120
# VALID: 41 30 95 2c  FRET.STK{{[[:space:]]+}}[ra ~ a7], sp!, 176

.ifdef ASM_NEG
	.text
	FENTRY [r1 ~ r10], sp!, 8
	FEXIT [r10 ~ r24], sp!, 8
	FRET.RA [r10 ~ r10], sp!, 9
	FENTRY [r10 ~ r10], sp!, 0
	FEXIT [r23 ~ r2], sp!, 8
	FRET.RA [r2 ~ r23], sp!, 168
	FENTRY [r10 ~ r10], sp!, 9223372036854775800
	FRET.STK [r2 ~ r23], sp!, 176
.endif

# ASM-NEG: error: frame template register range endpoints must be in R2..R23
# ASM-NEG: error: frame template register range endpoints must be in R2..R23
# ASM-NEG: error: frame template F must be 8-byte aligned
# ASM-NEG: error: frame template F is too small for 1 saved registers
# ASM-NEG: error: frame template F is too small for 2 saved registers
# ASM-NEG: error: frame template F is too small for 22 saved registers
# ASM-NEG: error: frame template F does not fit encoded field
# ASM-NEG: error: FRET.STK frame template begin register must be R10

.ifdef DISASM_NEG
	.text
	.globl	frame_template_invalid_raw
	.type	frame_template_invalid_raw,@function
frame_template_invalid_raw:
	# FENTRY [r1 ~ r10], sp!, 8: Begin endpoint outside R2..R23.
	.long 0x02a08041
	# FEXIT [r10 ~ r24], sp!, 8: End endpoint outside R2..R23.
	.long 0x03851041
	# FRET.RA [r10 ~ r10], sp!, 0: F < 8 * 1.
	.long 0x00a52041
	# FENTRY [r23 ~ r2], sp!, 8: F < 8 * 2.
	.long 0x022b8041
	# FENTRY [r2 ~ r23], sp!, 168: F < 8 * 22.
	.long 0x2b710041
	# FRET.STK [r2 ~ r23], sp!, 176: Begin is not fixed R10.
	.long 0x2d713041
	.size	frame_template_invalid_raw, .-frame_template_invalid_raw
.endif

# DISASM-NEG-LABEL: <frame_template_invalid_raw>:
# DISASM-NEG-COUNT-6: <unknown>
# DISASM-NEG-NOT: FENTRY
# DISASM-NEG-NOT: FEXIT
# DISASM-NEG-NOT: FRET.RA
# DISASM-NEG-NOT: FRET.STK
