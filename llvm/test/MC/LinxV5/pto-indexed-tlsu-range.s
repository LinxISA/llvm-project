# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s
# RUN: %python %S/../../../utils/linxv5/roundtrip_tile_macro_forms.py llvm-mc llvm-objdump %s
.text
# CHECK: MGATHER{{ +}}<{{.*}}Encoding=E4
MGATHER <Encoding=E4, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, ->T<128B>
# CHECK: MGATHER{{ +}}<{{.*}}Encoding=E5
MGATHER <Encoding=E5, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, ->T<128B>, [a3]
# CHECK: MGATHER{{ +}}<{{.*}}Encoding=E6
MGATHER <Encoding=E6, Col=1, U8, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, ->T<128B>, [a3, zero]
# CHECK: MGATHER_ADD{{ +}}<{{.*}}Encoding=E2
MGATHER_ADD <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_ADD{{ +}}<{{.*}}Encoding=E3
MGATHER_ADD <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_AND{{ +}}<{{.*}}Encoding=E2
MGATHER_AND <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_AND{{ +}}<{{.*}}Encoding=E3
MGATHER_AND <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_CAS{{ +}}<{{.*}}Encoding=E2
MGATHER_CAS <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, T#1, ->T<128B>
# CHECK: MGATHER_CAS{{ +}}<{{.*}}Encoding=E3
MGATHER_CAS <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_DEC{{ +}}<{{.*}}Encoding=E2
MGATHER_DEC <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_DEC{{ +}}<{{.*}}Encoding=E3
MGATHER_DEC <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_EXCH{{ +}}<{{.*}}Encoding=E2
MGATHER_EXCH <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_EXCH{{ +}}<{{.*}}Encoding=E3
MGATHER_EXCH <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_INC{{ +}}<{{.*}}Encoding=E2
MGATHER_INC <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_INC{{ +}}<{{.*}}Encoding=E3
MGATHER_INC <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_MASK{{ +}}<{{.*}}Encoding=E2
MGATHER_MASK <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, U#1, ->T<128B>
# CHECK: MGATHER_MASK{{ +}}<{{.*}}Encoding=E3
MGATHER_MASK <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, U#1, ->T<128B>, [a3]
# CHECK: MGATHER_MASK{{ +}}<{{.*}}Encoding=E4
MGATHER_MASK <Encoding=E4, Col=1, U8, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, U#1, ->T<128B>, [a3, zero]
# CHECK: MGATHER_MAX{{ +}}<{{.*}}Encoding=E2
MGATHER_MAX <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_MAX{{ +}}<{{.*}}Encoding=E3
MGATHER_MAX <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_MIN{{ +}}<{{.*}}Encoding=E2
MGATHER_MIN <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_MIN{{ +}}<{{.*}}Encoding=E3
MGATHER_MIN <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_OR{{ +}}<{{.*}}Encoding=E2
MGATHER_OR <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_OR{{ +}}<{{.*}}Encoding=E3
MGATHER_OR <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MGATHER_XOR{{ +}}<{{.*}}Encoding=E2
MGATHER_XOR <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>
# CHECK: MGATHER_XOR{{ +}}<{{.*}}Encoding=E3
MGATHER_XOR <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, [a3]
# CHECK: MSCATTER{{ +}}<{{.*}}Encoding=E2
MSCATTER <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER{{ +}}<{{.*}}Encoding=E3
MSCATTER <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]
# CHECK: MSCATTER{{ +}}<{{.*}}Encoding=E4
MSCATTER <Encoding=E4, Col=1, U8, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3, zero]
# CHECK: MSCATTER_ADD{{ +}}<{{.*}}Encoding=E2
MSCATTER_ADD <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER_ADD{{ +}}<{{.*}}Encoding=E3
MSCATTER_ADD <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]
# CHECK: MSCATTER_AND{{ +}}<{{.*}}Encoding=E2
MSCATTER_AND <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER_AND{{ +}}<{{.*}}Encoding=E3
MSCATTER_AND <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]
# CHECK: MSCATTER_DEC{{ +}}<{{.*}}Encoding=E2
MSCATTER_DEC <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER_DEC{{ +}}<{{.*}}Encoding=E3
MSCATTER_DEC <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]
# CHECK: MSCATTER_INC{{ +}}<{{.*}}Encoding=E2
MSCATTER_INC <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER_INC{{ +}}<{{.*}}Encoding=E3
MSCATTER_INC <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]
# CHECK: MSCATTER_MASK{{ +}}<{{.*}}Encoding=E2
MSCATTER_MASK <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
# CHECK: MSCATTER_MASK{{ +}}<{{.*}}Encoding=E3
MSCATTER_MASK <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, U#1, [a3]
# CHECK: MSCATTER_MASK{{ +}}<{{.*}}Encoding=E4
MSCATTER_MASK <Encoding=E4, Col=1, U8, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, U#1, [a3, zero]
# CHECK: MSCATTER_MAX{{ +}}<{{.*}}Encoding=E2
MSCATTER_MAX <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER_MAX{{ +}}<{{.*}}Encoding=E3
MSCATTER_MAX <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]
# CHECK: MSCATTER_MIN{{ +}}<{{.*}}Encoding=E2
MSCATTER_MIN <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER_MIN{{ +}}<{{.*}}Encoding=E3
MSCATTER_MIN <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]
# CHECK: MSCATTER_OR{{ +}}<{{.*}}Encoding=E2
MSCATTER_OR <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER_OR{{ +}}<{{.*}}Encoding=E3
MSCATTER_OR <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]
# CHECK: MSCATTER_POPC{{ +}}<{{.*}}Encoding=E1
MSCATTER_POPC <Encoding=E1, Col=1, U32>, [base=a0, min=zero, max=a2], T#1
# CHECK: MSCATTER_POPC{{ +}}<{{.*}}Encoding=E2
MSCATTER_POPC <Encoding=E2, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, [a3]
# CHECK: MSCATTER_XOR{{ +}}<{{.*}}Encoding=E2
MSCATTER_XOR <Encoding=E2, Col=1, U32>, [base=a0, min=zero, max=a2], T#1, T#1
# CHECK: MSCATTER_XOR{{ +}}<{{.*}}Encoding=E3
MSCATTER_XOR <Encoding=E3, Col=1, U32, CUBE_M32>, [base=a0, min=zero, max=a2], T#1, T#1, [a3]

# Explicit zero selectors in the final mask still represent mask words.
# CHECK: MGATHER{{ +}}<{{.*}}Encoding=E5
MGATHER <Encoding=E5, Col=1, U32, CUBE_M32>, [zero, a1, zero], T#1, ->T<128B>, [zero]
# CHECK: MGATHER{{ +}}<{{.*}}Encoding=E6
MGATHER <Encoding=E6, Col=1, U8, CUBE_M16>, [a0, a1, zero], T#1, ->T<128B>, [zero, zero]

# Mask coverage boundary values; physical Col may exceed the active ValidCol.
MGATHER <Encoding=E5, Col=8, ValidRow=32, ValidCol=2, U32, CUBE_M32>, [a0, a1, a2], T#1, ->T<1KB>, [a3]
MGATHER <Encoding=E5, Col=2, ValidRow=16, U32, CUBE_M16>, [a0, a1, a2], T#1, ->T<128B>, [a3]
MGATHER <Encoding=E5, Col=4, ValidRow=16, U16, CUBE_M16>, [a0, a1, a2], T#1, ->T<128B>, [a3]
MGATHER <Encoding=E6, Col=4, ValidRow=32, U8, CUBE_M32>, [a0, a1, a2], T#1, ->T<128B>, [a3, a4]
MGATHER <Encoding=E6, Col=8, ValidRow=16, U8, CUBE_M16>, [a0, a1, a2], T#1, ->T<128B>, [a3, a4]
# Dynamic dimensions cannot be resolved statically; the ISA checks their values.
MGATHER <Encoding=E5, Col=8, ValidRow=a5, ValidCol=a4, U32, CUBE_M32>, [a0, a1, a2], T#1, ->T<1KB>, [a3]
