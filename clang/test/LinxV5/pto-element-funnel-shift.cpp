// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_FSHL_I16 -c %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD16
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_FSHR_I64 -c %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD64

using u32 = unsigned int;

extern "C" void rotate_left(const u32 *__restrict input,
                             u32 *__restrict output, u32 shift, u32 valid) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      output[element] = __builtin_rotateleft32(input[element], shift);
}

// IR-LABEL: define{{.*}} @rotate_left(
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 5,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 8,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 1,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 5,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 9,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 6,
// IR: @llvm.linx.experimental.ew.tsel.gpr.v32i32
// IR-NOT: @llvm.fshl
// OBJ-LABEL: <rotate_left>:
// OBJ: BSTART.TEPL TAND, U32
// OBJ: BSTART.TEPL TSHL, U32
// OBJ: BSTART.TEPL TSUB, U32
// OBJ: BSTART.TEPL TAND, U32
// OBJ: BSTART.TEPL TSHR, U32
// OBJ: BSTART.TEPL TOR, U32
// OBJ: BSTART.TEPL TSEL, U32

extern "C" void rotate_right(const u32 *__restrict input,
                              u32 *__restrict output, u32 shift, u32 valid) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      output[element] = __builtin_rotateright32(input[element], shift);
}

// IR-LABEL: define{{.*}} @rotate_right(
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 8,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 9,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 6,
// IR-NOT: @llvm.fshr
// OBJ-LABEL: <rotate_right>:
// OBJ: BSTART.TEPL TSHL, U32
// OBJ: BSTART.TEPL TSHR, U32
// OBJ: BSTART.TEPL TOR, U32

extern "C" void rotate_boundaries(const u32 *__restrict input,
                                   u32 *__restrict output) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    const u32 value = input[element];
    output[element] = __builtin_rotateleft32(value, 0) ^
                      __builtin_rotateleft32(value, 32) ^
                      __builtin_rotateleft32(value, 63);
  }
}

// IR-LABEL: define{{.*}} @rotate_boundaries(
// IR-NOT: @llvm.fshl
// IR-NOT: @llvm.fshr
// OBJ-LABEL: <rotate_boundaries>:
// OBJ: BSTART.TEPL TSHL, U32
// OBJ: BSTART.TEPL TSHR, U32
// OBJ: BSTART.TEPL TOR, U32

#ifdef TEST_FSHL_I16
extern "C" void rejected_fshl_i16(const u32 *input, u32 *output, u32 shift) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    const unsigned short value = static_cast<unsigned short>(input[element]);
    output[element] = __builtin_rotateleft16(value, shift);
  }
}
#endif

// BAD16: unsupported opcode or effect in generic IR stage: call llvm.fshl.i16

#ifdef TEST_FSHR_I64
extern "C" void rejected_fshr_i64(const unsigned long *input,
                                   unsigned long *output, u32 shift) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    output[element] = __builtin_rotateright64(input[element], shift);
}
#endif

// BAD64: unsupported opcode or effect in generic IR stage: call llvm.fshr.i64
