# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d --no-show-raw-insn -M no-tile-macros - | FileCheck %s
# Issue #102: the backend PseudoEmptyTile placeholder (tile output-stack hand
# keeping, SlotCalc::BuildCopy with TR == NoRegister) used to expand to the
# reserved BSTART.VPAR header (encoding-ownership.asl: owner "PTO reserved
# two-level vector extension space", formal review outcome RESERVED). It now
# lowers as a legal TLOAD transport: BSTART.TLSU TLOAD (omitted B.IOR
# supplies GM base zero), one required C.B.DIMI LB0=ValidCol=1, and the same
# no-source destination B.IOT with a legal SizeCode (issue #101).

# CHECK: BSTART.TLSU TLOAD, U8
# CHECK-NEXT: C.B.DIMI 1, ->lb0
# CHECK-NEXT: B.IOT mask=1111, last, ->t<128B>
# CHECK-NEXT: BSTOP
BSTART.TLSU TLOAD, U8
C.B.DIMI 1, ->lb0
B.IOT mask=1111, last, ->t<128B>
BSTOP

# CHECK: BSTART.TLSU TLOAD, U8
# CHECK-NEXT: C.B.DIMI 1, ->lb0
# CHECK-NEXT: B.IOT mask=1111, last, ->u<128B>
# CHECK-NEXT: BSTOP
BSTART.TLSU TLOAD, U8
C.B.DIMI 1, ->lb0
B.IOT mask=1111, last, ->u<128B>
BSTOP
