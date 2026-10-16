// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ
using signed_elements = int tile_size(32);
__attribute__((annotate("pto.element.view:v1;dtype=s32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline signed_elements &signed_view(signed_elements &value) { return value; }
void signed_arithmetic(signed_elements *sink, const signed_elements *source,
                       int bias, int divisor) {
  signed_elements input=*source, output{};
  auto &in=signed_view(input);
  auto &out=signed_view(output);
#pragma pto element for
  for(unsigned element=0;element<32;++element) {
    int added=in[element]+bias;
    int product=added*3;
    int reduced=product-7;
    int quotient=reduced/divisor;
    int remainder=quotient%divisor;
    int shifted=remainder>>1;
    out[element]=(shifted & 255) ^ -3;
  }
  *sink=output;
}
// IR-LABEL: define{{.*}}signed_arithmetic
// IR-NOT: llvm.linx.experimental.element.region
// IR-NOT: llvm.linx.experimental.element.view
// IR-NOT: extractelement
// IR-NOT: insertelement
// IR: call <32 x i32> @llvm.linx.blk.tload.v32i32(i64 1, i64 32, i64 1, i64 17, i64 0, i64 21,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 17,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 17, i64 29, i64 0,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 17, i64 29, i64 2,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 17, i64 29, i64 -7, i64 0)
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 17, i64 29, i64 0,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 17, i64 29, i64 3,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 17, i64 29, i64 3,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 17, i64 29, i64 2,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 17, i64 29, i64 1,
// IR-NOT: i64 29, i64 4,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 25, i64 29, i64 9,
// IR: call void @llvm.linx.blk.tstore.v32i32(i64 1, i64 32, i64 1, i64 17, i64 24,
// OBJ-LABEL: <{{.*}}signed_arithmetic
// OBJ: BSTART.TLSU TLOAD, S32
// OBJ: B.DATR ND2M32
// OBJ: BSTART.TEPL TADD, S32
// OBJ: BSTART.TEPL TMUL, S32
// OBJ: BSTART.TEPL TADD, S32
// OBJ: BSTART.TEPL TDIV, S32
// OBJ: BSTART.TEPL TDIV, S32
// OBJ: BSTART.TEPL TMUL, S32
// OBJ: BSTART.TEPL TSUB, S32
// OBJ-NOT: TREM
// OBJ: BSTART.TEPL TSHR, U32
// OBJ: BSTART.TLSU TSTORE, S32

void signed_shift(signed_elements *sink, const signed_elements *source, int shift) {
  signed_elements input=*source, output{};
  auto &in=signed_view(input);
  auto &out=signed_view(output);
#pragma pto element for
  for(unsigned e=0;e<32;++e) out[e]=in[e]>>shift;
  *sink=output;
}
// IR-LABEL: define{{.*}}signed_shift
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 17, i64 29, i64 9,
// OBJ-LABEL: <{{.*}}signed_shift
// OBJ: BSTART.TEPL TSHR, S32
