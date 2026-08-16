# RUN: not llvm-mc -triple=linx64 -defsym ASM_NEG=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ASM-NEG
# RUN: not llvm-mc -triple=linx32 -defsym ASM_NEG=1 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ASM-NEG
# RUN: llvm-mc -triple=linx64 -defsym DISASM_NEG=1 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=DISASM-NEG
# RUN: llvm-mc -triple=linx32 -defsym DISASM_NEG=1 -filetype=obj %s -o - | llvm-objdump -d --triple=linx32 - | FileCheck %s --check-prefix=DISASM-NEG

.ifdef ASM_NEG
	C.BSTART.STD ICALL
.endif

# ASM-NEG: error: C.BSTART.STD branch kind must be FALL, IND, or RET

.ifdef DISASM_NEG
	.text
	.globl cbstart_std_invalid_brtype
	.type cbstart_std_invalid_brtype,@function
cbstart_std_invalid_brtype:
	# C.BSTART.STD with the forbidden BrType=6 selector.
	.short 0x3000
	.size cbstart_std_invalid_brtype, .-cbstart_std_invalid_brtype
.endif

# DISASM-NEG-LABEL: <cbstart_std_invalid_brtype>:
# DISASM-NEG: <unknown>
# DISASM-NEG-NOT: C.BSTART.STD
