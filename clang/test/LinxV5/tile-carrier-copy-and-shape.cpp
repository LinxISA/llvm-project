// RUN: %clang -target linx64v5 -mlxbc -O2 -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang -target linx64v5 -mlxbc -O2 -c %s -o %t.o
// RUN: %clang -target linx64v5 -mlxbc -O0 -Xclang -disable-llvm-passes -S -emit-llvm %s -o - | FileCheck %s --check-prefix=FRONTEND
// A Tile aggregate copy must retain the same typed transport as data() access.
// Use the API's one-field carrier wrapper, not a vector of the logical dtype.
namespace pto {
enum class Location { Vec };
enum class BLayout { RowMajor, ColMajor, CubeM16, CubeM32, CubeN8 };
template<int Words> struct Carrier {
  using RegisterType = unsigned __attribute__((ext_vector_type(Words)));
  RegisterType Register;
};
template<Location, class T, int Rows, int Cols, BLayout Layout,
         int ValidRows=Rows, int ValidCols=Cols>
struct Tile {
  int row, col;
  Carrier<(Rows*Cols*sizeof(T)+127)/128*32> data_;
  __attribute__((always_inline)) auto &data() { return data_.Register; }
  __attribute__((always_inline)) const auto &data() const { return data_.Register; }
};
template<class T, class Parent> struct ReinterpretedTileView {
  Parent &parent;
  __attribute__((always_inline)) auto &data() { return parent.data(); }
};
}
using F=pto::Tile<pto::Location::Vec,float,32,1,pto::BLayout::CubeM32>;
// CHECK-LABEL: define {{.*}} @copy_tile(
// CHECK-NOT: load <32 x i32>
// CHECK: call <32 x i32> @llvm.linx.blk.tload{{.*}}(i64 1, i64 32, i64 1, i64 1, i64 0, i64 21,
// CHECK-NOT: load <32 x i32>
// CHECK: asm sideeffect
// CHECK: ret void
// FRONTEND-LABEL: define {{.*}} @copy_tile(
// FRONTEND-NOT: @llvm.memcpy{{.*}}i64 160
// FRONTEND: load <32 x i32>{{.*}}!linx.tile.transport
// FRONTEND: store <32 x i32>{{.*}}!linx.tile.transport
extern "C" void copy_tile(F &dst,F &src) {
  F tmp=src;
  asm volatile("" : : "Tr"(tmp.data()));
  dst=tmp;
}
using B=pto::Tile<pto::Location::Vec,__blkc_bf16,32,2,pto::BLayout::CubeM32,32,1>;
// CHECK-LABEL: define {{.*}} @bf16_carrier(
// CHECK-NOT: load <32 x i32>
// CHECK: call <32 x i32> @llvm.linx.blk.tload{{.*}}(i64 1, i64 32, i64 1, i64 5, i64 0, i64 21,
// CHECK: ret void
extern "C" void bf16_carrier(B &src) { asm volatile("" : : "Tr"(src.data())); }
using D=pto::Tile<pto::Location::Vec,_Float16,32,128,pto::BLayout::RowMajor,32,-1>;
// CHECK-LABEL: define {{.*}} @dynamic_carrier(
// CHECK-NOT: load <2048 x i32>
// CHECK: call <2048 x i32> @llvm.linx.blk.tload{{.*}}(i64 %{{.*}}, i64 32, i64 128, i64 4, i64 0, i64 0,
// CHECK: ret void
extern "C" void dynamic_carrier(D &src) { asm volatile("" : : "Tr"(src.data())); }
using P=pto::Tile<pto::Location::Vec,unsigned short,2,64,pto::BLayout::RowMajor,1,17>;
// CHECK-LABEL: define {{.*}} @partial_carrier(
// CHECK: call <64 x i32> @llvm.linx.blk.tload{{.*}}(i64 17, i64 1, i64 64, i64 26, i64 0, i64 0,
// CHECK: call void @llvm.linx.blk.tstore{{.*}}(i64 17, i64 1, i64 64, i64 26, i64 0,
// CHECK: ret void
extern "C" void partial_carrier(P &dst,P &src) {
  asm volatile("BSTART.TLSU TMOV, U16\n"
               "B.DIM zero, 17, ->lb0\n"
               "B.DIM zero, 1, ->lb1\n"
               "B.DIM zero, 64, ->lb2\n"
               "B.IOT %1, mask=1111, last, ->%0<%Z2>\n"
               : "=Tr"(dst.data()) : "Tr"(src.data()), "i"(2));
}

// The view's public dtype, not its backing unsigned dtype, owns transport.
// CHECK-LABEL: define {{.*}} @view_carrier(
// CHECK: call <64 x i32> @llvm.linx.blk.tload{{.*}}(i64 17, i64 1, i64 64, i64 18, i64 0, i64 0,
// CHECK: ret void
extern "C" void view_carrier(P &src) {
  pto::ReinterpretedTileView<short,P> view{src};
  asm volatile("" : : "Tr"(view.data()));
}

namespace pto { namespace region {
template<class Parent, class Part> struct SubTileView {
  Parent *parent;
  __attribute__((always_inline)) auto &data() { return parent->data(); }
};
} }
// A view binds the complete parent, before the consumer applies B.SUBVIEW.
// FRONTEND-LABEL: define {{.*}} @subview_parent_carrier(
// FRONTEND: load <32 x i32>{{.*}}!linx.tile.transport
// CHECK-LABEL: define {{.*}} @subview_parent_carrier(
// CHECK-NOT: load <32 x i32>
// CHECK: @llvm.linx.blk.tload{{.*}}i64 21,
extern "C" void subview_parent_carrier(pto::region::SubTileView<F,F> &view) {
  asm volatile("" : : "Tr"(view.data()));
}
