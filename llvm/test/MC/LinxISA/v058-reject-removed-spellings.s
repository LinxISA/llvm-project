# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --implicit-check-not=warning:

	.text
removed_spellings:
	B.IOD
	B.IOTI t#1, last, ->u<1KB>
	B.IOT t#1, last, ->t<a0>
	B.IOT [t#1], last, ->u<1KB>
	B.ATTR aq
	BSTART.PAR 33, 4
	BSTART.FP CALL, removed_fp_call
	BSTART.FP ICALL
	BSTART.STD CALL, removed_std_call
	BSTART.STD ICALL

# CHECK: error:
# CHECK: B.IOD
# CHECK: error:
# CHECK: B.IOTI
# CHECK: error:
# CHECK: B.IOT
# CHECK: error:
# CHECK: bracketed B.IOT source lists are not canonical PTO 0.58
# CHECK: error:
# CHECK: B.ATTR
# CHECK: error:
# CHECK: BSTART.PAR
# CHECK: error:
# CHECK: BSTART.FP
# CHECK: error:
# CHECK: BSTART.FP
# CHECK: error:
# CHECK: BSTART.STD
# CHECK: error:
# CHECK: BSTART.STD
