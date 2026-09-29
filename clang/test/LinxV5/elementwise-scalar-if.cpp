// RUN: %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang --target=linx64v5 -mlxbc -O2 -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=OBJ

using tile = float tile_size(128);

void scalar_style_element_if(tile &out, const tile &lhs, const tile &rhs) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) {
    if (lhs[i] > 0.0f)
      out[i] = lhs[i] + rhs[i];
    else
      out[i] = lhs[i] - rhs[i];
  }
}

// IR-LABEL: define{{.*}} @_Z23scalar_style_element_if
// IR: call <128 x float> @llvm.linx.experimental.ew.texpands
// IR: call <128 x float> @llvm.linx.experimental.ew.tcmp
// IR-SAME: i64 3)
// IR: call <128 x float> @llvm.linx.experimental.ew.tadd.masked
// IR: call <128 x float> @llvm.linx.experimental.ew.tsub.masked
// IR: call <128 x float> @llvm.linx.experimental.ew.tsel
// OBJ: TEXPANDS
// OBJ: BSTART.TEPL TCMP, FP32
// OBJ: BSTART.TEPL TADD, FP32
// OBJ: BSTART.TEPL TSUB, FP32
// OBJ: TSEL
// OBJ: TSTORE
