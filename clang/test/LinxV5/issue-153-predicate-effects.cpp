// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -mlxbc -O3 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o

// CHECK-LABEL: define{{.*}} @allocate_slots(
// CHECK: [[OLD:%[^ ]+]] = call <32 x i32> asm sideeffect "BSTART.TLSU MGATHER.ADD, U32
// CHECK-SAME: B.IOT $3, mask=1111, last
// CHECK-SAME: ~{memory}
// CHECK-SAME: <32 x i32> [[PRED:%[^, ]+]], ptr
// CHECK: call void asm sideeffect "BSTART.TLSU MSCATTER, U32
// CHECK-SAME: B.IOT $2, mask=1111, last
// CHECK-SAME: ~{memory}
// CHECK-SAME: (<32 x i32> [[OLD]], <32 x i64> {{[^,]+}}, <32 x i32> [[PRED]], ptr
// CHECK: ret void
extern "C" void allocate_slots(int *__restrict count,
                                const int *__restrict mask,
                                int *__restrict slots) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    if (mask[element] != 0)
      slots[element] = __atomic_fetch_add(count, 1, __ATOMIC_RELAXED);
}

// CHECK-LABEL: define{{.*}} @scatter_streams(
// CHECK: asm sideeffect "BSTART.TLSU MSCATTER, U32
// CHECK-SAME: B.IOT $2, mask=1111, last
// CHECK-SAME: ~{memory}
// CHECK: asm sideeffect "BSTART.TLSU MSCATTER, U32
// CHECK-SAME: B.IOT $2, mask=1111, last
// CHECK-SAME: ~{memory}
// CHECK: asm sideeffect "BSTART.TLSU MSCATTER, U32
// CHECK-SAME: B.IOT $2, mask=1111, last
// CHECK-SAME: ~{memory}
// CHECK: ret void
extern "C" void scatter_streams(const int *__restrict slots,
                                 const int *__restrict mask,
                                 const int *__restrict keys,
                                 const int *__restrict offsets,
                                 const int *__restrict indices,
                                 int *__restrict out_keys,
                                 int *__restrict out_offsets,
                                 int *__restrict out_indices) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    if (mask[element] != 0)
      out_keys[slots[element]] = keys[element];
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    if (mask[element] != 0)
      out_offsets[slots[element]] = offsets[element];
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    if (mask[element] != 0)
      out_indices[slots[element]] = indices[element];
}

// CHECK-LABEL: define{{.*}} @wide_slots(
// CHECK: [[WIDE_OLD:%[^ ]+]] = call <256 x i32> asm sideeffect "BSTART.TLSU MGATHER.ADD, U32
// CHECK-SAME: B.IOT $3, mask=1111, last
// CHECK-SAME: ~{memory}
// CHECK: call void asm sideeffect "BSTART.TLSU MSCATTER, U32
// CHECK-SAME: B.IOT $2, mask=1111, last
// CHECK-SAME: (<256 x i32> [[WIDE_OLD]],
// CHECK: ret void
extern "C" void wide_slots(int *__restrict count,
                            const int *__restrict mask,
                            int *__restrict slots) {
#pragma pto element for grain(1024)
  for (unsigned element = 0; element < 256; ++element)
    if (mask[element] != 0)
      slots[element] = __atomic_fetch_add(count, 1, __ATOMIC_RELAXED);
}
