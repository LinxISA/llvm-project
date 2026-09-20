# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o - \
# RUN:   | llvm-objdump -d --no-show-raw-insn -M no-tile-macros - | FileCheck %s
# Issue #101: the no-source B.IOT destination form (Func=6) requires
# SizeCode 1..10 per the ASL B.IOT constraint; SizeCode=0 is the
# source-only encoding and is a reserved/illegal combination. The backend
# PseudoEmptyTile placeholder (tile output-stack hand keeping) used to
# expand to B_IOT_NoSrc_Dst with TSize=0, emitting the illegal word
# 0x00086e13. It now carries a legal 128B SizeCode (0x0008ee13).

# CHECK: BSTART.VPAR VS16
# CHECK-NEXT: B.IOT mask=1111, last, ->t<128B>
BSTART.VPAR VS16
B.IOT mask=1111, last, ->t<128B>

# CHECK: BSTART.VPAR VS16
# CHECK-NEXT: B.IOT mask=1111, last, ->u<128B>
BSTART.VPAR VS16
B.IOT mask=1111, last, ->u<128B>
