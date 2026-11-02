// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

extern "C" void tile_mask_bitwise(const unsigned *__restrict left,
                                   const unsigned *__restrict right,
                                   unsigned *__restrict output,
                                   unsigned *__restrict xor_output,
                                   unsigned *__restrict or_output) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    unsigned first = left[element];
    unsigned second = right[element];
    unsigned result = first;
    if ((first < second) | (first == 3)) {
      result = first & second;
      xor_output[element] = first ^ second;
      or_output[element] = first | second;
    }
    output[element] = result;
  }
}
// IR-LABEL: define{{.*}} @tile_mask_bitwise
// IR: @llvm.linx.experimental.ew.tcmp.v32i32.v32i32.v32i32
// IR: @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 27, i64 29, i64 6,
// IR-DAG: asm sideeffect "BSTART.TEPL TAND, U32
// IR-DAG: asm sideeffect "BSTART.TEPL TXOR, U32
// IR-DAG: asm sideeffect "BSTART.TEPL TOR, U32
// IR: @llvm.linx.experimental.ew.tsel.v32i32.v32i32.v32i32.v32i32
// IR: asm sideeffect "BSTART.TLSU MSCATTER, U32
// IR: ret void
// OBJ-LABEL: <tile_mask_bitwise>:
// OBJ: BSTART.TEPL TOR, U8
// OBJ-DAG: BSTART.TEPL TAND, U32
// OBJ-DAG: B.DATR CUBE_M32, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 1
// OBJ-DAG: BSTART.TEPL TXOR, U32
// OBJ-DAG: BSTART.TEPL TOR, U32
// OBJ: BSTART.TEPL TSEL, U32
