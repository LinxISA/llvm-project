# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s
# RUN: echo "0x81 0x11 0x00 0xc2" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=OLD
# RUN: echo "0x81 0x91 0x03 0x00" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=RETIRED
# RUN: echo "0x81 0x11 0x53 0x10" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=CUBE-NAMED
# RUN: echo "0x13 0xea 0x08 0x00 0x93 0xea 0x08 0x00 0x13 0xeb 0x08 0x00 0x93 0xeb 0x08 0x00" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=BIOT-DST
# RUN: echo "0x23 0x11 0xf0 0x19" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=BAD-DATR
# RUN: echo "0x23 0x10 0xf0 0xc1" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=BAD-DATR
# RUN: echo "0x81 0x11 0x01 0x78" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=BAD-DTYPE
# RUN: echo "0x81 0x11 0x01 0xf8" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=BAD-DTYPE

B.ARG NORM.normal
BSTART.CUBE 5, FP16
BSTART.FIXP 0, FP16
BSTART.TEPL 0, 5, FP16
B.IOT t#1, mask=1111, last, ->acc<1KB>
B.IOT t#1, mask=1111, last, ->u<128KB>
BSTART.TTRANSPOSE FP16
BSTART.TSORT32 FP16
B.DATR Layout21, NOT_A_DTYPE, Null
B.CATR NOT_AN_ATTRIBUTE
B.DATR Layout21, DTYPE_NONE, Null, cmode8
B.DATR Layout21, DTYPE_NONE, Null, rmodebogus
BSTART.TLOAD DTYPE_NONE
BSTART.TEPL 0, 0, DTYPE_NONE
BSTART.TLOAD 15
B.CATR FP16
B.DATR NORM, DTYPE_NONE, Null, atomic
B.DATR NORM, DTYPE_NONE, Null, cmode6
B.DATR Layout2, DTYPE_NONE, Null

# CHECK: error: unrecognized instruction 'b.arg'
# CHECK: error: unrecognized instruction 'bstart.cube'
# CHECK: error: unrecognized instruction 'bstart.fixp'
# CHECK: error: BSTART.TEPL Mode/Function is reserved in PTO ISA 0.58
# CHECK: error: B.IOT destination must be one of t/u/m/n
# CHECK: error: tile size must be in strict range 128B..64KB
# CHECK: error: unrecognized instruction 'bstart.ttranspose'
# CHECK: error: unrecognized instruction 'bstart.tsort32'
# CHECK: error: unknown block attribute: NOT_A_DTYPE
# CHECK: error: unknown block attribute: NOT_AN_ATTRIBUTE
# CHECK: error: unknown block attribute: CMODE8
# CHECK: error: unknown block attribute: RMODEBOGUS
# CHECK: error: BSTART requires an assigned concrete DataType
# CHECK: error: BSTART.TEPL requires an assigned concrete DataType
# CHECK: error: BSTART requires an assigned concrete DataType
# CHECK: error: unknown block attribute: FP16
# CHECK: error: unknown block attribute: ATOMIC
# CHECK: error: unknown block attribute: CMODE6
# CHECK: error: unknown block attribute: LAYOUT2
# OLD: warning: invalid instruction encoding
# RETIRED: warning: invalid instruction encoding
# CUBE-NAMED: BSTART.TMATMULMX.BIAS{{[[:space:]]+}}TF32
# CUBE-NAMED-NOT: BSTART.CUBE
# BIOT-DST: B.IOT mask=1100, last, ->t<128B>
# BIOT-DST: B.IOT mask=1100, last, ->u<128B>
# BIOT-DST: B.IOT mask=1100, last, ->m<128B>
# BIOT-DST: B.IOT mask=1100, last, ->n<128B>
# BAD-DATR: warning: invalid instruction encoding
# BAD-DATR-NOT: B.DATR
# BAD-DTYPE: warning: invalid instruction encoding
# BAD-DTYPE-NOT: BSTART.TLOAD
