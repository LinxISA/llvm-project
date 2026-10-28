// RUN: %clang_cc1 -triple linx64v5 -O1 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -fsyntax-only -verify -DNEGATIVE %s

typedef unsigned values_t __attribute__((ext_vector_type(128)));
typedef unsigned offsets_t __attribute__((ext_vector_type(256)));
typedef unsigned predicate_t __attribute__((ext_vector_type(32)));

void predicate_atomic(unsigned *base, values_t values, offsets_t offsets,
                      predicate_t active) {
  ew_mscatter_add_predicate(32, 4, 4, 25, 29, base, values, offsets, active, 0);
}

// CHECK-LABEL: define {{.*}}@predicate_atomic
// CHECK: call void @llvm.linx.experimental.ew.mscatter.add.predicate.v128i32.v256i32.v32i32(i64 32, i64 4, i64 4, i64 25, i64 29,
// CHECK-NOT: bitcast

#ifdef NEGATIVE
void invalid(unsigned *base, values_t values, offsets_t offsets,
             predicate_t active, int runtime) {
  ew_mscatter_add_predicate(1); // expected-error {{too few arguments}}
  ew_mscatter_add_predicate(runtime, 4, 4, 25, 29, base, values, offsets, active, 0); // expected-error {{immediate}}
  ew_mscatter_add_predicate(32, 4, 4, 25, 29, base, values, offsets, 1, 0); // expected-error {{inconsistent}}
}
#endif
