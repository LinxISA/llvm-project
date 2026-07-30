# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s
# RUN: echo "0x81 0x11 0x00 0xc2" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=OLD
# RUN: echo "0x23 0xa0 0x0f 0x00" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=RETIRED
# RUN: echo "0x81 0x91 0x03 0x00" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=RETIRED
# RUN: echo "0x81 0x11 0x53 0x10" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=CUBE-NAMED
# RUN: echo "0x13 0xe2 0x09 0x00 0x93 0xe2 0x09 0x00 0x13 0xe3 0x09 0x00 0x93 0xe3 0x09 0x00" | llvm-mc -triple=linx64 -disassemble 2>&1 | FileCheck %s --check-prefix=BIOT-DST

B.ARG NORM.normal
BSTART.CUBE 5, FP16
BSTART.FIXP 0, FP16
BSTART.TEPL 0, 5, FP16
B.IOT t#1, last, ->acc<1KB>
B.IOT t#1, last, ->u<16KB>
BSTART.TTRANSPOSE FP16
BSTART.TSORT32 FP16

# CHECK: error: unrecognized instruction 'b.arg'
# CHECK: error: unrecognized instruction 'bstart.cube'
# CHECK: error: unrecognized instruction 'bstart.fixp'
# CHECK: error: BSTART.TEPL Mode/Function is reserved in PTO ISA 0.57.1
# CHECK: error: B.IOT destination must be one of t/u/m/n; ACC is implicit
# CHECK: error: tile size must be in strict range 128B..8KB
# CHECK: error: unrecognized instruction 'bstart.ttranspose'
# CHECK: error: unrecognized instruction 'bstart.tsort32'
# OLD: warning: invalid instruction encoding
# RETIRED: warning: invalid instruction encoding
# CUBE-NAMED: BSTART.TMATMULMX.BIAS{{[[:space:]]+}}FP16
# CUBE-NAMED-NOT: BSTART.CUBE
# BIOT-DST-COUNT-4: warning: invalid instruction encoding
