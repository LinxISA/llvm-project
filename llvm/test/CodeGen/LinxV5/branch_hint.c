// RUN: clang %s --target=linx64 -mcpu=v0.43w -S -O2 -mllvm -enable-branch-hint=true -o - | FileCheck %s --check-prefixes=CHECK-OPEN
// RUN: clang %s --target=linx64 -mcpu=v0.43w -S -O2 -mllvm -enable-branch-hint=false -o - | FileCheck %s --check-prefixes=CHECK-CLOSE

#define strong_likely(x) __builtin_branch_hint (!!(x), 1)
#define strong_unlikely(x) __builtin_branch_hint (x, 0)

// CHECK-OPEN:   B.HINT BR.unlikely, TEMP.none, 0
// CHECK-OPEN:   L.BSTART.STD DIRECT, func1
// CHECK-OPEN:   L.BSTART.STD DIRECT, func2
// CHECK-CLOSE-NOT:   B.HINT BR.unlikely, TEMP.none, 0
void func1();
void func2();

void test1(int cond1) {
  if (strong_likely(cond1)) {
    func1();
  } else {
    func2();
  }
}

// CHECK-OPEN:   B.HINT BR.unlikely, TEMP.none, 0
// CHECK-OPEN:   L.BSTART.STD DIRECT, func2
// CHECK-OPEN:   L.BSTART.STD DIRECT, func1
// CHECK-CLOSE-NOT:   B.HINT BR.unlikely, TEMP.none, 0
void func1();
void func2();

void test2(int cond1) {
  if (strong_unlikely(cond1)) {
    func1();
  } else {
    func2();
  }
}