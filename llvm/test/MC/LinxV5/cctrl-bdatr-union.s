# RUN: llvm-mc -triple=linx64v5 --assemble -filetype=obj < %s | llvm-objdump -d - | FileCheck %s
# PTO-ISA #236: inside a BSTART.CUBE Matrix bundle (TMATMUL*/TGEMV*
# functions), B.DATR PadValueOrByteId[1:0] is the CCTRL union. The
# disassembler must label it CCTRL.* there and keep the PadValue names in
# non-CUBE contexts, and both spellings must encode identically.

# CHECK: BSTART.CUBE TMATMUL.ACC, FP16
# CHECK-NEXT: B.DATR FP32, byte0, CCTRL.None
BSTART.CUBE TMATMUL.ACC, FP16
B.DATR FP32, byte0, Zero, RNONE, NOSAT
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
B.IOT t#1, mask=1111, last
BSTOP

# CHECK: BSTART.CUBE TMATMUL.ACC, FP16
# CHECK-NEXT: B.DATR FP32, byte0, CCTRL.RawAccumulator
BSTART.CUBE TMATMUL.ACC, FP16
B.DATR FP32, byte0, Max, RNONE, NOSAT
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
B.IOT t#1, mask=1111, last
BSTOP

# CHECK: BSTART.CUBE TGEMV.ACC, FP32
# CHECK-NEXT: B.DATR FP32, byte0, CCTRL.InternalAccHint
BSTART.CUBE TGEMV.ACC, FP32
B.DATR FP32, byte0, CCTRL.InternalAccHint, RNONE, NOSAT
B.IOT t#1, mask=1111, last
BSTOP

# CHECK: BSTART.CUBE TMATMUL.ACC, FP16
# CHECK-NEXT: B.DATR FP32, byte0, CCTRL.RawAccumulatorInternalAccHint
BSTART.CUBE TMATMUL.ACC, FP16
B.DATR FP32, byte0, Null, RNONE, NOSAT
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
B.IOT t#1, mask=1111, last
BSTOP

# Same raw encoding outside a CUBE Matrix bundle keeps the PadValue name:
# CHECK: BSTART.TLSU TMOV, FP32
# CHECK-NEXT: B.DATR FP32, byte0, Max
BSTART.TLSU TMOV, FP32
B.DATR FP32, byte0, Max, RNONE, NOSAT
B.IOT t#1, mask=1111, last
BSTOP
