// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
using u32 = unsigned;
extern "C" void gather(const float *__restrict input,
                       const u32 *__restrict indices,
                       float *__restrict output, u32 valid) {
#pragma pto element for
  for (unsigned e = 0; e < 32; ++e)
    if (e < valid) output[e] = input[indices[e]];
}
// IR-LABEL: define{{.*}} @gather(
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32f32
// IR: @llvm.linx.experimental.ew.mscatter.gpr.masked.v32i64.v32f32
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <gather>:
// OBJ: BSTART.TLSU MGATHER, FP32
// OBJ: ExecMaskPresent
// OBJ: BSTART.TLSU MSCATTER, FP32
// OBJ: ExecMaskPresent

extern "C" void scatter(const float *__restrict input,
                        const int *__restrict indices,
                        float *__restrict output, u32 valid) {
#pragma pto element for
  for (unsigned e = 0; e < 32; ++e)
    if (e < valid) output[indices[e]] = input[e];
}
// IR-LABEL: define{{.*}} @scatter(
// IR-DAG: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32
// IR-DAG: @llvm.linx.experimental.ew.mgather.gpr.masked.v32f32
// IR: @llvm.linx.experimental.ew.mscatter.gpr.masked.v32i64.v32f32
// IR-NOT: @llvm.vp.scatter
// IR-NOT: @llvm.linx.experimental.element.scatter
// IR: ret void
// OBJ-LABEL: <scatter>:
// OBJ: BSTART.TLSU MGATHER, FP32
// OBJ: BSTART.TLSU MGATHER, U32
// OBJ: BSTART.TEPL TADD, S32
// OBJ: BSTART.TEPL TLEA, S32
// OBJ: BSTART.TLSU MSCATTER, FP32
// OBJ: ExecMaskPresent

extern "C" void ordered_key(const float *__restrict input,
                            u32 *__restrict output, u32 valid) {
#pragma pto element for
  for (unsigned e = 0; e < 32; ++e) {
    if (e < valid) {
      float value = input[e];
      u32 bits = __builtin_bit_cast(u32, value);
      output[e] = value < 0.0f ? ~bits : bits | 0x80000000u;
    }
  }
}
// IR-LABEL: define{{.*}} @ordered_key(
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32f32
// IR-DAG: bitcast <32 x float>
// IR-DAG: call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32
// IR: @llvm.linx.experimental.ew.mscatter.gpr.masked
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <ordered_key>:
// OBJ: BSTART.TLSU MGATHER, FP32
// OBJ: BSTART.TEPL TCMPS, FP32
// OBJ: BSTART.TLSU MSCATTER, U32

// This copy-like body must not disappear into memcpy before required lowering.
extern "C" void raw_bits(const float *__restrict input, u32 *__restrict output) {
#pragma pto element for
  for (unsigned e = 0; e < 32; ++e)
    output[e] = __builtin_bit_cast(u32, input[e]);
}
// IR-LABEL: define{{.*}} @raw_bits(
// IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32
// IR: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
// IR-NOT: @llvm.memcpy
// IR: ret void
// OBJ-LABEL: <raw_bits>:
// OBJ: BSTART.TLSU MGATHER, U32
// OBJ: BSTART.TLSU MSCATTER, U32

using elements = float tile_size(32);
__attribute__((annotate("pto.element.view:v1;dtype=f32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline elements &view(elements &v) { return v; }
extern "C" void wide_iv(const float *__restrict input, float *__restrict output,
                        unsigned long valid) {
  elements tile;
  auto &values = view(tile);
#pragma pto element for
  for (unsigned long e = 0; e < 32; ++e)
    values[static_cast<unsigned>(e)] = e < valid ? input[e] : 0.0f;
  asm volatile("BSTART.TLSU TSTORE, FP32\n"
               "B.DATR NORM, DTYPE_NONE, Null\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1,%2], []\n"
               : : "Tr"(tile), "r"(output), "r"(4UL) : "memory");
}
// IR-LABEL: define{{.*}} @wide_iv(
// IR: call <32 x float> @llvm.linx.experimental.ew.mgather.gpr.masked
// IR: call void asm sideeffect "BSTART.TLSU TSTORE, FP32
// IR: ret void
// OBJ-LABEL: <wide_iv>:
// OBJ: BSTART.TLSU MGATHER, FP32
// OBJ: BSTART.TLSU TSTORE, FP32
