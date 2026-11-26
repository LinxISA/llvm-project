// RUN: %clang -target linx64v5 -mlxbc -O2 -mllvm -enable-all-vector-as-tilereg=true -S -emit-llvm %s -o - | FileCheck %s
// RUN: not %clang -target linx64v5 -mlxbc -O2 -DSTORE_N8 -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD

namespace pto {
enum class Location { Vec };
enum class BLayout { RowMajor, ColMajor, CubeM16, CubeM32, CubeN8 };
template <Location, class T, int Rows, int Cols, BLayout Layout,
          int ValidRows = Rows, int ValidCols = Cols>
struct Tile {
  int row, col, reserved;
  using Carrier = T __attribute__((ext_vector_type(Rows * Cols)));
  Carrier data_;
  __attribute__((always_inline)) Carrier &data() { return data_; }
  __attribute__((always_inline)) const Carrier &data() const { return data_; }
};
}
using Dynamic = pto::Tile<pto::Location::Vec, float, 32, 256,
                          pto::BLayout::CubeM32, -1, 1>;
// Alignment padding changes the carrier field index. Runtime dimensions
// still come from the Tile object, rather than a fixed field-3 assumption.
// CHECK-LABEL: define {{.*}} @dynamic_carrier(
// CHECK: call i64 @llvm.linx.scalar.load
// CHECK: and i64
// CHECK: call <8192 x float> @llvm.linx.blk.tload{{.*}}(i64 1, i64 %{{.*}}, i64 1, i64 1, i64 0, i64 21,
extern "C" void dynamic_carrier(Dynamic &tile) {
  asm volatile("" : : "Tr"(tile.data()));
}

using Aux = pto::Tile<pto::Location::Vec, unsigned long, 2, 32,
                      pto::BLayout::CubeN8, 1, 32>;
// CHECK-LABEL: define {{.*}} @load_n8(
// CHECK: call <64 x i64> @llvm.linx.blk.tload{{.*}}(i64 32, i64 1, i64 32, i64 24, i64 0, i64 23,
extern "C" void load_n8(Aux &src) {
  asm volatile("" : : "Tr"(src.data()));
}
#ifdef STORE_N8
// BAD: error: 64-bit CUBE_N8 tile carrier cannot be stored to memory
extern "C" void store_n8(Aux &dst, Aux &src) {
  dst.data() = src.data();
}
#endif
