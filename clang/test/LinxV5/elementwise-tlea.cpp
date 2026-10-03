// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

using s32tile = int tile_size(128);
using u32tile = unsigned int tile_size(128);
using s64tile = long long tile_size(128);
using u64tile = unsigned long long tile_size(128);

void tlea_s32(s64tile &out, const s32tile &indices) {
  ew_tlea(32, 4, 17, 29, out, indices, 32);
}

void tlea_u32(u64tile &out, const u32tile &indices) {
  ew_tlea(32, 4, 25, 29, out, indices, 64);
}

void tlea_s64(s64tile &out, const s64tile &indices) {
  ew_tlea(32, 4, 16, 29, out, indices, 8);
}

void tlea_u64(u64tile &out, const u64tile &indices) {
  ew_tlea(32, 4, 24, 29, out, indices, 16);
}

// IR: call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i32
// IR: call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i32
// IR: call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i64
// IR: call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i64

// OBJ: BSTART.TEPL TLEA, S32
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TEPL TLEA, S64
// OBJ: BSTART.TEPL TLEA, U64
