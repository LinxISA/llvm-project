# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=DIS

	B.SUBVIEW 1, a0, 7, 5
	B.ASSEMBLE 1, 1, a1, 9, 5

# ENC: B.SUBVIEW 1, a0, 7, 5{{.*}}encoding: [0xd3,0x02,0x71,0x80]
# ENC: B.ASSEMBLE 1, 1, a1, 9, 5{{.*}}encoding: [0xd3,0x9a,0x91,0x80]

# DIS: B.SUBVIEW{{[[:space:]]+}}1, a0, 7, 5
# DIS: B.ASSEMBLE{{[[:space:]]+}}1, 1, a1, 9, 5
