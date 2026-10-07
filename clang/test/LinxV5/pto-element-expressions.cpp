// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ
using elements = unsigned int tile_size(32);

void arithmetic(elements &output, const elements &a, const elements &b,
                unsigned int shift) {
#pragma pto element for
  for (unsigned int element = 0; element < 32; ++element) {
    unsigned int sum = a[element] + b[element];
    const unsigned int shifted = sum >> shift;
    output[element] = (shifted & 255u) ^ 17u;
  }
}
// IR-LABEL: define{{.*}}arithmetic
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 0,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 9,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 5,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 7,
// IR-NOT: for.cond
// IR-NOT: extractelement
// OBJ-LABEL: <{{.*}}arithmetic
// OBJ: BSTART.TEPL TADD, U32
// OBJ: B.DATR CUBE_M32
// OBJ: BSTART.TEPL TSHR, U32
// OBJ: BSTART.TEPL TAND, U32
// OBJ: BSTART.TEPL TXOR, U32

void locals(elements &output, const elements &input) {
#pragma pto element for
  for (unsigned int element = 0; element < 32; ++element) {
    unsigned int value = input[element] * 3u;
    value = value - element;
    output[element] = ~value;
  }
}
// IR-LABEL: define{{.*}}locals
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 2,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci{{.*}}i64 4294967296
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 1,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 7,
