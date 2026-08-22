# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
# RUN: llvm-mc -triple=linx64 -show-encoding %s | sed -E 's/[[:space:]]*#.*$//' | llvm-mc -triple=linx64 -filetype=obj -o %t.re
# RUN: cmp %t %t.re

BSTART.TLOAD S8
BSTART.TMATMUL S8
add s8, zero, ->a0

# ENC: BSTART.TLOAD S8{{.*}}encoding:
# ENC: BSTART.TMATMUL S8{{.*}}encoding:
# ENC: add s8, zero, ->a0{{.*}}encoding:

# DIS: BSTART.TLOAD{{[[:space:]]+}}S8
# DIS: BSTART.TMATMUL{{[[:space:]]+}}S8
# DIS: add{{[[:space:]]+}}s8, zero, ->a0
