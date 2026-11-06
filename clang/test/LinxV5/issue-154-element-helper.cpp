// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -DNOINLINE -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REJECT

using tile = unsigned tile_size(64);
__attribute__((always_inline, annotate("pto.element.view:v1;dtype=u32;rows=32;cols=2;layout=cube_m32")))
inline tile &view(tile &value) { return value; }

#ifdef NOINLINE
__attribute__((noinline))
#else
__attribute__((always_inline)) inline
#endif
void bump(tile &value) {
  auto &elements = view(value);
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element)
    elements[element] = elements[element] + 1;
}

// CHECK-LABEL: define{{.*}} @helper_owner(
// CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked.v64i32
// REJECT: PTO element region: P1a accessed views require distinct local Tile storage
extern "C" void helper_owner() {
  tile value;
  asm volatile("BSTART.TEPL TEXPANDS, U32\nB.DATR CUBE_M32, DTYPE_NONE, Null\nB.DIM zero, 2, ->lb0\nB.DIM zero, 32, ->lb1\nB.IOT mask=1111, last, ->%0<%Z2>\nB.IOR [%1], []" : "=Tr"(value) : "r"(5UL), "i"(2));
  bump(value);
  asm volatile("" :: "Tr"(value));
}
