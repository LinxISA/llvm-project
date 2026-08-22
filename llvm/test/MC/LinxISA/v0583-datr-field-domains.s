# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

B.DATR NORM, DTYPE_NONE, Zero
B.DATR ND2M32, DTYPE_NONE, Max
B.DATR N82ND, DTYPE_NONE, Min
B.DATR NZ2ZN, DTYPE_NONE, Null
B.DATR ND2DN, DTYPE_NONE, Null
B.DATR ND2ZN, DTYPE_NONE, Null
B.DATR ND2NZ, DTYPE_NONE, Null
B.DATR DN2ND, DTYPE_NONE, Null
B.DATR DN2ZN, DTYPE_NONE, Null
B.DATR DN2NZ, DTYPE_NONE, Null
B.DATR ZN2ND, DTYPE_NONE, Null
B.DATR ZN2DN, DTYPE_NONE, Null
B.DATR ZN2NZ, DTYPE_NONE, Null
B.DATR ND2M16, DTYPE_NONE, Null
B.DATR ND2N8, DTYPE_NONE, Null
B.DATR M322ND, DTYPE_NONE, Null
B.DATR M162ND, DTYPE_NONE, Null
B.DATR NZ2ND, DTYPE_NONE, Null
B.DATR NZ2DN, DTYPE_NONE, Null

# ENC: B.DATR NORM, DTYPE_NONE, Zero{{.*}}encoding: [0x23,0x10,0xf0,0x01]
# ENC: B.DATR ND2M32, DTYPE_NONE, Max{{.*}}encoding: [0xa3,0x1a,0xf0,0x09]
# ENC: B.DATR N82ND, DTYPE_NONE, Min{{.*}}encoding: [0x23,0x1d,0xf0,0x11]
# ENC: B.DATR NZ2ZN, DTYPE_NONE, Null{{.*}}encoding: [0x23,0x1f,0xf0,0x19]

# DIS: B.DATR{{[[:space:]]+}}NORM, DTYPE_NONE, Zero
# DIS: B.DATR{{[[:space:]]+}}ND2M32, DTYPE_NONE, Max
# DIS: B.DATR{{[[:space:]]+}}N82ND, DTYPE_NONE, Min
# DIS: B.DATR{{[[:space:]]+}}NZ2ZN, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}ND2DN, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}ND2ZN, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}ND2NZ, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}DN2ND, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}DN2ZN, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}DN2NZ, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}ZN2ND, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}ZN2DN, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}ZN2NZ, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}ND2M16, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}ND2N8, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}M322ND, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}M162ND, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}NZ2ND, DTYPE_NONE, Null
# DIS: B.DATR{{[[:space:]]+}}NZ2DN, DTYPE_NONE, Null
