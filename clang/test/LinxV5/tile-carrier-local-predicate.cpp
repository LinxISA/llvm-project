// RUN: %clang -target linx64v5 -mlxbc -O2 -S -emit-llvm %s -o - | FileCheck %s
// A scoped shape field keeps the aggregate in memory. The independent carrier
// must remain an SSA value, including when its producer is a Predicate.
namespace pto {
enum class Location { Vec };
enum class BLayout { RowMajor };
template<Location, class T, int R, int C, BLayout L, int VR, int VC>
struct Tile {
  int row, col;
  using V=unsigned __attribute__((ext_vector_type(R*C/2)));
  V carrier;
  __attribute__((always_inline)) V &data() { return carrier; }
};
}
using T=pto::Tile<pto::Location::Vec,unsigned short,1,128,pto::BLayout::RowMajor,1,-1>;

// CHECK-LABEL: define {{.*}} @local_predicate(
// CHECK: [[P:%.*]] = {{.*}}call <64 x i32> asm sideeffect
// CHECK-NOT: @llvm.linx.blk.tstore
// CHECK-NOT: load <64 x i32>
// CHECK: call void @llvm.linx.scalar.store
// CHECK-NOT: @llvm.linx.blk.tload
// CHECK-NOT: load <64 x i32>
// CHECK: asm sideeffect {{.*}}<64 x i32> [[P]]
extern "C" void local_predicate() {
  T t;
  asm volatile("" : "=Tr"(t.data()));
  __builtin_linx_scalar_store(&t.col, 17, 4, 1);
  asm volatile("" : : "Tr"(t.data()));
}

extern "C" __attribute__((linx_scalar_shareability("intra"))) void escape(T *);

// An opaque callee can observe or modify the carrier. Keep its memory boundary.
// CHECK-LABEL: define {{.*}} @escaped_carrier(
// CHECK: call void @llvm.linx.blk.tstore
// CHECK: call void @escape
// CHECK: call <64 x i32> @llvm.linx.blk.tload
extern "C" void escaped_carrier() {
  T t;
  asm volatile("" : "=Tr"(t.data()));
  escape(&t);
  asm volatile("" : : "Tr"(t.data()));
}

// A scoped store overlapping the carrier also prevents independent promotion.
// CHECK-LABEL: define {{.*}} @overlapping_carrier(
// CHECK: call void @llvm.linx.blk.tstore
// CHECK: call void @llvm.linx.scalar.store
// CHECK: call <64 x i32> @llvm.linx.blk.tload
extern "C" void overlapping_carrier() {
  T t;
  asm volatile("" : "=Tr"(t.data()));
  __builtin_linx_scalar_store(&t.carrier, 17, 4, 1);
  asm volatile("" : : "Tr"(t.data()));
}
