// RUN: %clang_cc1 -triple linx64v5 -O1 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -fsyntax-only -verify -DNEGATIVE %s
// Bounds are byte displacements and must not pass through TLEA scaling.
typedef unsigned value_t __attribute__((ext_vector_type(32)));
typedef unsigned long long index_t __attribute__((ext_vector_type(32)));
typedef unsigned predicate_t __attribute__((ext_vector_type(32)));

void bounded(unsigned *base, value_t value, index_t index, predicate_t mask,
             long long min, long long max) {
  value_t result;
  ew_mgather_masked_range(32, 1, 25, 0, result, base, index, mask, min, max);
  ew_mscatter_masked_range(32, 1, 25, base, value, index, mask, min, max);
  ew_mgather_add_range(32, 1, 25, 0, result, base, index, value, min, max);
  ew_mscatter_add_range(32, 1, 25, base, value, index, min, max);
  ew_mscatter_add_predicate_range(32, 1, 1, 25, 29, base, value, index, mask,
                                 0, min, max);
}
// CHECK-LABEL: define {{.*}}@bounded
// CHECK: call {{.*}}@llvm.linx.experimental.ew.tlea
// CHECK: @llvm.linx.experimental.ew.mgather.masked.range.{{.*}}i64 {{%.*}}, i64 {{%.*}})
// CHECK: @llvm.linx.experimental.ew.mscatter.masked.range.
// CHECK: @llvm.linx.experimental.ew.mgather.add.range.
// CHECK: @llvm.linx.experimental.ew.mscatter.add.range.
// CHECK: @llvm.linx.experimental.ew.mscatter.add.predicate.range.
void wide_bounds(unsigned *base, value_t value, index_t index) {
  ew_mscatter_add_range(32, 1, 25, base, value, index,
                       -1099511627776LL, 1099511627776LL);
}
// CHECK-LABEL: define {{.*}}@wide_bounds
// CHECK: @llvm.linx.experimental.ew.mscatter.add.range.{{.*}}i64 -1099511627776, i64 1099511627776)
#ifdef NEGATIVE
void invalid(unsigned *base, value_t value, index_t index, predicate_t mask) {
  ew_mscatter_add_range(32, 1, 25, base, value, index, 0, 0LL); // expected-error {{inconsistent}}
  ew_mscatter_add_range(32, 1, 25, base, value, index, 0LL, 0ULL); // expected-error {{inconsistent}}
  ew_mscatter_add_range(32, 1, 25, base, value, index, 0LL); // expected-error {{too few arguments}}
}
#endif
