// RUN: llvm-mc %s --triple=linx64v5 --show-encoding | FileCheck %s --dump-input always -vv

// CHECK: BSTART.TLSU      MGATHER.MASK, FP32
// CHECK: B.DATR   NORM.normal, Zero
// CHECK: C.B.DIMI        16,     ->lb0
// CHECK: C.B.DIMI        8,      ->lb1
// CHECK: B.IOT   t#1, t#2, mask=1111, last,  ->t<64KB>
// CHECK: B.IOR [a0], []
BSTART.TLSU MGATHER.MASK, FP32
B.DATR Zero
B.DIM zero, 16, ->LB0
B.DIM zero, 8, ->LB1
B.IOT t#1, t#2, mask=1111, last, ->T<64KB>
B.IOR [a0],[]

// CHECK: BSTART.GMOV             U8
// CHECK: B.IOT   t#1, mask=1111, ->t<64KB>
// CHECK: B.IOR [], []
BSTART.GMOV U8
B.IOT t#1, mask=1111, ->T<64KB>
B.IOR [zero], []

// CHECK: BSTART.TLSU      MSCATTER.MASK, FP32
// CHECK: C.B.DIMI        16,     ->lb0
// CHECK: C.B.DIMI        8,      ->lb1
// CHECK: B.IOT   t#1, t#2, mask=1111
// CHECK: B.IOT   t#3, mask=1111, last
// CHECK: B.IOR [a0], []
BSTART.TLSU MSCATTER.MASK, FP32
B.DIM zero, 16, ->LB0
B.DIM zero, 8, ->LB1
B.IOT t#1, t#2, mask=1111
B.IOT t#3, mask=1111, last
B.IOR [a0],[]
