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
// IR: asm sideeffect "BSTART.TEPL TLEA, U32
// IR: asm sideeffect "BSTART.TLSU MGATHER, U32
// IR: asm sideeffect "BSTART.TEPL TDIV, S32
// IR: asm sideeffect "BSTART.TLSU MSCATTER, U32
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <generic_i32>:
// OBJ: BSTART.TEPL TCMP, U64
// OBJ-NEXT: {{.*}}B.DATR NORM
// OBJ: BSTART.{{(TLSU )?}}MGATHER{{,?}} U32
// OBJ: BSTART.TEPL TDIV, S32
// OBJ: BSTART.{{(TLSU )?}}MSCATTER{{,?}} U32
// OBJ-NOT: ExecMaskPresent

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
// IR: @llvm.linx.experimental.ew.tcmp.v32i32.v32i32.v32i32{{.*}}i64 25,
// IR: @llvm.linx.experimental.ew.tsel.v32i32.v32i32.v32i32.v32i32
// IR: asm sideeffect "BSTART.TLSU MSCATTER, U32
// IR: ret void
// OBJ-LABEL: <generic_compare>:
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: BSTART.TEPL TSEL, U32
// OBJ: BSTART.{{(TLSU )?}}MSCATTER{{,?}} U32

extern "C" void generic_signed_store(const int *__restrict input,
                                      const int *__restrict divisor,
                                      int *__restrict output) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    output[element] = input[element] / divisor[element];
}
// IR-LABEL: define{{.*}} @generic_signed_store
// IR: asm sideeffect "BSTART.TEPL TDIV, S32
// IR: asm sideeffect "BSTART.TLSU MSCATTER, S32
// IR: ret void
// OBJ-LABEL: <generic_signed_store>:
// OBJ: BSTART.TEPL TDIV, S32
// OBJ: BSTART.{{(TLSU )?}}MSCATTER{{,?}} S32
