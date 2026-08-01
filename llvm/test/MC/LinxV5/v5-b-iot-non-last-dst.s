# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

B.IOT mask=15, TSize=6, ->t
B.IOT t#1, mask=15, TSize=5, ->u
B.IOT t#1, u#2, mask=15, TSize=4, ->m

# CHECK: B.IOT{{.*}}mask=1111, TSize=6,{{.*}}->t
# CHECK: B.IOT{{.*}}t#1, mask=1111, TSize=5,{{.*}}->u
# CHECK: B.IOT{{.*}}t#1, u#2, mask=1111, TSize=4,{{.*}}->m

# DIS: B.IOT{{.*}}mask=1111, TSize=6,{{.*}}->t
# DIS: B.IOT{{.*}}t#1, mask=1111, TSize=5,{{.*}}->u
# DIS: B.IOT{{.*}}t#1, u#2, mask=1111, TSize=4,{{.*}}->m
