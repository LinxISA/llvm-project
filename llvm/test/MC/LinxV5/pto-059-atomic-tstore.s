# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s
# RUN: %python %S/../../../utils/linxv5/roundtrip_tile_macro_forms.py llvm-mc llvm-objdump %s
.text
# CHECK: TSTORE.ATOMIC_ADD{{ +}}<Encoding=E3
TSTORE.ATOMIC_ADD <Encoding=E3, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_ADD{{ +}}<Encoding=E4
TSTORE.ATOMIC_ADD <Encoding=E4, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK: TSTORE.ATOMIC_ADD{{ +}}<Encoding=E5
TSTORE.ATOMIC_ADD <Encoding=E5, Row=32, Col=1, ValidRow=32, ValidCol=1, U32, M322ND>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_MIN{{ +}}<Encoding=E6
TSTORE.ATOMIC_MIN <Encoding=E6, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_MIN{{ +}}<Encoding=E7
TSTORE.ATOMIC_MIN <Encoding=E7, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK: TSTORE.ATOMIC_MIN{{ +}}<Encoding=E8
TSTORE.ATOMIC_MIN <Encoding=E8, Row=32, Col=1, ValidRow=32, ValidCol=1, U32, M322ND>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_MAX{{ +}}<Encoding=E9
TSTORE.ATOMIC_MAX <Encoding=E9, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_MAX{{ +}}<Encoding=E10
TSTORE.ATOMIC_MAX <Encoding=E10, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK: TSTORE.ATOMIC_MAX{{ +}}<Encoding=E11
TSTORE.ATOMIC_MAX <Encoding=E11, Row=32, Col=1, ValidRow=32, ValidCol=1, U32, M322ND>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_AND{{ +}}<Encoding=E12
TSTORE.ATOMIC_AND <Encoding=E12, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_AND{{ +}}<Encoding=E13
TSTORE.ATOMIC_AND <Encoding=E13, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK: TSTORE.ATOMIC_AND{{ +}}<Encoding=E14
TSTORE.ATOMIC_AND <Encoding=E14, Row=32, Col=1, ValidRow=32, ValidCol=1, U32, M322ND>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_OR{{ +}}<Encoding=E15
TSTORE.ATOMIC_OR <Encoding=E15, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_OR{{ +}}<Encoding=E16
TSTORE.ATOMIC_OR <Encoding=E16, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK: TSTORE.ATOMIC_OR{{ +}}<Encoding=E17
TSTORE.ATOMIC_OR <Encoding=E17, Row=32, Col=1, ValidRow=32, ValidCol=1, U32, M322ND>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_XOR{{ +}}<Encoding=E18
TSTORE.ATOMIC_XOR <Encoding=E18, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_XOR{{ +}}<Encoding=E19
TSTORE.ATOMIC_XOR <Encoding=E19, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK: TSTORE.ATOMIC_XOR{{ +}}<Encoding=E20
TSTORE.ATOMIC_XOR <Encoding=E20, Row=32, Col=1, ValidRow=32, ValidCol=1, U32, M322ND>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_INC{{ +}}<Encoding=E21
TSTORE.ATOMIC_INC <Encoding=E21, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_INC{{ +}}<Encoding=E22
TSTORE.ATOMIC_INC <Encoding=E22, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK: TSTORE.ATOMIC_INC{{ +}}<Encoding=E23
TSTORE.ATOMIC_INC <Encoding=E23, Row=32, Col=1, ValidRow=32, ValidCol=1, U32, M322ND>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_DEC{{ +}}<Encoding=E24
TSTORE.ATOMIC_DEC <Encoding=E24, Col=4, ValidRow=2, ValidCol=4, U32>, T#1, [a0, a1]
# CHECK: TSTORE.ATOMIC_DEC{{ +}}<Encoding=E25
TSTORE.ATOMIC_DEC <Encoding=E25, Col=4, ValidRow=2, ValidCol=4, U32>, S1, [a0, a1]
# CHECK: TSTORE.ATOMIC_DEC{{ +}}<Encoding=E26
TSTORE.ATOMIC_DEC <Encoding=E26, Row=32, Col=1, ValidRow=32, ValidCol=1, U32, M322ND>, T#1, [a0, a1]
