// RUN: %clang --target=linx64-unknown-linux-gnu -march=linx64+pto-v0591-scoped -c %s -o %t
// RUN: llvm-readobj -r %t | FileCheck %s

        .text
        C.BSTOP
        lw.pcr.private [external], ->a0
        sw.pcr.intra a1, [external]
        hl.lw.pcr.inter [external], ->a2
        hl.sw.pcr.private a3, [external]

// CHECK: 0x2 R_LINX_SCOPED_PCR17_LOAD external 0x0
// CHECK: 0xA R_LINX_SCOPED_PCR17_STORE external 0x0
// CHECK: 0x12 R_LINX_SCOPED_HL_PCR29_LOAD external 0x0
// CHECK: 0x1A R_LINX_SCOPED_HL_PCR29_STORE external 0x0
