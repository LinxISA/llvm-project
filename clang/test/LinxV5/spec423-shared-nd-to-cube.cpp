// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -S -mllvm -stop-after=linxv5-tile-fixup %s -o - | FileCheck %s --check-prefix=DESC
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s

// Keep Shared handles inside one SSA lifetime; a Shared physical ABI is not
// implied by these tests. The reverse TMOV must preserve backing dtype.
using M32 = unsigned tile_size(128);
using M16 = unsigned short tile_size(256);

extern "C" void opaque();

static __attribute__((always_inline)) unsigned long shared_nd(M32 &input) {
  unsigned long shared;
  asm volatile("BSTART.TMOV U32\n"
               "B.DATR M322ND, DTYPE_NONE, Zero\n"
               "B.IOS mask=1111, ->%S0<512B>\n"
               "B.IOT %1, mask=1111, last\n"
               : "=Sr"(shared) : "Tr"(input) : "memory");
  return shared;
}

extern "C" void whole_m32(M32 &input) {
  unsigned long shared = shared_nd(input);
  M32 output;
  asm volatile("BSTART.TMOV DTYPE_NONE\n"
               "B.DATR ND2M32, DTYPE_NONE, Zero\n"
               "B.DIM zero, 3, ->lb0\n"
               "B.DIM zero, 19, ->lb1\n"
               "B.IOS %S1, mask=1111\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\n"
               : "=Tr"(output) : "Sr"(shared), "i"(3) : "memory");
  asm volatile("B.IOT %0, mask=1111, last" : : "Tr"(output));
}

// CHECK-LABEL: <whole_m32>:
// CHECK: B.DATR{{.*}}M322ND.normal, Zero
// CHECK: BSTART.TMOV{{.*}}DTYPE_NONE
// CHECK-NEXT: {{.*}}B.DATR{{.*}}ND2M32.normal, Zero
// CHECK: ->lb0
// CHECK: ->lb1
// CHECK-NOT: ->lb2
// CHECK: B.IOS{{.*}}S{{[0-9]+}}{{(.reuse)?}}, mask=1111
// CHECK-NEXT: {{.*}}B.IOT{{.*}}mask=1111, last, ->t<512B>

extern "C" void subview_m16(M16 &input, unsigned long base,
                             unsigned columns, unsigned rows) {
  unsigned long shared;
  M16 output;
  asm volatile("BSTART.TMOV U16\n"
               "B.DATR M162ND, DTYPE_NONE, Zero\n"
               "B.IOS mask=1111, ->%S0<1KB>\n"
               "B.ASSEMBLE %c2, %c2, zero, 0, 3\n"
               "B.IOT %1, mask=1111, last\n"
               : "=Sr"(shared) : "Tr"(input), "i"(1) : "memory");
  asm volatile("BSTART.TMOV DTYPE_NONE\n"
               "B.DATR ND2M16, DTYPE_NONE, Zero\n"
               "B.DIM %3, 0, ->lb0\n"
               "B.DIM %4, 0, ->lb1\n"
               "B.IOS %S1, mask=1111\n"
               "B.SUBVIEW 0, %2, 0, 3\n"
               "B.IOT mask=1111, last, ->%0<%Z5>\n"
               : "=Tr"(output)
               : "Sr"(shared), "r"(base), "r"(columns), "r"(rows), "i"(3)
               : "memory");
  asm volatile("B.IOT %0, mask=1111, last" : : "Tr"(output));
}

// CHECK-LABEL: <subview_m16>:
// CHECK: B.DATR{{.*}}M162ND.normal, Zero
// CHECK: BSTART.TMOV{{.*}}DTYPE_NONE
// CHECK-NEXT: {{.*}}B.DATR{{.*}}ND2M16.normal, Zero
// CHECK: B.IOS{{.*}}S{{[0-9]+}}{{(.reuse)?}}, mask=1111
// CHECK-NEXT: {{.*}}B.SUBVIEW{{.*}}0, {{[a-z0-9]+}}, 0, 3
// CHECK-NEXT: {{.*}}B.IOT{{.*}}mask=1111, last, ->t<512B>

extern "C" void static_subview_m32(M32 &input) {
  unsigned long shared;
  M32 output;
  asm volatile("BSTART.TMOV U32\n"
               "B.DATR M322ND, DTYPE_NONE, Zero\n"
               "B.IOS mask=1111, ->%S0<1KB>\n"
               "B.ASSEMBLE %c2, %c2, zero, 0, 3\n"
               "B.IOT %1, mask=1111, last\n"
               : "=Sr"(shared) : "Tr"(input), "i"(1) : "memory");
  asm volatile("BSTART.TMOV DTYPE_NONE\n"
               "B.DATR ND2M32, DTYPE_NONE, Zero\n"
               "B.DIM zero, 4, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOS %S1, mask=1000\n"
               "B.SUBVIEW 0, zero, 4, 3\n"
               "B.IOT mask=1000, last, ->%0<%Z2>\n"
               : "=Tr"(output) : "Sr"(shared), "i"(3) : "memory");
  asm volatile("B.IOT %0, mask=1000, last" : : "Tr"(output));
}

// CHECK-LABEL: <static_subview_m32>:
// CHECK: BSTART.TMOV{{.*}}DTYPE_NONE
// CHECK-NEXT: {{.*}}B.DATR{{.*}}ND2M32.normal, Zero
// CHECK: B.IOS{{.*}}S{{[0-9]+}}{{(.reuse)?}}, mask=1000
// CHECK-NEXT: {{.*}}B.SUBVIEW{{.*}}0, zero, 4, 3
// CHECK-NEXT: {{.*}}B.IOT{{.*}}mask=1000, last, ->t<512B>

extern "C" void cross_call_m32() {
  M32 input;
  asm volatile("BSTART.TEPL TEXPANDS, U32\n"
               "B.DATR CUBE_M32, Zero\n"
               "B.DIM zero, 4, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z1>\n"
               : "=Tr"(input) : "i"(3) : "memory");
  unsigned long shared;
  asm volatile("BSTART.TMOV DTYPE_NONE\n"
               "B.DATR M322ND, DTYPE_NONE, Zero\n"
               "B.IOS mask=1111, ->%S0<512B>\n"
               "B.IOT %1, mask=1111, last\n"
               : "=Sr"(shared) : "Tr"(input) : "memory");
  M32 output;
  asm volatile("BSTART.TMOV DTYPE_NONE\n"
               "B.DATR ND2M32, DTYPE_NONE, Zero\n"
               "B.DIM zero, 3, ->lb0\n"
               "B.DIM zero, 19, ->lb1\n"
               "B.IOS %S1, mask=1111\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\n"
               : "=Tr"(output) : "Sr"(shared), "i"(3) : "memory");
  opaque();
  asm volatile("B.IOT %0, mask=1111, last" : : "Tr"(output));
}

// The 0.59 descriptor spill preserves the valid rectangle and the reload
// restores its CUBE descriptor for subsequent typed copies/consumers.
// CHECK-LABEL: <cross_call_m32>:
// CHECK: BSTART.TMOV{{.*}}DTYPE_NONE
// CHECK: B.DATR{{.*}}ND2M32.normal, Zero
// CHECK: BSTART.TSTORE{{.*}}U32
// CHECK: B.DATR{{.*}}M322ND.normal, Null
// CHECK: C.B.DIMI{{.*}}3,{{.*}}->lb0
// CHECK: C.B.DIMI{{.*}}19,{{.*}}->lb1
// CHECK: BSTART.TLOAD{{.*}}U32
// CHECK: B.DATR{{.*}}ND2M32.normal, Zero
// CHECK: C.B.DIMI{{.*}}3,{{.*}}->lb0
// CHECK: C.B.DIMI{{.*}}19,{{.*}}->lb1
// DESC-LABEL: name:{{ *}}cross_call_m32
// DESC: PseudoTSpillDesc{{.*}}, 3, 19, 4, 25, 29, 3
// DESC: PseudoTReloadDesc{{.*}}, 3, 19, 4, 25, 29, 3

extern "C" void cross_call_m16() {
  M16 input;
  asm volatile("BSTART.TEPL TEXPANDS, U16\n"
               "B.DATR CUBE_M16, Zero\n"
               "B.DIM zero, 16, ->lb0\n"
               "B.DIM zero, 16, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z1>\n"
               : "=Tr"(input) : "i"(3) : "memory");
  unsigned long shared;
  asm volatile("BSTART.TMOV DTYPE_NONE\n"
               "B.DATR M162ND, DTYPE_NONE, Zero\n"
               "B.IOS mask=1111, ->%S0<512B>\n"
               "B.IOT %1, mask=1111, last\n"
               : "=Sr"(shared) : "Tr"(input) : "memory");
  M16 output;
  asm volatile("BSTART.TMOV DTYPE_NONE\n"
               "B.DATR ND2M16, DTYPE_NONE, Zero\n"
               "B.DIM zero, 13, ->lb0\n"
               "B.DIM zero, 11, ->lb1\n"
               "B.IOS %S1, mask=1111\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\n"
               : "=Tr"(output) : "Sr"(shared), "i"(3) : "memory");
  opaque();
  asm volatile("B.IOT %0, mask=1111, last" : : "Tr"(output));
}

// CHECK-LABEL: <cross_call_m16>:
// CHECK: BSTART.TMOV{{.*}}DTYPE_NONE
// CHECK: B.DATR{{.*}}ND2M16.normal, Zero
// CHECK: BSTART.TSTORE{{.*}}U16
// CHECK: BSTART.TLOAD{{.*}}U16
// DESC-LABEL: name:{{ *}}cross_call_m16
// DESC: PseudoTSpillDesc{{.*}}, 13, 11, 16, 26, 31, 3
// DESC: PseudoTReloadDesc{{.*}}, 13, 11, 16, 26, 31, 3
