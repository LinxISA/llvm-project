// RUN: %clang --target=linx64v5 -mlxbc -std=c11 -O2 \
// RUN:   -mllvm -linxv5-enable-generic-element=true \
// RUN:   -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang --target=linx64v5 -mlxbc -std=c11 -O2 \
// RUN:   -mllvm -linxv5-enable-generic-element=true -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

void histogram_tail_c(unsigned int *restrict old_values,
                      unsigned int *restrict hist,
                      const unsigned int *restrict indices,
                      unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      old_values[i] = __atomic_fetch_add(&hist[indices[i]], 1u,
                                         __ATOMIC_RELAXED);
    else
      old_values[i] = 0;
  }
}

void histogram_selected_c(unsigned int *restrict old_values,
                          unsigned int *restrict hist,
                          const unsigned int *restrict indices,
                          const unsigned int *restrict keys,
                          unsigned int selected, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes && keys[i] == selected)
      old_values[i] = __atomic_fetch_add(&hist[indices[i]], 1u,
                                         __ATOMIC_RELAXED);
    else
      old_values[i] = 0;
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
// OBJ-DAG: BSTART.TEPL TCMPS, U64
// OBJ-DAG: BSTART.TEPL TLEA, U32
// OBJ-DAG: BSTART.MGATHER.ADD U32
// OBJ-DAG: ExecMaskPresent
// OBJ-LABEL: <histogram_selected_c>:
// OBJ-DAG: BSTART.TEPL TCMPS, U64
// OBJ-DAG: BSTART.TEPL TCMPS, U32
// OBJ-DAG: BSTART.TEPL TLEA, U32
// OBJ-DAG: BSTART.MGATHER.ADD U32
// OBJ-DAG: ExecMaskPresent
