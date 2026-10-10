# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

# ENC: B.DATR CUBE_M16, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 0
# ENC-SAME: encoding: [0xa3,0x1f,0xf0,0x01]
B.DATR CUBE_M16, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 0

# ENC: B.DATR CUBE_M32, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 1, 1
# ENC-SAME: encoding: [0xa3,0x7e,0xf0,0x01]
B.DATR CUBE_M32, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 1, 1

# ENC: B.IOR [a4,zero,zero], ExecMaskPresent
# ENC-SAME: encoding: [0x13,0x00,0x03,0x04]
B.IOR [a4,zero,zero], ExecMaskPresent

# ENC: B.IOR [a4,a5,zero], ExecMaskPresent
# ENC-SAME: encoding: [0x13,0x00,0x73,0x04]
B.IOR [a4,a5,zero], ExecMaskPresent

# DIS: B.DATR CUBE_M16.normal, Zero
# DIS: B.DATR CUBE_M32, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 1, 1
# DIS: B.IOR [a4,zero,zero], ExecMaskPresent
# DIS: B.IOR [a4,a5,zero], ExecMaskPresent
