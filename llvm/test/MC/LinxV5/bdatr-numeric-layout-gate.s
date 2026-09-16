# RUN: not llvm-mc -triple=linx64v5 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s
# PTO-ISA B.DATR legality: a numeric first operand resolves through the
# generic immediate parser, so the named-spelling gates in parseBArgFormat
# never see it. validateInstruction checks the resolved Layout value instead:
# unassigned codes reject, the weight layouts 10/11 are TLOAD-only, and the
# CUBE transport selectors follow the recorded BSTART direction.

BSTART.TLSU TSTORE, FP32
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 10, Zero
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 11, Zero
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 2, Zero
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 12, Zero
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 19, Zero
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 21, Zero

BSTART.TEPL 64, FP32
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 10, Zero

BSTART.TLSU TLOAD, FP32
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 5, Zero
# CHECK: [[@LINE+1]]:1: error: Match Instruction Invalid!
B.DATR 24, Zero
# Assigned codes that stay legal under the right header; the run fails only
# at the expected lines above.
B.DATR 10, Zero
B.DATR 11, Zero
B.DATR 21, Zero
B.DATR 29, Zero
B.DATR 0, Zero
