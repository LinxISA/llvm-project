// RUN: %clang --target=linx64v5 -mlxbc -std=c11 -O0 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang --target=linx64v5 -mlxbc -std=c11 -O2 -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

typedef unsigned int tile_size(32) tile32;

void histogram_tail_c(tile32 *old_values, unsigned int *hist,
                      const tile32 *indices, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      (*old_values)[i] = __atomic_fetch_add(&hist[(*indices)[i]], 1u,
                                          __ATOMIC_RELAXED);
    else
      (*old_values)[i] = 0;
  }
}

void histogram_selected_c(tile32 *old_values, unsigned int *hist,
                          const tile32 *indices, const tile32 *keys,
                          unsigned int selected, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes && (*keys)[i] == selected)
      (*old_values)[i] = __atomic_fetch_add(&hist[(*indices)[i]], 1u,
                                          __ATOMIC_RELAXED);
    else
      (*old_values)[i] = 0;
  }
}

// IR-LABEL: define{{.*}} @histogram_tail_c
// IR: @llvm.linx.experimental.ew.tci
// IR: @llvm.linx.experimental.ew.tcmps.gpr
// IR: @llvm.linx.experimental.ew.tlea
// IR: @llvm.linx.experimental.ew.mgather.add.masked
// IR-NOT: atomicrmw
// IR-LABEL: define{{.*}} @histogram_selected_c
// IR: @llvm.linx.experimental.ew.tcmps.gpr
// IR: @llvm.linx.experimental.ew.tcmps.gpr
// IR: and i64
// IR: @llvm.linx.experimental.ew.tlea
// IR: @llvm.linx.experimental.ew.mgather.add.masked
// IR-NOT: atomicrmw

// OBJ-LABEL: <histogram_tail_c>:
// OBJ: BSTART.TEPL TCI, U32
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TLSU MGATHER.ADD, U32
// OBJ: ExecMaskPresent
// OBJ-LABEL: <histogram_selected_c>:
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: and
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TLSU MGATHER.ADD, U32
// OBJ: ExecMaskPresent
