// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s

extern "C" void opaque();

extern "C" void m32_partial(void *source) {
  using Tile = float tile_size(128);
  Tile value;
  asm volatile("BSTART.TLSU TLOAD, %D2\n"
               "B.DATR ND2M32, DTYPE_NONE, Zero\n"
               "B.DIM zero, 3, ->lb0\n"
               "B.DIM zero, 17, ->lb1\n"
               "B.DIM zero, 1, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1], []\n"
               : "=Tr"(value) : "r"(source), "i"(1), "i"(3) : "memory");
  opaque();
  asm volatile("" : : "Tr"(value) : "memory");
}

// CHECK-LABEL: <m32_partial>:
// CHECK-NOT: S64
// CHECK: c.movi 12,
// CHECK: BSTART.TSTORE
// CHECK: B.DATR M322ND
// CHECK: B.IOR [{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
// CHECK: BSTART.TLOAD
// CHECK: B.DATR ND2M32
// CHECK: C.B.DIMI 3, ->lb0
// CHECK: C.B.DIMI 17, ->lb1
// CHECK: B.IOR [{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
// CHECK-NOT: S64

extern "C" void m16_partial(void *source) {
  using Tile = short tile_size(64);
  Tile value;
  asm volatile("BSTART.TLSU TLOAD, %D2\n"
               "B.DATR ND2M16, DTYPE_NONE, Zero\n"
               "B.DIM zero, 3, ->lb0\n"
               "B.DIM zero, 7, ->lb1\n"
               "B.DIM zero, 1, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1], []\n"
               : "=Tr"(value) : "r"(source), "i"(4), "i"(1) : "memory");
  opaque();
  asm volatile("" : : "Tr"(value) : "memory");
}

// CHECK-LABEL: <m16_partial>:
// CHECK-NOT: S64
// CHECK: c.movi 6,
// CHECK: BSTART.TSTORE
// CHECK: B.DATR M162ND
// CHECK: B.IOR [{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
// CHECK: BSTART.TLOAD
// CHECK: B.DATR ND2M16
// CHECK: C.B.DIMI 3, ->lb0
// CHECK: C.B.DIMI 7, ->lb1
// CHECK: B.IOR [{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
// CHECK-NOT: S64

extern "C" void norm_partial(void *source) {
  using Tile = unsigned short tile_size(64);
  Tile value;
  asm volatile("BSTART.TLSU TLOAD, %D2\n"
               "B.DIM zero, 3, ->lb0\n"
               "B.DIM zero, 3, ->lb1\n"
               "B.DIM zero, 8, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1], []\n"
               : "=Tr"(value) : "r"(source), "i"(26), "i"(1) : "memory");
  opaque();
  asm volatile("" : : "Tr"(value) : "memory");
}

// CHECK-LABEL: <norm_partial>:
// CHECK-NOT: S64
// CHECK: addi zero, 16,
// CHECK: BSTART.TSTORE
// CHECK: B.DATR NORM
// CHECK: B.IOR [{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
// CHECK: BSTART.TLOAD
// CHECK: C.B.DIMI 3, ->lb0
// CHECK: C.B.DIMI 3, ->lb1
// CHECK: C.B.DIMI 8, ->lb2
// CHECK: B.IOR [{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
// CHECK-NOT: S64
