// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

using tile32 = unsigned int tile_size(32);

void histogram_tail(tile32 &old_values, unsigned int *hist,
                    const tile32 &indices, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      old_values[i] =
          __atomic_fetch_add(&hist[indices[i]], 1u, __ATOMIC_RELAXED);
    else
      old_values[i] = 0;
  }
}

void histogram_if(tile32 &old_values, unsigned int *hist,
                  const tile32 &low8, const tile32 &high8,
                  unsigned int selected_high, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes && high8[i] == selected_high)
      old_values[i] =
          __atomic_fetch_add(&hist[low8[i]], 1u, __ATOMIC_RELAXED);
    else
      old_values[i] = 0;
  }
}

// IR-LABEL: define{{.*}}histogram_tail
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci
// IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// IR: call <32 x i64> @llvm.linx.experimental.ew.tlea
// IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked

// IR-LABEL: define{{.*}}histogram_if
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci
// IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// IR: and i64
// IR: call <32 x i64> @llvm.linx.experimental.ew.tlea
// IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked

// OBJ-LABEL: <_Z14histogram_tail
// OBJ: BSTART.TEPL TCI, U32
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TLSU MGATHER.ADD, U32
// OBJ: ExecMaskPresent

// OBJ-LABEL: <_Z12histogram_if
// OBJ: BSTART.TEPL TCI, U32
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: and
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TLSU MGATHER.ADD, U32
// OBJ: ExecMaskPresent
