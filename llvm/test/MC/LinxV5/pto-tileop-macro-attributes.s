# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros - | FileCheck %s --check-prefix=PHYS

TCVT <Row=64, Col=1, FP32, FP16, RNE, sat, canon, ND2DN, Zero, PE0_1>, T#1, ->T<128B>
# CHECK: BSTART.TEPL{{.*}}TCVT, FP32
# CHECK-NEXT: B.DATR{{.*}}ND2DN.canon{{.*}}FP16{{.*}}Zero{{.*}}RNE{{.*}}sat

TCMP <Row=32, Col=1, FP32, NE, Zero, sat, PE0>, T#1, T#2, ->a0
# CHECK: BSTART.TEPL{{.*}}TCMP, FP32
# CHECK-NEXT: B.DATR{{.*}}Zero{{.*}}NE{{.*}}#sat
# CHECK: B.IOR{{.*}}->a0

TLOAD <Row=1, FP32, ND2M32, DTYPE_NONE, Zero>, [base=a0, stride=a1], ->T<2KB>
# CHECK: TLOAD{{ +}}<
# CHECK-SAME: Row=1
# CHECK-SAME: FP32
# CHECK-SAME: ND2M32
# CHECK-SAME: Zero
# CHECK-SAME: ->T<2KB>

TLOAD <ValidK=16, ValidN=8, TotalK=32, FP16, OHWI2NK>, [a0, a1, a2], ->S0<128B>
# CHECK: TLOAD{{ +}}<
# CHECK-SAME: OHWI2NK

# Direct CUBE layout uses encoded ValidRow; destination size is capacity only.
TEXPANDS <Row=16, Col=8, U8, CUBE_M16>, a2, ->T<2KB>
# CHECK: TEXPANDS{{ +}}<Row=16, Col=8, U8, CUBE_M16>
# CHECK-SAME: a2
# CHECK-SAME: ->T<2KB>

TMATMUL <M=2, N=3, K=4, FP32, FP16, RNE, sat, PreMode=2, PostMode=2, RowMax, GroupMax, RowMaxInit, PE0_1>, T#1, T#2, T#3, T#1, T#2, ->T<128B>, ->U<128B>, ->M<128B>
# CHECK: TMATMUL{{ +}}<
# CHECK-SAME: M=2
# CHECK-SAME: N=3
# CHECK-SAME: K=4
# CHECK-SAME: PreMode=2
# CHECK-SAME: PostMode=2
# CHECK-SAME: RowMax
# CHECK-SAME: GroupMax
# CHECK-SAME: RowMaxInit
# CHECK-SAME: PE0_1

TMATMUL <M=2, N=3, K=4, FP32, FP16, RNE, sat, PreMode=1, PostMode=1>, T#1, T#2, a0, a1, ->T<128B>
# CHECK: TMATMUL{{ +}}<
# CHECK-SAME: PreMode=1
# CHECK-SAME: PostMode=1
# CHECK-SAME: a0
# CHECK-SAME: a1

TMATMUL_MX <M=2, N=3, K=4, E4M3, E5M2>, T#1, T#2, T#3, T#1, ->T<128B>
# CHECK: TMATMUL_MX{{ +}}<
# CHECK-SAME: e4m3
# CHECK-SAME: e5m2
# CHECK-NOT: DefaultFP

TMATMUL <FP32, TransposeB>, T#1, T#2, ->T<128B>
# CHECK: TMATMUL{{ +}}<FP32, TransposeB>

TSEL <Row=128, Col=1, U8>, a0, a1, T#1, T#2, ->T<128B>
# CHECK: BSTART.TEPL{{.*}}TSEL, U8

GMOV <FP32, ND2DN, PE0>, T#1, a0, ->T<128B>
addi zero, 0, ->zero
# CHECK: GMOV{{ +}}<
# CHECK-SAME: ND2DN
# CHECK-SAME: PE0
# CHECK-SAME: a0
# CHECK-NEXT: c.movi

# PHYS-COUNT-4: B.FPATR
