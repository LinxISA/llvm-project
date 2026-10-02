// RUN: %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang --target=linx64v5 -mlxbc -O2 -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=OBJ

using tile = float tile_size(128);

#define EW_IF(NAME, COND)                                                   \
  void NAME(tile &out, const tile &lhs, const tile &rhs) {                  \
  _Pragma("linx elementwise")                                               \
    for (unsigned i = 0; i < 128; ++i) {                                    \
      if (COND)                                                             \
        out[i] = lhs[i] + rhs[i];                                           \
      else                                                                  \
        out[i] = lhs[i] - rhs[i];                                           \
    }                                                                       \
  }

EW_IF(elementwise_if_gt, lhs[i] > 0.0f)
EW_IF(elementwise_if_lt, lhs[i] < 0.0f)
EW_IF(elementwise_if_ge, lhs[i] >= 0.0f)
EW_IF(elementwise_if_le, lhs[i] <= 0.0f)
EW_IF(elementwise_if_eq, lhs[i] == 0.0f)
EW_IF(elementwise_if_ne, lhs[i] != 0.0f)
EW_IF(elementwise_if_not_gt, !(lhs[i] > 0.0f))

// Each condition must preserve its CmpMode in TCMP and still feed both
// masked arithmetic arms.  The final case also checks predicate inversion.
// IR-LABEL: define{{.*}} @{{.*}}elementwise_if_gt
// IR: @llvm.linx.experimental.ew.tcmp{{.*}}i64 3)
// IR: @llvm.linx.experimental.ew.tadd.masked{{.*}}i64 0, i64 1)
// IR: @llvm.linx.experimental.ew.tsub.masked{{.*}}i64 1, i64 1)
// IR-LABEL: define{{.*}} @{{.*}}elementwise_if_lt
// IR: @llvm.linx.experimental.ew.tcmp{{.*}}i64 2)
// IR-LABEL: define{{.*}} @{{.*}}elementwise_if_ge
// IR: @llvm.linx.experimental.ew.tcmp{{.*}}i64 5)
// IR-LABEL: define{{.*}} @{{.*}}elementwise_if_le
// IR: @llvm.linx.experimental.ew.tcmp{{.*}}i64 4)
// IR-LABEL: define{{.*}} @{{.*}}elementwise_if_eq
// IR: @llvm.linx.experimental.ew.tcmp{{.*}}i64 0)
// IR-LABEL: define{{.*}} @{{.*}}elementwise_if_ne
// IR: @llvm.linx.experimental.ew.tcmp{{.*}}i64 1)
// IR-LABEL: define{{.*}} @{{.*}}elementwise_if_not_gt
// IR: @llvm.linx.experimental.ew.tcmp{{.*}}i64 3)
// IR: @llvm.linx.experimental.ew.tadd.masked{{.*}}i64 1, i64 1)
// IR: @llvm.linx.experimental.ew.tsub.masked{{.*}}i64 0, i64 1)
// OBJ: TCMP{{.*}}FP32
// OBJ: BSTART.TEPL TADD, FP32
// OBJ: BSTART.TEPL TSUB, FP32
