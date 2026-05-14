// clang-format off
// RUN: not %clang++ --target=linx64 -std=c++20 -O2 -mlxbc -S %s 2>&1 | FileCheck %s --check-prefix=CHECK-ERRORS

typedef double tile_t tile_size(1024);
class tile {
public:
  tile_t data;
};

template<class T>
concept is_tile = requires(T t) {
  t.data;
};

template<class T, class D>
void __mtc__ TFOO_impl(T __out__ out, D &p) {}

template<is_tile T>
void TFOO(T &out, double p) {
  TFOO_impl<tile_t, double><<<1,1,1>>>(out.data, p, p);
}

void foo() {
  tile A;
  TFOO(A, 1.0);
}

// CHECK-ERRORS: error: execution argument count mismatch: provided 3 but function expects 2
// CHECK-ERRORS: TFOO_impl<tile_t, double><<<1,1,1>>>(out.data, p, p)
// CHECK-ERRORS:                                     ^
