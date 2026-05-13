// RUN: llvm-mc %s -arch=linx64v5 | FileCheck %s --dump-input always -vv

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [t#1, u#2], last ->t<64KB>
// CHECK: B.IOR  [a1,a2,a3],[]
// CHECK: B.IOR  [a7],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI   13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, U#2, [a1, a2, a3, a7], ->T<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [t#1, u#2]  ->t<64KB>
// CHECK: B.IOTI  [t#3], last
// CHECK: B.IOR  [a1,a2,a3],[]
// CHECK: B.IOR  [a7],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI   13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, U#2, T#3, [a1, a2, a3, a7], ->T<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [t#1, u#2]  ->t<64KB>
// CHECK: B.IOTI  [t#3], last
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI   13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, U#2, T#3, ->T<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI   [t#1, u#2], last
// CHECK: B.IOR  [a1,a2,a3],[]
// CHECK: B.IOR  [a7],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI   13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, U#2, [a1, a2, a3, a7]

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI   [t#1, u#2], last
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI  13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, U#2

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [t#1, u#2], last  ->t<64KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI   13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, U#2, ->T<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI   [t#1], last
// CHECK: B.IOR  [a1,a2,a3],[]
// CHECK: B.IOR  [a7],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI   13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, [a1, a2, a3, a7]

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [t#1], last      ->t<64KB>
// CHECK: B.IOR  [a1,a2,a3],[]
// CHECK: B.IOR  [a7],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI    13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, [a1, a2, a3, a7], ->T<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI   [t#1], last
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI    13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [t#1], last      ->t<64KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI    13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, ->T<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [], last ->t<64KB>
// CHECK: B.IOR  [a0],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI    13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> [a0], ->T<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [], last ->t<64KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI    13,       ->lb1
// CHECK: C.B.DIM   sp,  ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> ->T<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI   [t#1, t#2] ->t<64KB>
// CHECK: B.IOTI   [u#2], last ->u<32KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, T#2, U#2, ->T<64KB>, U<32KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI   [t#1, t#2]  ->t<64KB>
// CHECK: B.IOTI   [], last ->u<32KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, T#2, ->T<64KB>, U<32KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI   [t#1] ->t<64KB>
// CHECK: B.IOTI   [], last ->u<32KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, ->T<64KB>, U<32KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI   [] ->t<64KB>
// CHECK: B.IOTI   [], last ->u<32KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> ->T<64KB>, U<32KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOTI  [t#1, t#2]     ->t<64KB>
// CHECK: B.IOTI  [t#3, t#4]     ->u<64KB>
// CHECK: B.IOTI  [u#1, u#2]     ->m<64KB>
// CHECK: B.IOTI  [u#3, u#4], last        ->n<64KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, T#2, T#3, T#4, U#1, U#2, U#3, U#4,->T<64KB>, U<64KB>, M<64KB>, N<64KB>

// CHECK: BSTART.VPAR      VS16
// CHECK: B.IOT  [t#1, t#2]     ->t<zero>
// CHECK: B.IOTI  [t#3, t#4]     ->u<64KB>
// CHECK: B.IOT  [u#1, u#2]     ->m<zero>
// CHECK: B.IOTI  [u#3, u#4], last        ->n<64KB>
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.TEXT  label
VPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, T#2, T#3, T#4, U#1, U#2, U#3, U#4,->T<ZERO>, U<64KB>, M<ZERO>, N<64KB>

// CHECK: BSTART.MPAR      VS16
// CHECK: B.IOTI   [t#1, u#2], last  ->s<8KB>
// CHECK: B.IOR   [a0],[]
// CHECK: B.DIM   a0, 12,         ->lb0
// CHECK: C.B.DIMI        13,     ->lb1
// CHECK: C.B.DIM sp,     ->lb2
// CHECK: B.TEXT  label
MPAR label, <M: R2+12, N: 13, K: R1, MR> T#1, U#2, [a0], ->S<8KB>
