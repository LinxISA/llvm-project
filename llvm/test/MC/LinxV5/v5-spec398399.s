# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | llvm-mc -triple=linx64v5 -filetype=obj -o %t.roundtrip
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | sed -n 's/^[[:space:]]*[[:xdigit:]]\+:[[:space:]]*//p' | llvm-mc -triple=linx64v5 -filetype=obj -o %t.dis-roundtrip
# RUN: cmp %t %t.dis-roundtrip

BSTART.TLSU TSTORE, U32
B.DATR NORM.canon, DTYPE_NONE, Zero, 0
B.DATR NORM.canon, DTYPE_NONE, Zero, 1
B.DATR NORM.canon, DTYPE_NONE, Zero, 2
B.DATR NORM.canon, DTYPE_NONE, Zero, 3
B.DATR NORM.canon, DTYPE_NONE, Zero, 4
B.DATR NORM.canon, DTYPE_NONE, Zero, 5
B.DATR NORM.canon, DTYPE_NONE, Zero, 6
B.DATR NORM.canon, DTYPE_NONE, Zero, 7
BSTART.TLSU TMOV, 15
B.DATR M322ND, DTYPE_NONE, Zero
BSTART.TLSU TMOV, FP32
B.DATR M162ND, DTYPE_NONE, Zero

BSTART.TLSU 1, U32
B.DIM zero, 4, ->lb0
B.DATR NORM.canon, DTYPE_NONE, Zero, byte0, 6, RNONE, nosat, 1, 0
B.DATR NORM.canon, DTYPE_NONE, Zero, byte0, 7, RNONE, nosat, 0, 1
BSTART.TSTORE U32
B.DATR NORM.canon, DTYPE_NONE, Zero, 7
BSTART.TLSU 2, FP32
B.DIM zero, 4, ->lb0
B.DATR 24, DTYPE_NONE, Zero
BSTART.TMOV FP32
B.DATR 25, DTYPE_NONE, Zero
BSTART.TLSU TMOV, FP32
B.DATR M322ND, DTYPE_NONE, Zero

# CHECK: 03f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero
# CHECK: 23f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, NE
# CHECK: 43f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, LT
# CHECK: 63f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, GT
# CHECK: 83f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, LE
# CHECK: a3f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, GE
# CHECK: c3f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, 6
# CHECK: e3f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, 7
# CHECK: BSTART.TLSU TMOV,{{.*}}e6m2
# CHECK: B.DATR{{.*}}M322ND.normal, Zero
# CHECK: BSTART.TLSU TMOV,{{.*}}FP32
# CHECK: B.DATR{{.*}}M162ND.normal, Zero
# CHECK: c3f05023{{.*}}B.DATR{{.*}}NORM.canon, DTYPE_NONE, Zero, byte0, 6, RNONE, nosat, 1, 0
# CHECK: e3f03023{{.*}}B.DATR{{.*}}NORM.canon, DTYPE_NONE, Zero, byte0, 7, RNONE, nosat, 0, 1
# CHECK: e3f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, 7
# CHECK: B.DATR{{.*}}M322ND.normal, Zero
# CHECK: B.DATR{{.*}}M162ND.normal, Zero
# CHECK: B.DATR{{.*}}M322ND.normal, Zero
