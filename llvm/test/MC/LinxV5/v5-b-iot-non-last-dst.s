# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d - | FileCheck %s --check-prefix=DIS

# v5: non-last B.IOT destinations use the destination-suffix TileSize form.
# Legacy "TSize=N, ->t" input is still accepted; canonical print is "->t<4KB>".
# TSize is encoded at PE granularity (6=4KB per PE).
B.IOT mask=1111, ->t<4KB>
B.IOT t#1, mask=1111, ->u<2KB>
B.IOT t#1, u#2, mask=1111, ->m<1KB>

# Legacy input remains accepted.
B.IOT mask=15, TSize=6, ->t
B.IOT t#1, mask=15, TSize=5, ->u
B.IOT t#1, u#2, mask=15, TSize=4, ->m

# CHECK: B.IOT{{.*}}mask=1111,{{.*}}->t<4KB>
# CHECK: B.IOT{{.*}}t#1, mask=1111,{{.*}}->u<2KB>
# CHECK: B.IOT{{.*}}t#1, u#2, mask=1111,{{.*}}->m<1KB>
# CHECK: B.IOT{{.*}}mask=1111,{{.*}}->t<4KB>
# CHECK: B.IOT{{.*}}t#1, mask=1111,{{.*}}->u<2KB>
# CHECK: B.IOT{{.*}}t#1, u#2, mask=1111,{{.*}}->m<1KB>

# DIS: B.IOT{{.*}}mask=1111,{{.*}}->t<4KB>
# DIS: B.IOT{{.*}}t#1, mask=1111,{{.*}}->u<2KB>
# DIS: B.IOT{{.*}}t#1, u#2, mask=1111,{{.*}}->m<1KB>
# DIS: B.IOT{{.*}}mask=1111,{{.*}}->t<4KB>
# DIS: B.IOT{{.*}}t#1, mask=1111,{{.*}}->u<2KB>
# DIS: B.IOT{{.*}}t#1, u#2, mask=1111,{{.*}}->m<1KB>
