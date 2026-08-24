# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=DIS

# PTO-ISA ADR-0070 (pto-spec 23ca883): B.DATR.Layout codes 21..26 select the
# GM<->Local CUBE cell transport. 21..23 (ND2M32/ND2M16/ND2N8) are legal
# only on TLOAD; 24..26 (M322ND/M162ND/N82ND) only on TSTORE. Each code
# round-trips: assembler emits the canonical B.DATR selector and objdump
# restores it.

TLOAD.ND2M32 <LB0: 1, LB1: 1, LB2: 1, FP32, Zero> [a0], ->t<1KB>
TLOAD.ND2M16 <LB0: 1, LB1: 1, LB2: 1, FP32, Zero> [a0], ->t<1KB>
TLOAD.ND2N8  <LB0: 1, LB1: 1, LB2: 1, FP32, Zero> [a0], ->t<1KB>
TSTORE.M322ND <LB0: 1, LB1: 1, LB2: 1, FP32>, T#1, [a1]
TSTORE.M162ND <LB0: 1, LB1: 1, LB2: 1, FP32>, T#1, [a1]
TSTORE.N82ND  <LB0: 1, LB1: 1, LB2: 1, FP32>, T#1, [a1]

# ENC: BSTART.TLSU{{.*}}TLOAD, FP32
# ENC: B.DATR{{.*}}ND2M32.normal, Zero
# ENC: B.DATR{{.*}}ND2M16.normal, Zero
# ENC: B.DATR{{.*}}ND2N8.normal, Zero
# ENC: BSTART.TLSU{{.*}}TSTORE, FP32
# ENC: B.DATR{{.*}}M322ND.normal, Null
# ENC: B.DATR{{.*}}M162ND.normal, Null
# ENC: B.DATR{{.*}}N82ND.normal, Null

# DIS: B.DATR{{.*}}ND2M32.normal, Zero
# DIS: B.DATR{{.*}}ND2M16.normal, Zero
# DIS: B.DATR{{.*}}ND2N8.normal, Zero
# DIS: B.DATR{{.*}}M322ND.normal, Null
# DIS: B.DATR{{.*}}M162ND.normal, Null
# DIS: B.DATR{{.*}}N82ND.normal, Null

# TileOP emits the block start and descriptor as separate inline-asm
# statements.  The parser carries the TLSU TLOAD/TSTORE direction to the
# following B.DATR statement for the canonical selector legality check.
BSTART.TLSU TLOAD, FP32
B.DATR ND2M32.normal, Zero
BSTART.TLSU TSTORE, FP32
B.DATR M322ND.normal, Null

# ENC: BSTART.TLSU{{.*}}TLOAD, FP32
# ENC: B.DATR{{.*}}ND2M32.normal, Zero
# ENC: BSTART.TLSU{{.*}}TSTORE, FP32
# ENC: B.DATR{{.*}}M322ND.normal, Null
# DIS: BSTART.TLSU{{.*}}TLOAD, FP32
# DIS: B.DATR{{.*}}ND2M32.normal, Zero
# DIS: BSTART.TLSU{{.*}}TSTORE, FP32
# DIS: B.DATR{{.*}}M322ND.normal, Null
