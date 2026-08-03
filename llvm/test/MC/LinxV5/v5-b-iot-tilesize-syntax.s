# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=DIS

# v5: B.IOT destination-suffix TileSize syntax. TileSize follows the
# destination tile as "<8KB>". Legacy "TSize=N" input is still accepted and
# canonicalizes to the bracket form. objdump prints only the bracket form.

# All 7 legal sizes (512B..32KB) round-trip (NoSrc-Dst form).
B.IOT mask=1111, last, ->t<512B>
B.IOT mask=1111, last, ->t<1KB>
B.IOT mask=1111, last, ->t<2KB>
B.IOT mask=1111, last, ->t<4KB>
B.IOT mask=1111, last, ->t<8KB>
B.IOT mask=1111, last, ->t<16KB>
B.IOT mask=1111, last, ->t<32KB>

# One-source + destination.
B.IOT t#1, mask=1111, last, ->u<8KB>
# Two-source + destination.
B.IOT t#1, u#2, mask=1111, last, ->m<2KB>

# Legacy "TSize=N" input must still assemble and canonicalize to bracket form.
B.IOT mask=1111, TSize=5, last, ->n

# ENC: B.IOT{{.*}}->t<512B>
# ENC: B.IOT{{.*}}->t<1KB>
# ENC: B.IOT{{.*}}->t<2KB>
# ENC: B.IOT{{.*}}->t<4KB>
# ENC: B.IOT{{.*}}->t<8KB>
# ENC: B.IOT{{.*}}->t<16KB>
# ENC: B.IOT{{.*}}->t<32KB>
# ENC: B.IOT{{.*}}t#1,{{.*}}->u<8KB>
# ENC: B.IOT{{.*}}t#1, u#2,{{.*}}->m<2KB>
# ENC: B.IOT{{.*}}->n<8KB>

# DIS: B.IOT{{.*}}->t<512B>
# DIS: B.IOT{{.*}}->t<1KB>
# DIS: B.IOT{{.*}}->t<2KB>
# DIS: B.IOT{{.*}}->t<4KB>
# DIS: B.IOT{{.*}}->t<8KB>
# DIS: B.IOT{{.*}}->t<16KB>
# DIS: B.IOT{{.*}}->t<32KB>
# DIS: B.IOT{{.*}}t#1,{{.*}}->u<8KB>
# DIS: B.IOT{{.*}}t#1, u#2,{{.*}}->m<2KB>
# DIS: B.IOT{{.*}}->n<8KB>
