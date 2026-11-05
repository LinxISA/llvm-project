# RUN: not llvm-mc -triple=linx64v5 -show-encoding %s 2>&1 | FileCheck %s
# Short CAS has no far selector; store-only AMOs have only release ordering.
# CHECK-COUNT-16: error:
casb.f [a0], a1, a2, ->a3
casd.aqf [a0], a1, a2, ->a3
sw.add.f [a0], a1
sd.add.rlf [a0], a1
sb.add.aq [a0], a1
sh.sub.aqrl [a0], a1
sw.sub.f [a0], a1
sd.sub.aq [a0], a1
prf [a0, a1.neg]
prf [a0, a1<<32]
prfi.u [a0, -2049]
prfi.u [a0, 2048]
dma [a0], a1, ->a2
casw [a0], a1, ->a3
sync.core 0
bwt
