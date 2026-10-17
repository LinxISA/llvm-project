# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

	SB.ADD.AQ [a0], a1
	SH.SUB.F [a0], a1
	SW.ADD.RD [a0], a1
	SD.ADD.AQRL [a0], a1

# CHECK: error: mnemonic suffix '.aq' is not supported for this instruction
# CHECK: error: mnemonic suffix '.f' is not supported for this instruction
# CHECK: error: mnemonic suffix '.rd' is not supported for this instruction
# CHECK: error: mnemonic suffix '.aq' is not supported for this instruction
