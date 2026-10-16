// RUN: %clang -target linx64v5-unknown-linux-musl -mlxbc -fenable-matrix -O2 \
// RUN:     -mllvm -enable-all-vector-as-tilereg=true -std=c++20 -D__linx \
// RUN:     -isystem %S/Inputs -S %s -o - | FileCheck %s

// Issue #120: a pto::Tile<..., CubeM32, ...> passed across a real function
// call must keep its CUBE layout. The carrier accesses on the boundary are
// CUBE transports (ND2M32 load / M322ND store with the runtime valid shape
// from the object's mask fields), never raw S64/S32 NORM payloads that a
// CUBE_M32 consumer would reject per PTO #291. This test replicates the
// minimal pto::Tile shape (tile_size(n) is ext_vector_type(n)) so it stays
// independent of the installed TileOP headers.

namespace pto {
enum class Location { Vec };
enum class BLayout { RowMajor, ColMajor, CubeM16, CubeM32, CubeN8 };

template <Location, typename DType, int Rows, int Cols, BLayout Layout,
          int ValidRow = -1, int ValidCol = -1>
struct Tile {
  int RowMaskInternal;
  int ColMaskInternal;
  using TileDType = DType __attribute__((ext_vector_type(Rows * Cols)));
  TileDType &data() { return data_; }
  TileDType data_;
};
} // namespace pto

using TileF =
    pto::Tile<pto::Location::Vec, float, 32, 256, pto::BLayout::CubeM32>;

extern "C" void transport_roundtrip(TileF &dst, TileF &src) {
  auto v = src.data(); // boundary carrier load
  dst.data() = v;      // boundary carrier store
}

extern "C" void kernel(float *out) {
  TileF a{32, 256}, b{32, 256};
  transport_roundtrip(a, b);
  *out = 0.f;
}

// Boundary reload: CUBE_M32 transport with the runtime valid shape read
// from the object's mask fields and the contiguous 256*4B row stride.
// CHECK: TLOAD.ND2M32 <LB0: {{[a-z0-9#]+}}, LB1: {{[a-z0-9#]+}}, LB2: {{[a-z0-9#]+}}, FP32, Zero>
// Boundary materialization: the matching Local->GM transport.
// CHECK: TSTORE.M322ND <LB0: {{[a-z0-9#]+}}, LB1: {{[a-z0-9#]+}}, LB2: {{[a-z0-9#]+}}, FP32>
// No raw carrier-sized S64/S32 NORM payload anywhere.
// CHECK-NOT: S64
// CHECK-NOT: S32
