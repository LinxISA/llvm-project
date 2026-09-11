# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

# A 128-byte destination with Col=64 derives Row=4 for packed 4-bit types.
TADD <Row=4, Col=64, E2M1X2>, T#1, T#2, ->T<128B>
TADD <Row=4, Col=64, E1M2X2>, T#1, T#2, ->T<128B>
TADD <Row=4, Col=64, HiF4x2>, T#1, T#2, ->T<128B>
TADD <Row=4, Col=64, S4x2>, T#1, T#2, ->T<128B>
TADD <Row=4, Col=64, U4x2>, T#1, T#2, ->T<128B>

# CHECK: TADD{{ +}}<Row=4, Col=64, e2m1x2>
# CHECK: TADD{{ +}}<Row=4, Col=64, e1m2x2>
# CHECK: TADD{{ +}}<Row=4, Col=64, HiF4x2>
# CHECK: TADD{{ +}}<Row=4, Col=64, S4x2>
# CHECK: TADD{{ +}}<Row=4, Col=64, U4x2>

# Six-bit floating encodings and E8M0 occupy one byte per element, so Row=2.
TADD <Row=2, Col=64, E3M2>, T#1, T#2, ->T<128B>
TADD <Row=2, Col=64, E2M3>, T#1, T#2, ->T<128B>
TADD <Row=2, Col=64, E8M0>, T#1, T#2, ->T<128B>

# CHECK: TADD{{ +}}<Row=2, Col=64, e3m2>
# CHECK: TADD{{ +}}<Row=2, Col=64, e2m3>
# CHECK: TADD{{ +}}<Row=2, Col=64, e8m0>
