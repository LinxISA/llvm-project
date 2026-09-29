// RUN: %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang --target=linx64v5 -mlxbc -O2 -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=OBJ

using tile = float tile_size(128);

void scalar_style_tadd(tile &out, const tile &lhs, const tile &rhs) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i)
    out[i] = lhs[i] + rhs[i];
}

// IR-LABEL: define{{.*}} @_Z17scalar_style_tadd
// IR: call <128 x float> @llvm.linx.experimental.ew.tadd.masked.v128f32.v128f32.v128f32
// IR-SAME: i64 16, i64 8, i64 1, i64 31
// IR-SAME: i64 -1, i64 -1, i64 0, i64 1
// OBJ-LABEL: <_Z17scalar_style_taddRDv128_fRKS_S2_>:
// OBJ: BSTART.TEPL TADD, FP32
// OBJ: B.DATR CUBE_M16, FP32, Zero
// OBJ: B.IOR [a1,a1,zero], ExecMaskPresent
