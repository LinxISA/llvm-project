// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s

using Tile = float __attribute__((ext_vector_type(128)));
extern "C" void opaque();

extern "C" void partial_cvt(Tile &input) {
  Tile output;
  asm volatile("BSTART.TEPL 27, %D1\n"
               "B.DATR %D2, RNONE\n"
               "B.DIM %5, 0, ->lb0\n"
               "B.DIM %6, 0, ->lb1\n"
               "B.IOT %3, mask=1111, last, ->%0<%Z4>\n"
               : "=Tr"(output)
               : "i"(4), "i"(1), "Tr"(input), "i"(3),
                 "r"(2), "r"(16), "i"(29) : "memory");
  opaque();
  asm volatile("BSTART.TSTORE FP32\n"
               "B.DATR M322ND, DTYPE_NONE, Zero\n"
               "B.IOT %0, mask=1111, last\n"
               : : "Tr"(output) : "memory");
}

// CHECK-LABEL: <partial_cvt>:
// CHECK: BSTART.TLSU TSTORE,{{.*}}S32
// CHECK: B.DATR{{.*}}M322ND
// CHECK: C.B.DIMI{{.*}}2,{{.*}}->lb0
// CHECK: C.B.DIMI{{.*}}16,{{.*}}->lb1
// CHECK-NOT: C.B.DIMI{{.*}}{{[2-9]}},{{.*}}->lb2
// CHECK: B.IOR{{.*}}[{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
// CHECK: BSTART.TLSU TLOAD,{{.*}}S32
// CHECK: B.DATR{{.*}}ND2M32
// CHECK: C.B.DIMI{{.*}}2,{{.*}}->lb0
// CHECK: C.B.DIMI{{.*}}16,{{.*}}->lb1
// CHECK-NOT: C.B.DIMI{{.*}}{{[2-9]}},{{.*}}->lb2
// CHECK: B.IOR{{.*}}[{{[a-z0-9]+}}, {{[a-z0-9]+}}], []
