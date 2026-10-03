// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

using u32tile = unsigned int tile_size(32);

void atomic_add_old(u32tile &old_values, unsigned int *hist,
                    const u32tile &logical_indices,
                    const u32tile &add_values) {
  ew_mgather_add(1, 32, 25, 3, old_values, hist, logical_indices,
                 add_values);
}

// The C-facing operation accepts logical U32 indices.  The IR makes the
// conversion explicit and the TLSU intrinsic receives only U64 byte offsets.
// IR-COUNT-1: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
// IR-SAME: i64 1, i64 32, i64 25, i64 0, <32 x i32>
// IR-SAME: i64 32)
// IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.add
// IR-SAME: <32 x i64>

// OBJ-COUNT-1: BSTART.TEPL TLEA, U32
// OBJ: B.IOT {{.*}} ->t<256B>
// OBJ: BSTART.TLSU MGATHER.ADD, U32
// OBJ-NOT: BSTART.TEPL TLEA
