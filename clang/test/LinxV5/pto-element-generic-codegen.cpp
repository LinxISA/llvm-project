// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

extern "C" void generic_i32(const int *__restrict input,
                             const int *__restrict divisor,
                             int *__restrict output, unsigned valid,
                             bool enabled) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (!enabled || element >= valid)
      continue;
    int value = input[element];
    int result;
    if (value < 0)
      result = value / divisor[element];
    else if (value == 0)
      result = 7;
    else
      result = value ^ 5;
    output[element] = result;
  }
}
// IR-LABEL: define{{.*}} @generic_i32
// IR-NOT: @llvm.linx.experimental.element.region
// IR-NOT: @llvm.vp.
// IR-NOT: extractelement
// IR-NOT: insertelement
// IR: @llvm.linx.experimental.ew.tlea
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked{{.*}}i64 17, i64 29, i64 3,
// IR: @llvm.linx.experimental.ew.mscatter.gpr.masked
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <generic_i32>:
// OBJ: BSTART.TLSU MGATHER, U32
// OBJ: BSTART.TEPL TDIV, S32
// OBJ: BSTART.TLSU MSCATTER, U32
// OBJ: ExecMaskPresent

extern "C" void generic_compare(const unsigned *__restrict a,
                                 const unsigned *__restrict b,
                                 unsigned *__restrict output) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    unsigned x = a[element], y = b[element];
    output[element] = x < y ? x + 1 : y - 1;
  }
}
// IR-LABEL: define{{.*}} @generic_compare
// IR: @llvm.linx.experimental.ew.tcmp.gpr{{.*}}i64 25,
// IR: @llvm.linx.experimental.ew.tsel.gpr
// IR: @llvm.linx.experimental.ew.mscatter.gpr.masked
// IR: ret void
// OBJ-LABEL: <generic_compare>:
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: BSTART.TEPL TSEL, U32
// OBJ: BSTART.TLSU MSCATTER, U32

extern "C" void generic_signed_store(const int *__restrict input,
                                      const int *__restrict divisor,
                                      int *__restrict output) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    output[element] = input[element] / divisor[element];
}
// IR-LABEL: define{{.*}} @generic_signed_store
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked{{.*}}i64 17, i64 29, i64 3,
// IR: @llvm.linx.experimental.ew.mscatter.gpr.masked{{.*}}i64 32, i64 1, i64 17, i64 29,
// IR: ret void
// OBJ-LABEL: <generic_signed_store>:
// OBJ: BSTART.TEPL TDIV, S32
// OBJ: BSTART.TLSU MSCATTER, S32
