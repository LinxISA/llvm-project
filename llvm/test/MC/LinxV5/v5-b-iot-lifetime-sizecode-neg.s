# RUN: split-file %s %t
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/one-source.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=SIZE
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/two-source.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=SIZE

# Reuse opcodes must pass through the same Local destination SizeCode gate as
# their bare-source counterparts.
# SIZE: error:

#--- one-source.s
B.IOT t#1.reuse, mask=1111, TSize=11, last, ->u

#--- two-source.s
B.IOT t#1.reuse, u#1.reuse, mask=1111, TSize=11, last, ->u
