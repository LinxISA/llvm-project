// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DMARKED %s 2>&1 | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DMARKED_F16_WRONG %s 2>&1 | FileCheck %s --check-prefix=F16-WRONG

using s32_elements = int tile_size(32);
using f32_elements = float tile_size(32);

__attribute__((annotate(
    "pto.element.view:v1;dtype=s32;rows=32;cols=1;layout=cube_m32")))
s32_elements &typed_view(s32_elements &value);
__attribute__((annotate(
    "pto.element.view:v1;dtype=f32;rows=32;cols=1;layout=cube_m32")))
f32_elements &typed_view(f32_elements &value);

void ordinary_s32(s32_elements &value) {
  auto &elements = typed_view(value);
  elements[0] = elements[0];
}

void ordinary_f32(f32_elements &value) {
  auto &elements = typed_view(value);
  elements[0] = elements[0];
}

#ifdef MARKED
using f64_elements = double tile_size(32);
__attribute__((annotate(
    "pto.element.view:v1;dtype=f64;rows=32;cols=1;layout=cube_m32")))
f64_elements &typed_view(f64_elements &value);
void rejected_marked_f64(f64_elements &input, f64_elements &output) {
  auto &in = typed_view(input);
  auto &out = typed_view(output);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    out[element] = in[element];
}
// CHECK: PTO element region: unsupported logical view contract 'pto.element.view:v1;dtype=f64;rows=32;cols=1;layout=cube_m32'
#endif

#ifdef MARKED_F16_WRONG
using f16_elements = _Float16 tile_size(64);
__attribute__((annotate(
    "pto.element.view:v1;dtype=f16;rows=32;cols=1;layout=cube_m32")))
f16_elements &f16_wrong_view(f16_elements &value);
void rejected_f16_shape(f16_elements &input, f16_elements &output) {
  auto &in = f16_wrong_view(input);
  auto &out = f16_wrong_view(output);
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element)
    out[element] = in[element];
}
// F16-WRONG: PTO element region: unsupported logical view contract 'pto.element.view:v1;dtype=f16;rows=32;cols=1;layout=cube_m32'
#endif

