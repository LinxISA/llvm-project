# RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s --implicit-check-not=error:

# N8 is not supported in either TMOV direction, named or numeric.
BSTART.TMOV DTYPE_NONE
B.DATR ND2N8, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TMOV DTYPE_NONE
B.DATR N82ND, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TLSU 2, DTYPE_NONE
B.DATR 23, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TLSU TMOV, DTYPE_NONE
B.DATR 26, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:

# Direction switches must not inherit TMOV's broader selector set.
BSTART.TMOV DTYPE_NONE
BSTART.TSTORE U8
B.DATR ND2M32, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TMOV DTYPE_NONE
BSTART.TLSU 1, U8
B.DATR 22, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TMOV DTYPE_NONE
BSTART.TLOAD U8
B.DATR M322ND, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TMOV DTYPE_NONE
BSTART.TLSU 0, U8
B.DATR 25, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:

# Block termination and an unrelated block reset transport context.
BSTART.TMOV DTYPE_NONE
BSTOP
B.DATR ND2M32, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TMOV DTYPE_NONE
BSTOP
B.DATR 22, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TMOV DTYPE_NONE
BSTART.TEPL TADD, U8
B.DATR ND2M16, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
BSTART.TMOV DTYPE_NONE
BSTART.TEPL TADD, U8
B.DATR 21, DTYPE_NONE, Zero
# CHECK: :[[@LINE-1]]:{{[0-9]+}}: error:
