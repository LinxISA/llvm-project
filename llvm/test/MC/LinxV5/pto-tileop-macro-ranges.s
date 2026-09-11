# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros - | FileCheck %s --check-prefix=PHYS
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=MACRO

TADD <Row=8, Col=64, FP32>, T#1[base=a0, offset=4], T#2[base=a1, offset=8], ->T<2KB>[base=a2, offset=16]

TLOAD <Row=32, Col=1, FP32>, [base=a0], ->S0<128B>[base=a1, offset=4]

TSTORE <Row=32, Col=1, FP32>, S0[base=a2, offset=8], [base=a3]

# PHYS: BSTART.TEPL{{.*}}TADD, FP32
# PHYS: B.IOT{{.*}}t#1, t#2{{.*}}->t<2KB>
# PHYS-NEXT: B.SUBVIEW{{.*}}0, a0, 4, 1
# PHYS-NEXT: B.SUBVIEW{{.*}}1, a1, 8, 1
# PHYS-NEXT: B.ASSEMBLE{{.*}}1, 1, a2, 16, 1
# PHYS: BSTART.TLSU{{.*}}TLOAD, FP32
# PHYS: B.IOR{{.*}}a0
# PHYS-NEXT: B.IOS{{.*}}->S0<128B>
# PHYS-NEXT: B.ASSEMBLE{{.*}}1, 1, a1, 4, 1
# PHYS: BSTART.TLSU{{.*}}TSTORE, FP32
# PHYS: B.IOS{{.*}}S0
# PHYS-NEXT: B.SUBVIEW{{.*}}0, a2, 8, 1
# PHYS-NEXT: B.IOR{{.*}}a3

# MACRO: TADD{{ +}}<
# MACRO-SAME: T#1[base=a0, offset=4]
# MACRO-SAME: T#2[base=a1, offset=8]
# MACRO-SAME: ->T<2KB>[base=a2, offset=16]
# MACRO: TLOAD{{ +}}<
# MACRO-SAME: ->S0<128B>[base=a1, offset=4]
# MACRO: BSTART.TLSU{{.*}}TSTORE, FP32
