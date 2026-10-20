// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S -o - -DNONZERO %s | FileCheck %s --check-prefixes=GATHER,NONZERO --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S -o - -DWIDE %s | FileCheck %s --check-prefix=GATHER --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement

using elements = unsigned int tile_size(32);
__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

#ifdef WIDE
using valid_type = unsigned long long;
#else
using valid_type = unsigned;
#endif

void rejected_gather(elements *output_pointer, const elements *index_pointer,
                     const unsigned *table, valid_type valid) {
  elements indices;
  asm volatile("" : "=Tr"(indices));
  elements output{};
  auto &index_elements = element_view(indices);
  auto &output_elements = element_view(output);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (element < valid)
      output_elements[element] = table[index_elements[element]];
    else
#ifdef NONZERO
      output_elements[element] = 9u;
#else
      output_elements[element] = 0u;
#endif
  }
  asm volatile("" : : "Tr"(output));
}

// GATHER-LABEL: define{{.*}}rejected_gather
// GATHER: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
// NONZERO: freeze i32 9
// GATHER: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr
// GATHER-NOT: extractelement
// GATHER-NOT: insertelement
// GATHER-NOT: call void @llvm.linx.experimental.element.region
// GATHER: ret void
