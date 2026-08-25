# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s --check-prefix=DIS

# PTO-ISA 0.58.4 range modifiers (ADR-0098, pto-spec 564ac2d):
#   B.SUBVIEW  SrcSelect, RegSrc, uimm11, SubviewSizeCode    (match 0x53)
#   B.ASSEMBLE INIT, LAST, RegSrc, uimm11, ParentSizeCode    (match 0x1053)
# Legal contracts covered here:
#   SubviewSizeCode 1..12
#   ParentSizeCode  0..12, with INIT=1 -> 1..12 and INIT=0 -> 0
#   RegSrc is an absolute GPR selector 0..23 (zero/R0, a0/R2, ..., r23/R23;
#   the disassembler prints the first alias, e.g. r23 -> x3)
#   uimm11 0..2047
#
# Each modifier attaches to the immediately preceding B.IOT binder in the
# same contiguous block-command group (ADR-0098 binder contract). The MC
# layer round-trips each word; binder-group adjacency beyond immediacy is
# enforced at the TileOP layer, so here the modifiers appear right after
# their binder.

.text
# --- B.SUBVIEW over a B.IOT destination binder (RegSrc boundaries) ---
B.IOT mask=1111, ->t<1KB>
B.SUBVIEW 0, zero, 0, 1
B.SUBVIEW 1, a0, 2047, 12
B.SUBVIEW 0, a1, 100, 5

# --- B.SUBVIEW over a B.IOT binder, RegSrc=23 boundary ---
B.IOT mask=1111, ->t<1KB>
B.SUBVIEW 0, r23, 100, 5

# --- B.ASSEMBLE: INIT/INIT_LAST forms over a B.IOT destination binder ---
B.IOT mask=1111, last, ->t<1KB>
B.ASSEMBLE 1, 0, zero, 0, 1
B.ASSEMBLE 1, 1, a0, 2047, 12
B.ASSEMBLE 1, 0, a1, 1, 1

# --- B.ASSEMBLE: MIDDLE/LAST forms (INIT=0, ParentSizeCode=0) ---
B.IOT mask=1111, last, ->t<1KB>
B.ASSEMBLE 0, 0, r23, 1, 0
B.ASSEMBLE 0, 1, a1, 100, 0

# ENC: B.IOT{{.*}}mask=1111,{{.*}}->t<1KB>
# ENC: B.SUBVIEW{{.*}}0, zero, 0, 1
# ENC: B.SUBVIEW{{.*}}1, a0, 2047, 12
# ENC: B.SUBVIEW{{.*}}0, a1, 100, 5
# ENC: B.IOT{{.*}}mask=1111,{{.*}}->t<1KB>
# ENC: B.SUBVIEW{{.*}}0, x3, 100, 5
# ENC: B.IOT{{.*}}mask=1111, last,{{.*}}->t<1KB>
# ENC: B.ASSEMBLE{{.*}}1, 0, zero, 0, 1
# ENC: B.ASSEMBLE{{.*}}1, 1, a0, 2047, 12
# ENC: B.ASSEMBLE{{.*}}1, 0, a1, 1, 1
# ENC: B.IOT{{.*}}mask=1111, last,{{.*}}->t<1KB>
# ENC: B.ASSEMBLE{{.*}}0, 0, x3, 1, 0
# ENC: B.ASSEMBLE{{.*}}0, 1, a1, 100, 0

# DIS: B.IOT{{.*}}mask=1111,{{.*}}->t<1KB>
# DIS: B.SUBVIEW{{.*}}0, zero, 0, 1
# DIS: B.SUBVIEW{{.*}}1, a0, 2047, 12
# DIS: B.SUBVIEW{{.*}}0, a1, 100, 5
# DIS: B.IOT{{.*}}mask=1111,{{.*}}->t<1KB>
# DIS: B.SUBVIEW{{.*}}0, x3, 100, 5
# DIS: B.IOT{{.*}}mask=1111, last,{{.*}}->t<1KB>
# DIS: B.ASSEMBLE{{.*}}1, 0, zero, 0, 1
# DIS: B.ASSEMBLE{{.*}}1, 1, a0, 2047, 12
# DIS: B.ASSEMBLE{{.*}}1, 0, a1, 1, 1
# DIS: B.IOT{{.*}}mask=1111, last,{{.*}}->t<1KB>
# DIS: B.ASSEMBLE{{.*}}0, 0, x3, 1, 0
# DIS: B.ASSEMBLE{{.*}}0, 1, a1, 100, 0