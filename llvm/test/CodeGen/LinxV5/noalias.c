// RUN: clang %s --target=linx64v5 -S -emit-llvm -O2 -mllvm -enable-struct-access-opt=true -o - | FileCheck %s --check-prefixes=CHECK

typedef struct {
  int a;
  int b;
} sub_s;

typedef struct {
  int a;
  int b;
  sub_s s;
} s_type;



int foo(s_type *);
int foo2(s_type *, s_type *);


static int foo_inl(s_type *__noalias__ s) {
  return s->a + 1;
}


static int foo_inl1(s_type *__noalias__ s) {
  return s->b + 1;
}

static int foo_inl2(s_type *s) {
  return s->b + 1;
}

static int foo_inl3(s_type *s) {
  return s->s.b + 1;
}

static int foo_inl4(s_type *__noalias__ s) {
  s->s.a += 3;
  return s->b + s->s.a;
}

// CHECK-LABEL: @test1
// CHECK-SAME: linx_noalias
int test1(s_type *__noalias__ s1, s_type *__noalias__ s2) {
  s1->a += 5;
  s1->b += 6;
  s2->s.a += 1;
  s2->s.b += 2;
  return s1->a + s1->b;
}

// CHECK-LABEL: @test2
// CHECK-SAME: linx_noalias
// CHECK-COUNT-1: @foo
int test2(s_type *__noalias__ s1, s_type *__noalias__ s2) {
  s1->a += 5;
  int res = foo(s2);
  s1->b += res;
  return s1->a + s1->b;
}

// CHECK-LABEL: @test3
// CHECK-SAME: linx_noalias
// CHECK-COUNT-1: @foo

int test3(s_type *__noalias__ s1) {
  s1->a += 5;
  int res = foo(s1);
  s1->b += res;
  return s1->a + s1->b;
}

// CHECK-LABEL: @test4
// CHECK-SAME: linx_noalias
int test4(s_type *__noalias__ s1) {
  s1->a += 5;
  int res = foo_inl(s1);
  s1->b += res;
  return s1->a + s1->b;
}

// CHECK-LABEL: @test5
// CHECK-SAME: linx_noalias
int test5(s_type *__noalias__ s1) {
  s1->a += 5;
  int res = foo_inl1(s1);
  return s1->a + res;
}

// CHECK-LABEL: @test6
// CHECK-SAME: linx_noalias
int test6(s_type *__noalias__ s1) {
  s1->a += 5;
  int res = foo_inl2(s1);
  return s1->a + res;
}

// CHECK-LABEL: @test7
// CHECK-SAME: linx_noalias
int test7(s_type *__noalias__ s1) {
  s1->a += 5;
  int res = foo_inl3(s1);
  return s1->b + res;
}

// CHECK-LABEL: @test8
int test8(s_type * s1) {
  s1->a += 5;
  int res = foo_inl4(s1);
  return s1->s.b + res;
}
