# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s
# RUN: echo '0x09 0x22 0x31 0x06' | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=RAW

	lw [a0, a1.neg], ->a2
	lw [a0, a1.not], ->a2
	csel a0, a1, a2.sw, ->a3
	csel a0, a1, a2.uw, ->a3
	csel a0, a1, a2.not, ->a3
	cmp.eq a0, a1.neg, ->a2
	cmp.eq a0, a1.not, ->a2
	cmp.and a0, a1.neg, ->a2

# CHECK-COUNT-2: error: address register offset does not allow .neg or .not
# CHECK-COUNT-3: error: CSEL SrcR allows only the .neg modifier
# CHECK-COUNT-2: error: comparison SrcR does not allow .neg or .not
# CHECK: error: logical comparison SrcR uses .not, not .neg
# RAW: warning: invalid instruction encoding
