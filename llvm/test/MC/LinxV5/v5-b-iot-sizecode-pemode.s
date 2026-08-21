# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=DIS

# PTO-ISA ADR 0069 (commit 1e91bf9): B.IOT/B.IOS re-encode size and PE mode.
#   SizeCode 4-bit @ [18:15]: 0=source-only; B.IOT dest 1..10 (128B..64KB/PE);
#     B.IOS dest 1..12 (128B..256KB/PE); 13..15 reserved.
#   PEMode 3-bit @ [11:9]: 000->0000 001->1000(PE0) 010->0100 011->0010
#     100->0001(PE3) 101->1100 110->1110 111->1111.

# === Positive: SizeCode beyond 8KB ===
B.IOT mask=1111, last, ->t<16KB>
B.IOT mask=1111, last, ->t<32KB>
B.IOT mask=1111, last, ->t<64KB>
B.IOS mask=1111, ->S0<64KB>
B.IOS mask=1111, ->S0<128KB>
B.IOS mask=1111, ->S0<256KB>

# === Positive: all eight PEMode masks ===
B.IOT mask=0000, last, ->t<128B>
B.IOT mask=1000, last, ->t<128B>
B.IOT mask=0100, last, ->t<128B>
B.IOT mask=0010, last, ->t<128B>
B.IOT mask=0001, last, ->t<128B>
B.IOT mask=1100, last, ->t<128B>
B.IOT mask=1110, last, ->t<128B>
B.IOT mask=1111, last, ->t<128B>
B.IOS S3, mask=1111
B.IOS S4, mask=1100

# ENC: B.IOT{{.*}}->t<16KB>
# ENC: B.IOT{{.*}}->t<32KB>
# ENC: B.IOT{{.*}}->t<64KB>
# ENC: B.IOS{{.*}}->S0<64KB>
# ENC: B.IOS{{.*}}->S0<128KB>
# ENC: B.IOS{{.*}}->S0<256KB>
# ENC: B.IOT{{.*}}mask=0000{{.*}}->t<128B>
# ENC: B.IOT{{.*}}mask=1000{{.*}}->t<128B>
# ENC: B.IOT{{.*}}mask=0100{{.*}}->t<128B>
# ENC: B.IOT{{.*}}mask=0010{{.*}}->t<128B>
# ENC: B.IOT{{.*}}mask=0001{{.*}}->t<128B>
# ENC: B.IOT{{.*}}mask=1100{{.*}}->t<128B>
# ENC: B.IOT{{.*}}mask=1110{{.*}}->t<128B>
# ENC: B.IOT{{.*}}mask=1111{{.*}}->t<128B>
# ENC: B.IOS{{.*}}S3, mask=1111
# ENC: B.IOS{{.*}}S4, mask=1100

# DIS: B.IOT{{.*}}->t<16KB>
# DIS: B.IOT{{.*}}->t<32KB>
# DIS: B.IOT{{.*}}->t<64KB>
# DIS: B.IOS{{.*}}->S0<64KB>
# DIS: B.IOS{{.*}}->S0<128KB>
# DIS: B.IOS{{.*}}->S0<256KB>
# DIS: B.IOT{{.*}}mask=0000{{.*}}->t<128B>
# DIS: B.IOT{{.*}}mask=1000{{.*}}->t<128B>
# DIS: B.IOT{{.*}}mask=0100{{.*}}->t<128B>
# DIS: B.IOT{{.*}}mask=0010{{.*}}->t<128B>
# DIS: B.IOT{{.*}}mask=0001{{.*}}->t<128B>
# DIS: B.IOT{{.*}}mask=1100{{.*}}->t<128B>
# DIS: B.IOT{{.*}}mask=1110{{.*}}->t<128B>
# DIS: B.IOT{{.*}}mask=1111{{.*}}->t<128B>
# DIS: B.IOS{{.*}}S3, mask=1111
# DIS: B.IOS{{.*}}S4, mask=1100