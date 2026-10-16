// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

using f16_elements = _Float16 tile_size(64);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=f16;rows=32;cols=2;layout=cube_m32"),
               always_inline))
inline f16_elements &f16_view(f16_elements &value) {
  return value;
}



extern "C" void f16x64_add(_Float16 *sink, const _Float16 *lhs,
                            const _Float16 *rhs) {
  f16_elements left, right, result;
  asm volatile("BSTART.TLSU TLOAD, FP16\n"
               "B.DATR ND2M32, DTYPE_NONE, Null\n"
               "B.DIM zero, 2, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1,%2], []\n"
               : "=Tr"(left) : "r"(lhs), "r"(4UL), "i"(1) : "memory");
  asm volatile("BSTART.TLSU TLOAD, FP16\n"
               "B.DATR ND2M32, DTYPE_NONE, Null\n"
               "B.DIM zero, 2, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1,%2], []\n"
               : "=Tr"(right) : "r"(rhs), "r"(4UL), "i"(1) : "memory");
  auto &out = f16_view(result);
  auto &a = f16_view(left);
  auto &b = f16_view(right);
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element) {
    _Float16 x = a[element], y = b[element];
    out[element] = ((element & 1) == 0 && x > y) ? x + y : x;
  }
  asm volatile("BSTART.TLSU TSTORE, FP16\n"
               "B.DATR M322ND, DTYPE_NONE, Null\n"
               "B.DIM zero, 2, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1,%2], []\n"
               : : "Tr"(result), "r"(sink), "r"(4UL) : "memory");
}

// IR-LABEL: define{{.*}} @f16x64_add
// IR-NOT: @llvm.linx.experimental.element.region
// IR-NOT: @llvm.vp.
// Logical flat indices are composed from legal row/column TCI steps.
// IR: @llvm.linx.experimental.ew.tci.v64i32({{.*}}i64 4294967296)
// IR: @llvm.linx.experimental.ew.tci.v64i32({{.*}}i64 1)
// IR: @llvm.linx.experimental.ew.tcmp.gpr.masked.v64f16
// IR: @llvm.linx.experimental.ew.tsel.gpr.v64f16
// IR: ret void
// OBJ-LABEL: <f16x64_add>:
// OBJ-DAG: BSTART.TEPL TCMP, FP16
// OBJ-DAG: BSTART.TEPL TADD, FP16
// OBJ-DAG: BSTART.TEPL TSEL, FP16

extern "C" void f16x64_if_add_sub(const _Float16 *lhs,
                                    const _Float16 *rhs) {
  f16_elements left, right, result;
  asm volatile("BSTART.TLSU TLOAD, FP16\n"
               "B.DATR ND2M32, DTYPE_NONE, Null\n"
               "B.DIM zero, 2, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1,%2], []\n"
               : "=Tr"(left) : "r"(lhs), "r"(4UL), "i"(1) : "memory");
  asm volatile("BSTART.TLSU TLOAD, FP16\n"
               "B.DATR ND2M32, DTYPE_NONE, Null\n"
               "B.DIM zero, 2, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1,%2], []\n"
               : "=Tr"(right) : "r"(rhs), "r"(4UL), "i"(1) : "memory");
  auto &out = f16_view(result);
  auto &a = f16_view(left);
  auto &b = f16_view(right);
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element) {
    _Float16 x = a[element], y = b[element];
    out[element] = x > 0 ? x + y : x - y;
  }
  asm volatile("BSTART.TLSU TSTORE, FP16\n"
               "B.DATR M322ND, DTYPE_NONE, Null\n"
               "B.DIM zero, 2, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [zero,zero,zero], []\n"
               : : "Tr"(result) : "memory");
}
// IR-LABEL: define{{.*}} @f16x64_if_add_sub
// IR: @llvm.linx.experimental.ew.tneg.gpr.masked.v64f16
// IR-NOT: fsub
// OBJ-LABEL: <f16x64_if_add_sub>:
// OBJ: BSTART.TEPL TNEG, FP16


