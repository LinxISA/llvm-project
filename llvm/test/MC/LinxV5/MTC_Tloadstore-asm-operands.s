// RUN: llvm-mc %s --triple=linx64v5 --show-encoding | FileCheck %s --dump-input always -vv

// CHECK: BSTART.TMA      TLOAD, FP32
// CHECK: B.ATTR   ZZ2ZN
// CHECK: B.IOTI  [], last  ->t<64KB>
// CHECK: B.IOR   [a1,a2,a3],[]
// CHECK: B.IOR   [a7],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
TLOAD.Zz2Zn <LB0: R2+12, LB1: 13, LB2: R1, FP32, Zero> [a1, a2, a3, a7], ->T<64KB>

// CHECK: BSTART.TMA      TSTORE, FP32
// CHECK: B.ATTR   ZZ2ZN
// CHECK: B.IOT   [t#1], last
// CHECK: B.IOR   [a1,a2,a3],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.IOD , ->d
TSTORE.Zz2Zn <LB0: R2+12, LB1: 13, LB2: R1, FP32> T#1, [a1, a2, a3], ->d

// CHECK: BSTART.TMA      TLOAD, FP32
// CHECK: B.ATTR   ZZ2ZN
// CHECK: B.IOTI  [], last  ->t<64KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.IOD d#4
TLOAD.Zz2Zn <LB0: R2+12, LB1: 13, LB2: R1, FP32, Zero> [], d#4, ->T<64KB>

// CHECK: BSTART.TMA      TSTORE, FP32
// CHECK: B.ATTR   ZZ2ZN
// CHECK: B.IOT   [u#1], last
// CHECK: B.IOR   [a1,a2,a3],[]
// CHECK: B.IOR   [a7],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.IOD d#8, ->d
TSTORE.Zz2Zn <LB0: R2+12, LB1: 13, LB2: R1, FP32>, U#1, [a1, a2, a3, a7], d#8, ->d

// CHECK: BSTART.TMA      TLOAD, FP32
// CHECK: B.ATTR   NORM.normal, Max
// CHECK: B.IOTI  [], last  ->t<64KB>
// CHECK: B.IOR   [a1,a2,a3],[]
// CHECK: B.IOR   [a7],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
TLOAD.NORM <LB0: R2+12, LB1: 13, LB2: R1, FP32, Max> [a1, a2, a3, a7], ->T<64KB>

// CHECK: BSTART.TMA      TSTORE, FP32
// CHECK: B.ATTR   NORM
// CHECK: B.IOT   [t#1], last
// CHECK: B.IOR   [a1,a2,a3],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
TSTORE.NORM <LB0: R2+12, LB1: 13, LB2: R1, FP32> T#1, [a1, a2, a3]