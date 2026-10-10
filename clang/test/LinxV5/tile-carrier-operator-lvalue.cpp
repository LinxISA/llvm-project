// RUN: %clang -target linx64v5-unknown-linux-musl -mlxbc -fenable-matrix -O2 \
// RUN:     -mllvm -enable-all-vector-as-tilereg=true -std=c++20 -D__linx \
// RUN:     -isystem %S/Inputs -S %s -o - | FileCheck %s

// Regression test for the issue-120 carrier-contract hook: a
// reference-returning overloaded operator used as an lvalue
// (operator[]/operator=/conversion) reaches EmitCallExprLValue, and
// NamedDecl::getName() asserts on non-identifier declaration names. The
// hook must skip such callees instead of crashing: any C++ assigning
// through a reference-returning operator (std::vector::operator[],
// TileOP TileArray::operator[], ...) failed on assert-enabled builds
// before the isIdentifier guard was added.

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

// operator[] returning a reference, used as an lvalue: the pattern that
// crashed on the pre-fix hook.
struct Box {
  TileF t;
  TileF &operator[](int) { return t; }
};

extern "C" void operator_lvalue_carrier(Box &b, TileF &src) {
  b[0].data() = src.data(); // operator[] lvalue + boundary carrier store
}

// The identifier-named data() path must keep recording the transport
// contract: the carrier accesses below still lower to CUBE transports.
extern "C" void data_contract_still_recorded(TileF &dst, TileF &src) {
  auto v = src.data();
  dst.data() = v;
}

// Both entry points lower their boundary carrier accesses to CUBE
// transports; no raw carrier-sized S64/S32 NORM payload anywhere.
// CHECK: TLOAD.ND2M32
// CHECK: TSTORE.M322ND
// CHECK-NOT: S64
// CHECK-NOT: S32
