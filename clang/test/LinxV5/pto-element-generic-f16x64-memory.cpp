// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

extern "C" void f16x64_gather_scatter(const _Float16 *__restrict input,
                                       _Float16 *__restrict output,
                                       const unsigned *__restrict indices,
                                       unsigned valid) {
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element) {
    if (element < valid)
      output[element] = input[indices[element]];
  }
}

// IR-LABEL: define{{.*}} @f16x64_gather_scatter
// IR: asm sideeffect "BSTART.TEPL TLEA, U32
// IR: asm sideeffect "BSTART.TLSU MGATHER, FP16
// IR: asm sideeffect "BSTART.TLSU MSCATTER, FP16
// IR-NOT: @llvm.vp.
// OBJ-LABEL: <f16x64_gather_scatter>:
// OBJ: BSTART.{{(TLSU )?}}MGATHER{{,?}} FP16
// OBJ: C.B.DIMI 2, ->lb2
// OBJ: BSTART.{{(TLSU )?}}MSCATTER{{,?}} FP16
// OBJ: C.B.DIMI 2, ->lb2

extern "C" void f16x64_signed_indices(
    const _Float16 *__restrict input, _Float16 *__restrict output,
    const int *__restrict indices) {
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element)
    output[element] = input[indices[element]];
}

// IR-LABEL: define{{.*}} @f16x64_signed_indices
// IR: asm sideeffect "BSTART.TEPL TADD, S32
// IR: asm sideeffect "BSTART.TEPL TLEA, S32
// OBJ-LABEL: <f16x64_signed_indices>:
// OBJ: BSTART.TEPL TADD, S32
// OBJ: BSTART.TEPL TLEA, S32

extern "C" void u16x64_gather_scatter(
    const unsigned short *__restrict input,
    unsigned short *__restrict output,
    const unsigned *__restrict indices, unsigned valid) {
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element) {
    if (element < valid)
      output[element] = input[indices[element]];
  }
}

// IR-LABEL: define{{.*}} @u16x64_gather_scatter
// The canonical i64 GEP induction is proved to fit B32 and narrowed before
// TLEA; this preserves the public S32/U32 index contract for flat64 addresses.
// IR: asm sideeffect "BSTART.TEPL TLEA, U32
// IR: asm sideeffect "BSTART.TLSU MGATHER, U16
// IR: asm sideeffect "BSTART.TLSU MSCATTER, U16
// OBJ-LABEL: <u16x64_gather_scatter>:
// OBJ: BSTART.{{(TLSU )?}}MGATHER{{,?}} U16
// OBJ: C.B.DIMI 2, ->lb2
// OBJ: BSTART.{{(TLSU )?}}MSCATTER{{,?}} U16
// OBJ: C.B.DIMI 2, ->lb2

extern "C" void u32x64_copy(const unsigned *__restrict input,
                            unsigned *__restrict output) {
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element)
    output[element] = input[element];
}

// IR-LABEL: define{{.*}} @u32x64_copy
// IR: asm sideeffect "BSTART.TLSU MGATHER, U32
// IR: asm sideeffect "BSTART.TLSU MSCATTER, U32
// OBJ-LABEL: <u32x64_copy>:
// OBJ: BSTART.{{(TLSU )?}}MGATHER{{,?}} U32
// OBJ: C.B.DIMI 2, ->lb2
// OBJ: BSTART.{{(TLSU )?}}MSCATTER{{,?}} U32
// OBJ: C.B.DIMI 2, ->lb2

extern "C" void s32x64_copy(const int *__restrict input,
                            const int *__restrict divisor,
                            int *__restrict output) {
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element)
    output[element] = input[element] / divisor[element];
}

// IR-LABEL: define{{.*}} @s32x64_copy
// IR: asm sideeffect "BSTART.TEPL TDIV, S32
// IR: asm sideeffect "BSTART.TLSU MSCATTER, S32
// OBJ-LABEL: <s32x64_copy>:
// OBJ: BSTART.TEPL TDIV, S32
// OBJ: BSTART.{{(TLSU )?}}MSCATTER{{,?}} S32
// OBJ: C.B.DIMI 2, ->lb2

extern "C" void u32x64_widen_compare(const unsigned *__restrict input,
                                     unsigned *__restrict output,
                                     unsigned long long limit) {
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element) {
    unsigned value = input[element];
    if (static_cast<unsigned long long>(value) < limit)
      output[element] = value;
  }
}

// IR-LABEL: define{{.*}} @u32x64_widen_compare
// IR: asm sideeffect "BSTART.TEPL TLEA, U32
// OBJ-LABEL: <u32x64_widen_compare>:
// OBJ: BSTART.TEPL TLEA, U32

extern "C" void u32x64_runtime_i64_base(const unsigned *__restrict input,
                                         unsigned *__restrict output,
                                         unsigned long offset) {
#pragma pto element for
  for (unsigned long element = 0; element < 64; ++element)
    output[element] = input[offset + (element << 2)];
}

// IR-LABEL: define{{.*}} @u32x64_runtime_i64_base
// IR: getelementptr i32, ptr %{{.*}}, i64 %{{.*}}
// IR: asm sideeffect "BSTART.TEPL TLEA, U32
// IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked.v64i64
// IR: ret void
// OBJ-LABEL: <u32x64_runtime_i64_base>:
// OBJ: BSTART.TEPL TLEA, S32
// OBJ-NOT: BSTART.TEPL TADD, U64
// OBJ: BSTART.{{(TLSU )?}}MGATHER{{,?}} U32

