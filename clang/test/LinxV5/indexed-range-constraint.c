// RUN: %clang -target linx64v5 -O0 -S %s -o - | FileCheck %s
// RUN: %clang -target linx64v5 -O2 -S %s -o - | FileCheck %s
#if __linx_indexed_tlsu_range != 1
#error indexed TLSU range support macro missing
#endif
void explicit_zero(void *base) {
  asm volatile("B.IOR [%0,%1,%2], []" :: "r"(base), "R"(0LL), "r"(0LL));
}
// CHECK-LABEL: explicit_zero:
// CHECK: B.IOR {{.*}}[{{[^,]+}}, {{(a[0-7]|x[0-3]|t[0-9]+|ra|sp|s[0-9]+|r[1-9][0-9]*)}}{{[^]]*}}], []
