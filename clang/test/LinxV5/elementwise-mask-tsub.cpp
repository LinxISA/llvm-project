// RUN: %clang++ --target=linx64v5 -O2 -mlxbc -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -O2 -mlxbc -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=OBJ

using tile = float tile_size(128);

// IR: call <128 x float> @llvm.linx.experimental.ew.tsub.masked.v128f32.v128f32.v128f32.v128f32(i64 16, i64 16, i64 1, i64 31
// OBJ-LABEL: <_Z23elementwise_masked_tsubPfS_S_>:
// OBJ: BSTART.TEPL TSUB, FP32
// OBJ-NEXT: B.DATR CUBE_M16, FP32, Zero, byte0, Eq, RNONE, nosat, 0, 1
// OBJ: B.IOT
// OBJ: B.IOT
// OBJ-NOT: ExecMaskPresent
void elementwise_masked_tsub(float *lhs_ptr, float *rhs_ptr,
                             float *predicate_ptr) {
  tile lhs;
  tile rhs;
  tile predicate;
  tile result;
  blk_tload(16, 1, 1, 1, 3, 4, lhs, lhs_ptr, 16);
  blk_tload(16, 1, 1, 1, 3, 4, rhs, rhs_ptr, 16);
  blk_tload(16, 1, 1, 1, 3, 4, predicate, predicate_ptr, 16);
  ew_tsub_masked(16, 16, 1, 31, result, lhs, rhs,
                 predicate, 1, 1);
}
