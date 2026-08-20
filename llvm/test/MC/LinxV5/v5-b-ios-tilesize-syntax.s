# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=DIS

# v5: B.IOS destination TSize uses its own mapping (1=512B .. 7=32KB),
# distinct from B.IOT's density mapping (1=128B .. 7=8KB). The decoder
# accepts every TSize value on the shared B_IOS instruction and printB_IOS
# renders the source (TSize=0) or destination (TSize!=0) form.

# Source form (TSize=0).
B.IOS S0, mask=1111
B.IOS S255, mask=1111

# Destination form, all seven per-PE sizes of the B.IOS map.
B.IOS mask=1111, ->S0<512B>
B.IOS mask=1111, ->S1<1KB>
B.IOS mask=1111, ->S2<2KB>
B.IOS mask=1111, ->S3<4KB>
B.IOS mask=1111, ->S4<8KB>
B.IOS mask=1111, ->S5<16KB>
B.IOS mask=1111, ->S6<32KB>

# B.IOT keeps its original density map: 128B..8KB.
B.IOT mask=1111, last, ->t<128B>
B.IOT mask=1111, last, ->t<8KB>

# ENC: B.IOS{{.*}}S0, mask=1111
# ENC: B.IOS{{.*}}S255, mask=1111
# ENC: B.IOS{{.*}}->S0<512B>
# ENC: B.IOS{{.*}}->S1<1KB>
# ENC: B.IOS{{.*}}->S2<2KB>
# ENC: B.IOS{{.*}}->S3<4KB>
# ENC: B.IOS{{.*}}->S4<8KB>
# ENC: B.IOS{{.*}}->S5<16KB>
# ENC: B.IOS{{.*}}->S6<32KB>
# ENC: B.IOT{{.*}}->t<128B>
# ENC: B.IOT{{.*}}->t<8KB>

# DIS: B.IOS{{.*}}S0, mask=1111
# DIS: B.IOS{{.*}}S255, mask=1111
# DIS: B.IOS{{.*}}->S0<512B>
# DIS: B.IOS{{.*}}->S1<1KB>
# DIS: B.IOS{{.*}}->S2<2KB>
# DIS: B.IOS{{.*}}->S3<4KB>
# DIS: B.IOS{{.*}}->S4<8KB>
# DIS: B.IOS{{.*}}->S5<16KB>
# DIS: B.IOS{{.*}}->S6<32KB>
# DIS: B.IOT{{.*}}->t<128B>
# DIS: B.IOT{{.*}}->t<8KB>