// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -mlxbc -O3 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o

using tile32 = unsigned tile_size(32);
using tile64 = unsigned tile_size(64);

__attribute__((always_inline, annotate("pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32")))
inline tile32 &view(tile32 &value) { return value; }
__attribute__((always_inline, annotate("pto.element.view:v1;dtype=u32;rows=32;cols=2;layout=cube_m32")))
inline tile64 &view(tile64 &value) { return value; }

// CHECK-LABEL: define{{.*}} @predicate32(
// CHECK: @llvm.linx.experimental.ew.tsel.gpr.v32i32(i64 32, i64 1, i64 25, i64 29,
extern "C" void predicate32(unsigned limit) {
  tile32 input, output;
  asm volatile("BSTART.TEPL TEXPANDS, U32\nB.DATR CUBE_M32, DTYPE_NONE, Null\nB.DIM zero, 1, ->lb0\nB.DIM zero, 32, ->lb1\nB.IOT mask=1111, last, ->%0<%Z2>\nB.IOR [%1], []" : "=Tr"(input) : "r"(5UL), "i"(1));
  auto &source = view(input);
  auto &dest = view(output);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    dest[element] = element < limit ? unsigned(source[element] != 0) : 7u;
  asm volatile("" :: "Tr"(output));
}

// CHECK-LABEL: define{{.*}} @predicate64(
// CHECK: @llvm.linx.experimental.ew.tsel.gpr.v64i32(i64 32, i64 2, i64 25, i64 29,
extern "C" void predicate64() {
  tile64 input, output;
  asm volatile("BSTART.TEPL TEXPANDS, U32\nB.DATR CUBE_M32, DTYPE_NONE, Null\nB.DIM zero, 2, ->lb0\nB.DIM zero, 32, ->lb1\nB.IOT mask=1111, last, ->%0<%Z2>\nB.IOR [%1], []" : "=Tr"(input) : "r"(5UL), "i"(2));
  auto &source = view(input);
  auto &dest = view(output);
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element)
    dest[element] = source[element] != 0;
  asm volatile("" :: "Tr"(output));
}
