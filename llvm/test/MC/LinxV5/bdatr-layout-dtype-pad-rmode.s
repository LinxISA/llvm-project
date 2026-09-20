# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d --no-show-raw-insn - | FileCheck --check-prefix=DIS %s
# Issue #100: TCVT's datr_contract allows Layout next to DataType and RMode
# (asl/tile/elementwise-tile-tile/format-conversion/TCVT.asl:
# allowed_nonzero_fields), and a CUBE_M16/M32 source must keep its layout
# while the same B.DATR selects the destination dtype and rounding mode.
# TileOP CUBE TCVT emits `B.DATR CUBE_M32/M16, <dtype>, Null, <rmode>`;
# expose the missing {Layout, SrcType, PadValue} and
# {Layout, SrcType, PadValue, RMode} aliases in the bare spelling (the
# canonical .normal-suffixed spelling was already parsed but had no 4-field
# form). CmpMode/Sat/ByteId stay at their zero defaults. Field words:
# Layout{11:7}, DataType{24:20}, PadValue{28:27}, RMode{17:15}.

# CHECK: B.DATR CUBE_M32, e8m0, Null, RTM // encoding: [0xa3,0x9e,0xd1,0x18]
# DIS: B.DATR CUBE_M32, e8m0, Null, RTM
B.DATR CUBE_M32, e8m0, Null, RTM

# CHECK: B.DATR CUBE_M32, BF16, Null // encoding: [0xa3,0x1e,0x50,0x18]
# DIS: B.DATR CUBE_M32, BF16, Null
B.DATR CUBE_M32, BF16, Null, RNONE

# CHECK: B.DATR CUBE_M16, e8m0, Null, RTM // encoding: [0xa3,0x9f,0xd1,0x18]
# DIS: B.DATR CUBE_M16, e8m0, Null, RTM
B.DATR CUBE_M16, e8m0, Null, RTM

# CHECK: B.DATR CUBE_M32, e8m0, Null // encoding: [0xa3,0x1e,0xd0,0x18]
# DIS: B.DATR CUBE_M32, e8m0, Null
B.DATR CUBE_M32, e8m0, Null

# The .normal-suffixed 4-field spelling round-trips too; the bare and
# suffixed spellings share the CmpMode=0/Sat=0/ByteId=0 defaults.
# CHECK: B.DATR CUBE_M32, e8m0, Null, RNE // encoding: [0xa3,0x9e,0xd0,0x18]
# DIS: B.DATR CUBE_M32, e8m0, Null, RNE
B.DATR CUBE_M32.normal, e8m0, Null, RNE

# CHECK: B.DATR CUBE_M16, BF16, Null, RNE // encoding: [0xa3,0x9f,0x50,0x18]
# DIS: B.DATR CUBE_M16, BF16, Null, RNE
B.DATR CUBE_M16.normal, BF16, Null, RNE

# The numeric spellings resolve to the same encodings (parse-level aliases
# only; no copy is injected).
# CHECK: B.DATR CUBE_M32, e8m0, Null, RTM // encoding: [0xa3,0x9e,0xd1,0x18]
B.DATR 29, e8m0, Null, RTM
# CHECK: B.DATR CUBE_M16, e8m0, Null // encoding: [0xa3,0x1f,0xd0,0x18]
B.DATR 31, e8m0, Null

# A weight-layout {Layout, DTYPE_NONE, PadValue} block under a TLOAD header
# (the TLOAD weight contract) also assembles through the 3-field alias.
# CHECK: B.DATR OHWI2NK.normal, Zero // encoding: [0x23,0x15,0xf0,0x01]
BSTART.TLSU TLOAD, FP32
B.DATR 10, DTYPE_NONE, Zero
