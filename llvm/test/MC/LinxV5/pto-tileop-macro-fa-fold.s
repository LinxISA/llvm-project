# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=HEX
# RUN: llvm-objdump -d %t | %python %S/../../../utils/linxv5/verify_tile_macro_hex_layout.py

# FA emits the shared-right matrix form with one logical IOT group split
# across a source-only IOT and a destination-only IOT.
BSTART.CUBE TMATMUL, FP32
B.DATR FP32, byte0, CCTRL.None, RNONE, nosat
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 1, 0
B.DIM a5, 0, ->lb0
C.B.DIMI 128, ->lb1
C.B.DIMI 128, ->lb2
B.IOS S0, mask=1111
B.IOT N#1, mask=1111
B.IOT mask=1111, last, ->M<16KB>

# The accumulator form splits three logical local operands over three IOTs
# and interleaves the shared operand.
BSTART.CUBE TMATMUL.ACC, FP32
B.DATR FP32, byte0, CCTRL.None, RNONE, nosat
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 1, 0
B.DIM s2, 0, ->lb0
C.B.DIMI 128, ->lb1
C.B.DIMI 128, ->lb2
B.IOT U#1, mask=1111
B.IOS S0, mask=1111
B.IOT M#1, mask=1111
B.IOT mask=1111, last, ->N<16KB>

# The CUBE-layout TSTORE derives Row from encoded ValidRow, then omits the equal default
# ValidRow while preserving the nondefault ValidCol and address pair.
BSTART.TLSU TSTORE, FP32
B.DATR M322ND.normal, Null
C.B.DIMI 128, ->lb0
C.B.DIMI 32, ->lb1
B.IOT T#1, mask=1111, last
B.IOR [a0, a4], []

# Missing lb0/lb2 commands retain the Tile block default of 1 and must not
# prevent the physical block from folding back into a macro instruction.
BSTART.TEPL TEXPANDS, FP32
C.B.DIMI 32, ->lb1
B.IOT mask=1111, last, ->T<128B>
B.IOR [a7], []
addi zero, 0, ->zero

# CHECK-NOT: BSTART
# CHECK: TMATMUL{{ +}}<M=a5, N=128, K=128, FP32, TransposeB>, N#1, S0, ->M<16KB>
# CHECK: TMATMUL_ACC{{ +}}<M=s2, N=128, K=128, FP32, TransposeB>, U#1, M#1, S0, ->N<16KB>
# CHECK: TSTORE{{ +}}<Row=32, ValidCol=128, FP32, M322ND>, T#1, [base=a0, stride=a4]
# CHECK: TEXPANDS{{ +}}<Row=32, Col=1, FP32>, a7, ->T<128B>

# HEX: {{^ *0: [0-9a-f]{8} [0-9a-f]{8}[[:space:]]+}}TMATMUL
# HEX-NEXT: {{^[[:space:]]+[0-9a-f]{8} [0-9a-f]{8}$}}
# HEX: {{^ *[0-9a-f]+: [0-9a-f]{4}[[:space:]]+}}c.movi
