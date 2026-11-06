# RUN: not llvm-mc -triple=linx64v5 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s
# Range form cannot silently encode absence.
# CHECK: error:
MGATHER <Encoding=E4, Col=1, U32>, [a0, zero, zero], T#1, ->T<128B>
# Reserved selectors cannot carry bounds.
# CHECK: error:
MGATHER <Encoding=E4, Col=1, U32>, [a0, tos1, a2], T#1, ->T<128B>
# GPR masks require a legal CUBE layout.
# CHECK: error:
MGATHER <Encoding=E5, Col=1, U32>, [a0, a1, a2], T#1, ->T<128B>, [a3]
# One word cannot mask an eight-bit transfer.
# CHECK: error:
MGATHER <Encoding=E5, Col=1, U8, CUBE_M32>, [a0, a1, a2], T#1, ->T<128B>, [a3]
# Two words cannot mask a 32-bit transfer.
# CHECK: error:
MGATHER <Encoding=E6, Col=1, U32, CUBE_M32>, [a0, a1, a2], T#1, ->T<128B>, [a3, a4]
# Unsupported eight-bit predicate type.
# CHECK: error:
MGATHER <Encoding=E6, Col=1, HiF8, CUBE_M32>, [a0, a1, a2], T#1, ->T<128B>, [a3, a4]

# A single GPR mask covers at most 32 rows and two U32 columns in M32.
# CHECK: error:
MGATHER <Encoding=E5, Col=1, ValidRow=33, U32, CUBE_M32>, [a0, a1, a2], T#1, ->T<256B>, [a3]
# CHECK: error:
MGATHER <Encoding=E5, Col=3, ValidRow=32, U32, CUBE_M32>, [a0, a1, a2], T#1, ->T<512B>, [a3]
# M16 has 16 row bits and two U32 fields, or four U16 fields.
# CHECK: error:
MGATHER <Encoding=E5, Col=1, ValidRow=17, U16, CUBE_M16>, [a0, a1, a2], T#1, ->T<128B>, [a3]
# CHECK: error:
MGATHER <Encoding=E5, Col=3, U32, CUBE_M16>, [a0, a1, a2], T#1, ->T<128B>, [a3]
# CHECK: error:
MGATHER <Encoding=E5, Col=5, U16, CUBE_M16>, [a0, a1, a2], T#1, ->T<256B>, [a3]
# Supported eight-bit types use both words: four M32 or eight M16 columns.
# CHECK: error:
MGATHER <Encoding=E6, Col=5, U8, CUBE_M32>, [a0, a1, a2], T#1, ->T<256B>, [a3, a4]
# CHECK: error:
MGATHER <Encoding=E6, Col=9, U8, CUBE_M16>, [a0, a1, a2], T#1, ->T<256B>, [a3, a4]
# An explicit zero effective dimension remains illegal.
# CHECK: error:
MGATHER <Encoding=E5, Col=1, ValidRow=0, U32, CUBE_M32>, [a0, a1, a2], T#1, ->T<128B>, [a3]
