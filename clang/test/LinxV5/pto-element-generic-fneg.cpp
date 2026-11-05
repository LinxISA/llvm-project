// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
// Compiler fixture for the same annotation/Tile ABI as the public API.
// Scalar optimized C++ proves values only: unconstrained LLVM may speculate
// FP expressions. Direct CFG/VP tests separately lock native active-only masks.
using f32_elements = float tile_size(32);
__attribute__((annotate("pto.element.view:v1;dtype=f32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline f32_elements &float_view(f32_elements &value) { return value; }

extern "C" void optimized_if_add_sub(f32_elements *sink, const f32_elements *a,
                            const f32_elements *b) {
  f32_elements input, other, output;
  asm volatile("BSTART.TLSU TLOAD, FP32\n"
               "B.DATR ND2M32, DTYPE_NONE, Null\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1,%2], []\n"
               : "=Tr"(input) : "r"(a), "r"(4UL), "i"(1) : "memory");
  asm volatile("BSTART.TLSU TLOAD, FP32\n"
               "B.DATR ND2M32, DTYPE_NONE, Null\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT mask=1111, last, ->%0<%Z3>\n"
               "B.IOR [%1,%2], []\n"
               : "=Tr"(other) : "r"(b), "r"(4UL), "i"(1) : "memory");
  auto &x = float_view(input);
  auto &y = float_view(other);
  auto &result = float_view(output);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (x[element] > 0.0f)
      result[element] = x[element] + y[element];
    else
      result[element] = x[element] - y[element];
  }
  asm volatile("BSTART.TLSU TSTORE, FP32\n"
               "B.DATR NORM, DTYPE_NONE, Null\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1,%2], []\n"
               : : "Tr"(output), "r"(sink), "r"(4UL) : "memory");
}
// IR-LABEL: define{{.*}} @optimized_if_add_sub
// IR: call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32
// IR-DAG: @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32
// IR-DAG: @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32
// IR-DAG: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32
// IR-DAG: @llvm.linx.experimental.ew.tsel.gpr.v32f32
// IR: call void asm sideeffect "BSTART.TLSU TSTORE, FP32
// IR-NOT: @llvm.linx.experimental.element.region
// IR-NOT: @llvm.vp.
// IR-NOT: @llvm.linx.experimental.ew.tcvt
// IR-NOT: extractelement
// IR-NOT: insertelement
// IR: ret void
// OBJ-LABEL: <optimized_if_add_sub>:
// OBJ: BSTART.TLSU TLOAD, FP32
// OBJ-DAG: BSTART.TEPL TCMPS, FP32
// OBJ-DAG: ExecMaskPresent
// OBJ-DAG: BSTART.TEPL TNEG, FP32
// OBJ-DAG: ExecMaskPresent
// OBJ-DAG: BSTART.TEPL TSEL, FP32
// OBJ: BSTART.TLSU TSTORE, FP32
// InstCombine may rewrite the ordinary if/add/sub CFG to fneg/select/fadd.
// The backend must accept that standard IR; this test does not suppress it.
