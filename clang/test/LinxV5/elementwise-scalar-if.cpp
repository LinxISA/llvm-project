// RUN: %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang --target=linx64v5 -mlxbc -O2 -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=OBJ
// RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=RAW

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
// OBJ: TLOAD{{.*}}[base=a1, stride=a3]
// OBJ: TLOAD{{.*}}[base=a2, stride=a3]
// OBJ: TEXPANDS
// OBJ: CUBE_M16
// OBJ: TCMP{{.*}}FP32
// OBJ: BSTART.TEPL TADD, FP32
// OBJ: B.DATR CUBE_M16, DTYPE_NONE
// OBJ: BSTART.TEPL TSUB, FP32
// OBJ: B.DATR CUBE_M16, DTYPE_NONE
// OBJ: TSEL
// OBJ: TSTORE{{.*}}M162ND{{.*}}[base=a0, stride=a3]

// The TCMP macro printer folds its BSTART/DATR/DIM/B.IOT body into one line.
// Check the raw bundle as well: 0x61f01023 carries DTYPE_NONE in B.DATR and
// 0x823c is the explicit LB2=8 dimension between LB1 and the IOT binder.
// RAW: 08d19181 61f01023
// RAW-NEXT: 443c023c {{.*}}823c
