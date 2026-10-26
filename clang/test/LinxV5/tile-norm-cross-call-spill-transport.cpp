// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s

using Elements = unsigned tile_size(32);

extern "C" void opaque();

extern "C" void norm_cross_call(unsigned *source, unsigned *destination) {
  Elements value;
  asm volatile("BSTART.TLOAD U32\n"
               "B.DIM zero, 1, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.DIM zero, 1, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\n"
               "B.IOR [%1], []\n"
               : "=Tr"(value) : "r"(source), "i"(1) : "memory");
  opaque();
  asm volatile("BSTART.TSTORE U32\n"
               "B.DIM zero, 1, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.DIM zero, 1, ->lb2\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1], []\n"
               : : "Tr"(value), "r"(destination) : "memory");
}

// CHECK-NOT: S64
// CHECK: BSTART.TSTORE U32
// CHECK: BSTART.TLOAD U32
// CHECK: C.B.DIMI 1, ->lb0
// CHECK: C.B.DIMI 32, ->lb1
// CHECK: C.B.DIMI 1, ->lb2
// CHECK-NOT: S64
