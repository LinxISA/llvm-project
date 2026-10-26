# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s

# PTO-TMATMUL-ACC-CONTRACT-001 orders the Local mathematical sources C, A, B
# before the newly allocated D. The public macro catalog has this same
# AccTile, SrcTile0, SrcTile1, ->Dst order; the first two sources therefore
# stay source-only and the final source is carried with the destination.
TMATMUL_ACC <M=2, N=3, K=4, FP32, FP32>, t#2, u#3, t#4, ->n<128B>

# CHECK: BSTART.TMATMUL.ACC FP32
# CHECK: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
# CHECK: B.IOT t#2, u#3, mask=1111
# CHECK-NEXT: B.IOT t#4, mask=1111, last, ->n<128B>
