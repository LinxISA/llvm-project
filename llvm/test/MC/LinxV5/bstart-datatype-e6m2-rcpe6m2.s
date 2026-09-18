# RUN: llvm-mc -triple linx64v5 -show-encoding %s 2>&1 | FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -triple linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

# PTO-ISA tile data types E6M2 (code 15) and RCPE6M2 (code 21) are assigned
# five-bit DataType codes (pto-spec #322). RCPE6M2 is a source-only derived
# reciprocal interpretation of the E6M2 code space: TCVT accepts it only as a
# source and only with FP16/BF16 destinations; it has no destination encoding.
# This test pins the assembler/disassembler round-trip of both names in the
# BSTART.TEPL TCVT header (DataType field = instruction bits 31:27).

# ASM-LABEL: rcpe6m2_source:
# ASM: BSTART.TEPL TCVT, rcpe6m2
# ASM: encoding: [0x81,0x91,0xb1,0xa9]
# OBJ-LABEL: <rcpe6m2_source>:
# OBJ: BSTART.TEPL TCVT, rcpe6m2
# OBJ: B.DATR FP16, byte0, Null, RNONE, nosat
# OBJ: B.IOT t#1, mask=1111, last, ->t<2KB>
rcpe6m2_source:
BSTART.TEPL TCVT, rcpe6m2
B.DATR FP16, byte0, Null, RNONE, NOSAT
C.B.DIMI 64, ->lb0
C.B.DIMI 16, ->lb1
B.IOT t#1, mask=1111, last, ->t<2KB>
BSTOP

# ASM-LABEL: rcpe6m2_bf16:
# ASM: BSTART.TEPL TCVT, rcpe6m2
# OBJ: BSTART.TEPL TCVT, rcpe6m2
# OBJ: B.DATR BF16, byte0, Null, RNONE, nosat
rcpe6m2_bf16:
BSTART.TEPL TCVT, rcpe6m2
B.DATR BF16, byte0, Null, RNONE, NOSAT
C.B.DIMI 64, ->lb0
C.B.DIMI 16, ->lb1
B.IOT t#1, mask=1111, last, ->t<2KB>
BSTOP

# E6M2 is an ordinary 8-bit float type: both conversion directions with the
# profile-legal FP16/BF16 counterparts must spell and decode.
# ASM-LABEL: e6m2_source:
# ASM: BSTART.TEPL TCVT, e6m2
# OBJ: BSTART.TEPL TCVT, e6m2
# OBJ: B.DATR FP16, byte0, Null, RNONE, nosat
e6m2_source:
BSTART.TEPL TCVT, e6m2
B.DATR FP16, byte0, Null, RNONE, NOSAT
C.B.DIMI 64, ->lb0
C.B.DIMI 16, ->lb1
B.IOT t#1, mask=1111, last, ->t<2KB>
BSTOP

# ASM-LABEL: e6m2_destination:
# ASM: BSTART.TEPL TCVT, FP16
# OBJ: BSTART.TEPL TCVT, FP16
# OBJ: B.DATR e6m2, byte0, Null, RNONE, nosat
e6m2_destination:
BSTART.TEPL TCVT, FP16
B.DATR e6m2, byte0, Null, RNONE, NOSAT
C.B.DIMI 64, ->lb0
C.B.DIMI 16, ->lb1
B.IOT t#1, mask=1111, last, ->t<2KB>
BSTOP
