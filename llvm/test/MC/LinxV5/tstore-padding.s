# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s

# All four legacy pseudo forms encode ordinary TSTORE with Zero padding,
# including the dependency source/destination variants used by codegen.
ordinary_pseudos:
TSTORE.NORM <LB0: 32, LB1: 1, LB2: 32, FP32>, T#1, [a0, a1]
TSTORE.NORM <LB0: 32, LB1: 1, LB2: 32, FP32>, T#1, [a0, a1], ->d
TSTORE.NORM <LB0: 32, LB1: 1, LB2: 32, FP32>, T#1, [a0, a1], d#1
TSTORE.NORM <LB0: 32, LB1: 1, LB2: 32, FP32>, T#1, [a0, a1], d#1, ->d
# CHECK-LABEL: <ordinary_pseudos>:
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR NORM.normal, Zero
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR NORM.normal, Zero
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR NORM.normal, Zero
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR NORM.normal, Zero

# Explicit ordinary and Shared macro layouts require the same padding.
ordinary_macros:
TSTORE <Row=1, Col=32, FP32, NORM>, T#1, [base=a0, stride=a1]
TSTORE <Row=1, Col=32, FP32, NORM>, S1, [base=a0, stride=a1]
# CHECK-LABEL: <ordinary_macros>:
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR NORM.normal, Zero
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR NORM.normal, Zero

# CUBE stores retain their conversion layout and optional padding.
cube_stores:
TSTORE.M322ND <LB0: 1, LB1: 32, LB2: 1, FP32>, T#1, [a0, a1]
TSTORE.M162ND <LB0: 1, LB1: 16, LB2: 1, FP32>, T#1, [a0, a1], d#1
TSTORE <Row=32, Col=1, FP32, M322ND>, T#1, [base=a0, stride=a1]
# CHECK-LABEL: <cube_stores>:
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR M322ND.normal, Null
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR M162ND.normal, Null
# CHECK: BSTART.TSTORE FP32
# CHECK-NEXT: B.DATR M322ND.normal, Null

ordinary_atomic:
TSTORE.ATOMIC_ADD <Encoding=E3, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
TSTORE.ATOMIC_ADD <Encoding=E4, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK-LABEL: <ordinary_atomic>:
# CHECK: BSTART.TSTORE U32
# CHECK-NEXT: B.DATR NORM.canon, Zero
# CHECK: BSTART.TSTORE U32
# CHECK-NEXT: B.DATR NORM.canon, Zero
