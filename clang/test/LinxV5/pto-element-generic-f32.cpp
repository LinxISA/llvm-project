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

extern "C" void nested_f32(f32_elements *sink, const f32_elements *a,
                            const f32_elements *b, float bias) {
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
    float value = x[element], rhs = y[element];
    if (value > 0.0f) {
      if (value == rhs)
        result[element] = value + bias;
      else
        result[element] = rhs - value;
    } else {
      result[element] = -0.0f;
    }
  }
  asm volatile("BSTART.TLSU TSTORE, FP32\n"
               "B.DATR NORM, DTYPE_NONE, Null\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1,%2], []\n"
               : : "Tr"(output), "r"(sink), "r"(4UL) : "memory");
}
// IR-LABEL: define{{.*}} @nested_f32
// IR: call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32
// IR-DAG: @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32
// IR-DAG: @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32
// IR-DAG: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32
// IR-DAG: @llvm.linx.experimental.ew.tsel.gpr.v32f32
// IR: call void asm sideeffect "BSTART.TLSU TSTORE, FP32
// IR-NOT: @llvm.linx.experimental.element.region
// IR-NOT: @llvm.vp.
// IR-NOT: extractelement
// IR-NOT: insertelement
// IR: ret void
// OBJ-LABEL: <nested_f32>:
// OBJ: BSTART.TLSU TLOAD, FP32
// OBJ-DAG: BSTART.TEPL TCMPS, FP32
// OBJ-DAG: ExecMaskPresent
// OBJ-DAG: BSTART.TEPL TCMP, FP32
// OBJ-DAG: ExecMaskPresent
// OBJ-DAG: BSTART.TEPL TSEL, FP32
// OBJ: BSTART.TLSU TSTORE, FP32
