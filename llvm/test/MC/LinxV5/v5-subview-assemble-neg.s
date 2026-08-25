# RUN: not llvm-mc -triple=linx64v5 -show-encoding %s 2>&1 | FileCheck %s

# PTO-ISA ADR-0098 negative cases. Each line is rejected by the assembler
# (fail closed), so the output echoes the offending line instead of a
# replacement encoding.
#
# Covered contracts (active ASL BundleRangeSub*RawLegal):
#   SubviewSizeCode must be 1..12 (0, 13..15 reserved)
#   ParentSizeCode  must be 0..12 (13..15 reserved)
#   INIT=1 requires ParentSizeCode 1..12 (INIT/INIT_LAST forms)
#   INIT=0 requires ParentSizeCode 0     (MIDDLE/LAST forms)
#   RegSrc is an absolute GPR selector 0..23 (r24 and t#1 not GPR)
#   uimm11 must be 0..2047

# --- B.SUBVIEW size-code boundaries ---
# CHECK: B.SUBVIEW{{.*}}0, r0, 0, 0
B.SUBVIEW 0, r0, 0, 0
# CHECK: B.SUBVIEW{{.*}}0, r0, 0, 13
B.SUBVIEW 0, r0, 0, 13
# CHECK: B.SUBVIEW{{.*}}0, r0, 0, 15
B.SUBVIEW 0, r0, 0, 15
# CHECK: B.SUBVIEW{{.*}}0, r0, 2048, 1
B.SUBVIEW 0, r0, 2048, 1

# --- B.SUBVIEW RegSrc outside the GPR selector range ---
# CHECK: B.SUBVIEW{{.*}}0, r24, 0, 1
B.SUBVIEW 0, r24, 0, 1
# CHECK: B.SUBVIEW{{.*}}0, t#1, 0, 1
B.SUBVIEW 0, t#1, 0, 1

# --- B.ASSEMBLE reserved ParentSizeCode 13..15 ---
# CHECK: B.ASSEMBLE{{.*}}1, 0, r0, 0, 13
B.ASSEMBLE 1, 0, r0, 0, 13
# CHECK: B.ASSEMBLE{{.*}}1, 0, r0, 0, 15
B.ASSEMBLE 1, 0, r0, 0, 15
# CHECK: B.ASSEMBLE{{.*}}0, 1, r0, 0, 14
B.ASSEMBLE 0, 1, r0, 0, 14

# --- B.ASSEMBLE INIT/ParentSizeCode contradictory combinations ---
# CHECK: B.ASSEMBLE{{.*}}1, 0, r0, 0, 0
B.ASSEMBLE 1, 0, r0, 0, 0
# CHECK: B.ASSEMBLE{{.*}}0, 0, r0, 0, 1
B.ASSEMBLE 0, 0, r0, 0, 1
# CHECK: B.ASSEMBLE{{.*}}0, 1, r0, 0, 12
B.ASSEMBLE 0, 1, r0, 0, 12
# CHECK: B.ASSEMBLE{{.*}}1, 1, r0, 0, 0
B.ASSEMBLE 1, 1, r0, 0, 0

# --- B.ASSEMBLE uimm11 > 2047 ---
# CHECK: B.ASSEMBLE{{.*}}0, 1, r0, 2048, 0
B.ASSEMBLE 0, 1, r0, 2048, 0
# CHECK: B.ASSEMBLE{{.*}}1, 0, r0, 2048, 12
B.ASSEMBLE 1, 0, r0, 2048, 12

# --- B.ASSEMBLE RegSrc outside the GPR selector range ---
# CHECK: B.ASSEMBLE{{.*}}1, 0, r24, 0, 12
B.ASSEMBLE 1, 0, r24, 0, 12
# CHECK: B.ASSEMBLE{{.*}}1, 0, u#1, 0, 12
B.ASSEMBLE 1, 0, u#1, 0, 12