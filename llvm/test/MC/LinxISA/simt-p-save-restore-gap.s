# RUN: split-file %s %t
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/scalar_add_p.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=SCALAR
# RUN: not llvm-mc -triple=linx64 -filetype=obj %t/legacy_l_add_p.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=LEGACYL

# SCALAR: error: register operand does not fit field width
# SCALAR: add p, zero, ->t
# SCALAR: error: destination register does not fit field width
# SCALAR: add t, zero, ->p

# LEGACYL: error: legacy 'L.*' mnemonics are not allowed in canonical PTO 0.58
# LEGACYL: l.add p, zero, ->t

#--- scalar_add_p.s
	.text
scalar_add_p:
	add p, zero, ->t
	add t, zero, ->p

#--- legacy_l_add_p.s
	.text
legacy_l_add_p:
	l.add p, zero, ->t
