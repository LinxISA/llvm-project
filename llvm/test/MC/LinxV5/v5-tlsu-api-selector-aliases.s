# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.mc.o
# RUN: clang --target=linx64v5 -c %s -o %t.clang.o
# RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.mc.o | FileCheck %s --check-prefix=DIS
# RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.clang.o | FileCheck %s --check-prefix=DIS

# Public API Shared TMOV aliases use bundle schemas carried by Function 2.
# Their API spellings must therefore encode exactly like numeric Function 2.
BSTART.TLSU TMOV.L2S.INSERT, FP32
BSTART.TLSU TMOV.L2S.PUBLISH, FP32
BSTART.TLSU TMOV.S2L.BROADCAST, FP32
BSTART.TLSU TMOV.S2L.EXTRACT, FP32
BSTART.TLSU 2, FP32

# MSCATTER.MAX is PTO-AS TLSU Function 19.
BSTART.TLSU MSCATTER.MAX, U32
BSTART.TLSU 19, U32

# The remaining public MSCATTER spellings own Functions 20 and 22--27.
BSTART.TLSU MSCATTER.MIN, U32
BSTART.TLSU 20, U32
BSTART.TLSU MSCATTER.INC, U32
BSTART.TLSU 22, U32
BSTART.TLSU MSCATTER.DEC, U32
BSTART.TLSU 23, U32
BSTART.TLSU MSCATTER.AND, U32
BSTART.TLSU 24, U32
BSTART.TLSU MSCATTER.OR, U32
BSTART.TLSU 25, U32
BSTART.TLSU MSCATTER.XOR, U32
BSTART.TLSU 26, U32
BSTART.TLSU MSCATTER.POPC, U32
BSTART.TLSU 27, U32

# ENC-COUNT-5: BSTART.TLSU{{.*}}TMOV, FP32{{.*}}encoding: [0x81,0x11,0x21,0x08]
# ENC-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.MAX, U32{{.*}}encoding: [0x81,0x11,0x31,0xc9]
# ENC-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.MIN, U32{{.*}}encoding: [0x81,0x11,0x41,0xc9]
# ENC-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.INC, U32{{.*}}encoding: [0x81,0x11,0x61,0xc9]
# ENC-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.DEC, U32{{.*}}encoding: [0x81,0x11,0x71,0xc9]
# ENC-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.AND, U32{{.*}}encoding: [0x81,0x11,0x81,0xc9]
# ENC-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.OR, U32{{.*}}encoding: [0x81,0x11,0x91,0xc9]
# ENC-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.XOR, U32{{.*}}encoding: [0x81,0x11,0xa1,0xc9]
# ENC-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.POPC, U32{{.*}}encoding: [0x81,0x11,0xb1,0xc9]

# DIS-COUNT-5: BSTART.TLSU{{.*}}TMOV, FP32
# DIS-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.MAX, U32
# DIS-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.MIN, U32
# DIS-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.INC, U32
# DIS-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.DEC, U32
# DIS-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.AND, U32
# DIS-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.OR, U32
# DIS-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.XOR, U32
# DIS-COUNT-2: BSTART.TLSU{{.*}}MSCATTER.POPC, U32
