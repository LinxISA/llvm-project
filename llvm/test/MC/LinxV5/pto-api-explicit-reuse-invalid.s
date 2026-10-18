# RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s
# RUN: not clang --target=linx64v5 -c %s -o %t.o 2>&1 | FileCheck %s
B.IOT t#1.reuse.reuse, mask=1111
# CHECK: error:
B.IOT t#1.last, mask=1111
# CHECK: error:
B.IOT t#1.reuse, mask=1111, last, ->u.reuse<128B>
# CHECK: error:
BSTART.TMOV.UNKNOWN U32
# CHECK: error:
B.IOS mask=1111, ->S0.reuse<128B>
# CHECK: error:
B.IOS mask=1111, S0.reuse<128B>
# CHECK: error:
B.IOS S0.last, mask=1111
# CHECK: error:
B.IOS S0.reuse.reuse, mask=1111
# CHECK: error:
B.IOT mask=1111, last, ->t<128KB>
# CHECK: error:
B.IOT t#1.reuse, mask=1111, last, ->t<256KB>
# CHECK: error:
B.IOT t#1.reuse, u#1.reuse, mask=1111, TSize=11, ->t
# CHECK: error:
