// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t.o
// RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.o | FileCheck %s

using Tile = unsigned tile_size(128);

extern "C" void atomic_masked(void *base, unsigned long stride) {
  Tile source;
  Tile mask;
  asm volatile("BSTART.TLOAD U32\n"
               "B.DATR ND2M32, DTYPE_NONE, Zero\n"
               "B.DIM zero, 4, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.DIM zero, 1, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1,%2], []\n"
               : "=Tr"(source) : "r"(base), "r"(stride), "i"(3) : "memory");
  asm volatile("BSTART.TEPL TCMP, U32\n"
               "B.DIM zero, 4, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %1, %1, mask=1111, last, ->%0<%Z2>\n"
               : "=Tr"(mask) : "Tr"(source), "i"(3) : "memory");
  asm volatile("BSTART.TLSU 1, U32\n"
               "B.DATR M322ND.canon, DTYPE_NONE, Zero, byte0, 7, RNONE, nosat, 1, 0\n"
               "B.DIM zero, 4, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.DIM zero, 1, ->lb2\n"
               "B.IOT %0, %1, mask=1111, last\n"
               "B.IOR [%2,%3], []\n"
               : : "Tr"(source), "Tr"(mask), "r"(base), "r"(stride)
               : "memory");
}

// CHECK-LABEL: <atomic_masked>:
// CHECK: BSTART.TLSU TSTORE,
// CHECK: e3f05c23{{.*}}B.DATR{{.*}}M322ND.canon, DTYPE_NONE, Zero, byte0, 7, RNONE, nosat, 1, 0
// CHECK: B.IOT{{.*}}t#{{[0-9]+}}, t#{{[0-9]+}}, mask=1111, last
// CHECK: B.IOR{{.*}}[{{[a-z0-9]+}}, {{[a-z0-9]+}}], []

extern "C" void shared_conversion(void *base, unsigned long stride) {
  Tile source;
  unsigned long shared;
  unsigned long offset = 4;
  asm volatile("BSTART.TLOAD U32\n"
               "B.DATR ND2M32, DTYPE_NONE, Zero\n"
               "B.DIM zero, 3, ->lb0\n"
               "B.DIM zero, 17, ->lb1\n"
               "B.DIM zero, 1, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1,%2], []\n"
               : "=Tr"(source) : "r"(base), "r"(stride), "i"(3) : "memory");
  asm volatile("BSTART.TLSU 2, U32\n"
               "B.DATR M322ND, DTYPE_NONE, Zero\n"
               "B.IOS mask=1111, ->%S0<%Z2>\n"
               "B.ASSEMBLE %c3, %c4, zero, 0, 3\n"
               "B.IOT %1, mask=1111, last\n"
               : "=Sr"(shared) : "Tr"(source), "i"(4), "i"(1), "i"(0) : "memory");
  asm volatile("BSTART.TMOV U32\n"
               "B.DATR M322ND, DTYPE_NONE, Zero\n"
               "B.IOS %S0, mask=1111\n"
               "B.ASSEMBLE %c3, %c4, %2, 0, 3\n"
               "B.IOT %1, mask=1111, last\n"
               : : "Sr"(shared), "Tr"(source), "r"(offset), "i"(0), "i"(1) : "memory");
  asm volatile("BSTART.TSTORE U32\n"
               "B.DATR NORM.canon, DTYPE_NONE, Zero, 6\n"
               "B.DIM zero, 3, ->lb0\n"
               "B.DIM zero, 49, ->lb1\n"
               "B.DIM zero, 4, ->lb2\n"
               "B.IOS %S0, mask=1111\n"
               "B.IOR [%1,%2], []\n"
               : : "Sr"(shared), "r"(base), "r"(stride) : "memory");
}

// CHECK-LABEL: <shared_conversion>:
// CHECK: BSTART.TLSU TMOV,
// CHECK: B.DATR{{.*}}M322ND.normal, Zero
// CHECK: B.IOS{{.*}}mask=1111, ->S{{[0-9]+}}<1KB>
// CHECK: B.ASSEMBLE{{.*}}1, 0, zero, 0, 3
// CHECK: B.IOT{{.*}}mask=1111, last
// CHECK: BSTART.TLSU TMOV,
// CHECK: B.DATR{{.*}}M322ND.normal, Zero
// CHECK: B.IOS{{.*}}S{{[0-9]+}}, mask=1111
// CHECK: B.ASSEMBLE{{.*}}0, 1, {{[a-z0-9]+}}, 0, 3
// CHECK: BSTART.TLSU TSTORE,
// CHECK: c3f01023{{.*}}B.DATR{{.*}}NORM.canon, Zero, 6
// CHECK: B.IOS{{.*}}S{{[0-9]+}}, mask=1111
// CHECK: B.IOR{{.*}}[{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
