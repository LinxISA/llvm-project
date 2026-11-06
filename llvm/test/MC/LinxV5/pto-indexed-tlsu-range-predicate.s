# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s
# RUN: %python %S/../../../utils/linxv5/roundtrip_tile_macro_forms.py llvm-mc llvm-objdump %s
# CHECK: MGATHER {{.*}}PredInv=1, ZeroMask=1
# CHECK: MSCATTER_ADD {{.*}}PredInv=1, ZeroMask=1
# CHECK: MSCATTER_XOR {{.*}}PredInv=1, ZeroMask=1
.text
MGATHER <Encoding=E11, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, ->T<128B>, U#1
MGATHER_ADD <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_AND <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_CAS <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, T#1, ->T<128B>, U#1
MGATHER_DEC <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_EXCH <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_INC <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_MASK <Encoding=E2, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, U#1, ->T<128B>
MGATHER_MAX <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_MIN <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_OR <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_XOR <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MSCATTER <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_ADD <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_AND <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_DEC <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_INC <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_MASK <Encoding=E2, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_MAX <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_MIN <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_OR <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_POPC <Encoding=E4, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, U#1
MSCATTER_XOR <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=1, ZeroMask=1>, [base=a0, min=zero, max=a2], T#1, T#1, U#1

# Zero-valued attributes decode as ordinary B.DATR and must still fold.
MGATHER <Encoding=E11, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, ->T<128B>, U#1
MGATHER_ADD <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_AND <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_CAS <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, T#1, ->T<128B>, U#1
MGATHER_DEC <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_EXCH <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_INC <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_MASK <Encoding=E2, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, U#1, ->T<128B>
MGATHER_MAX <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_MIN <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_OR <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MGATHER_XOR <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, ->T<128B>, U#1
MSCATTER <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_ADD <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_AND <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_DEC <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_INC <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_MASK <Encoding=E2, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_MAX <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_MIN <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_OR <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
MSCATTER_POPC <Encoding=E4, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, U#1
MSCATTER_XOR <Encoding=E9, Col=1, U32, CUBE_M32, PredInv=0, ZeroMask=0>, [base=a0, min=zero, max=a2], T#1, T#1, U#1
