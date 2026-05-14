// RUN: not %clang++ -std=c++20 -O3 -mlxbc -S --target=linx64 %s -DTEST=1 -emit-llvm 2>&1 | FileCheck %s --check-prefix=ERR1
// ERR1: error: function 'copyin' is declared with mtc or vec and cannot be called as a regular function

// RUN: not %clang++ -std=c++20 -O3 -mlxbc -S --target=linx64 %s -DTEST=2 -emit-llvm 2>&1 | FileCheck %s --check-prefix=ERR2
// ERR2: error: The function 'copyin' lacks __vec__ or __mtc__ keyword modifier.

typedef double tile tile_size(1024);

#if TEST==1
/* Scenario 1: copyin with the __mtc__ attribute, but called as a regular function */
extern void __mtc__ copyin(tile __out__ out, double *p);
void tile_caller_1(double *p1, double *p2, double *p3) {
  tile out;
  tile in1;
  tile in2;
  copyin(out, p1);
}
#elif TEST==2
/* Scenario 2: copyin without attributes, but called with <<<>>> parallel syntax */
extern void copyin(tile __out__ out, double *p);
void tile_caller_2(double *p1, double *p2, double *p3) {
  tile out;
  tile in1;
  tile in2;
  copyin<<<1, 1, 1>>>(out, p1);
}
#endif
