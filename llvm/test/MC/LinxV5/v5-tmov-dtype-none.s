# RUN: llvm-mc -triple=linx64v5 --assemble -filetype=obj < %s | llvm-objdump -d - | FileCheck %s

# Issue #64 (llvm-project): compiler-inserted tile window copies lower through
# the TCOPY macro. The emitted carrier is the minimal BSTART.TMOV form: code 31
# DTYPE_NONE selects source-descriptor inference, and B.DATR Layout / B.DIM
# shape are optional per the normative asl/block/execution/BSTART.TMOV.asl
# contract ("DataType accepts the 25 concrete TileDataType codes and code 31
# DTYPE_NONE for source-descriptor inference"). The functional model accepts
# it via inference; only the gfsim timing paths that skip inference reject it.
# Both spellings must encode identically and round-trip through the
# disassembler.

# CHECK: BSTART.TLSU TMOV, DTYPE_NONE
# CHECK-NEXT: B.IOT t#1, mask=1111, last, ->t<128B>
TCOPY T#1, ->T<128B>

# CHECK: BSTART.TLSU TMOV, DTYPE_NONE
# CHECK-NEXT: B.IOT t#1, mask=1111, last, ->t<128B>
# CHECK-NEXT: BSTOP
BSTART.TLSU TMOV, DTYPE_NONE
B.IOT t#1, mask=1111, last, ->t<128B>
BSTOP

# A concrete dtype keeps its explicit carrier (source-level TMOV form).
# CHECK: BSTART.TLSU TMOV, FP32
# CHECK-NEXT: B.IOT t#1, mask=1111, last, ->t<512B>
# CHECK-NEXT: BSTOP
BSTART.TLSU TMOV, FP32
B.IOT t#1, mask=1111, last, ->t<512B>
BSTOP
