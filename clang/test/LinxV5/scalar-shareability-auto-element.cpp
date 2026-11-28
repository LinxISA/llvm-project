// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O3 -mllvm -linxv5-enable-generic-element=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
// Scalar initialization and consumption surround fixed-IntraCore indexed work.
extern "C" unsigned mixed(unsigned *__restrict counters,
                           const unsigned *__restrict indices) {
  __builtin_linx_scalar_store(counters, 0, 4, 1);
#pragma pto element for
  for (unsigned e = 0; e < 32; ++e)
    __atomic_fetch_add(&counters[indices[e]], 1u, __ATOMIC_RELAXED);
  return __builtin_linx_scalar_load(counters, 4, 1);
}
// CHECK-LABEL: define{{.*}} @mixed
// CHECK: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call i64 @llvm.linx.scalar.load({{.*}}i32 4, i32 1)
// OBJ-LABEL: <mixed>:
// OBJ: swi.intra
// OBJ: BSTART.MGATHER.ADD
// OBJ: lwui.intra
