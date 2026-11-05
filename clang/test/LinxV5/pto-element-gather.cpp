// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

// This is the carrier exposed by ElementTile<uint32_t, 32> through the public
// TPARTELEMENT API. The local tiles model the producer/view boundary used by
// the public API; the pragma body remains ordinary scalar-looking C++.
using element_part = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline element_part &
element_view(element_part &part) {
  return part;
}

void gather(element_part &out_part, element_part &index_part,
            const unsigned int *__restrict source, unsigned int valid,
            bool active) {
  element_part local_indices = index_part;
  element_part local_output{};
  auto &indices = element_view(local_indices);
  auto &out = element_view(local_output);
#pragma pto element for
  for (unsigned int element = 0; element < 32; ++element) {
    if (active && element < valid)
      out[element] = source[indices[element]];
    else
      out[element] = 0;
  }
  out_part = local_output;
}

// IR-LABEL: define{{.*}}gather
// IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// IR: call <32 x i64> @llvm.linx.experimental.ew.tlea
// IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
// IR-SAME: i64 0, i64 0, i64 1)
