// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not=extractelement --implicit-check-not=insertelement
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -mlxbc -O3 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -DINVALID_PRODUCER -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=INVALID
// CHECK-LABEL: define{{.*}} @two_phase_loop(
// CHECK: phi <32 x i32>
// CHECK: [[FIRST:%[^ ]+]] = call <32 x i32> asm sideeffect "BSTART.TEPL TADD, U32
// CHECK: [[TRANSFER:%[^ ]+]] = phi <32 x i32> [ [[FIRST]],
// CHECK: call <32 x i32> asm sideeffect "BSTART.TEPL TADD, U32
// CHECK-SAME: (<32 x i32> [[TRANSFER]],
// CHECK: br i1
// INVALID: PTO element region: predication: typed import requires a dominating Tile producer

using elements = unsigned tile_size(32);
__attribute__((annotate(
    "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline))
inline elements &view(elements &value) { return value; }
__attribute__((always_inline)) inline void
phase(elements &input, elements &output, unsigned bias) {
  auto &source = view(input);
  auto &dest = view(output);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    dest[element] = source[element] + bias;
}
extern "C" void two_phase_loop(unsigned batches) {
  elements first, second;
  asm volatile("BSTART.TEPL TEXPANDS, U32\n"
               "B.DATR CUBE_M32, DTYPE_NONE, Null\n"
               "B.DIM zero, 1, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\n"
               "B.IOR [%1], []\n"
               : "=Tr"(first) : "r"(7UL), "i"(1));
  for (unsigned batch = 0; batch < batches; ++batch) {
    phase(first, second, 1);
#ifdef INVALID_PRODUCER
    second = second + second;
#endif
    phase(second, first, 2);
  }
  asm volatile("" :: "Tr"(first));
}
