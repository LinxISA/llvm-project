// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_MUL -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_DIV -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_REM -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_MAX -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_MIN -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_AND -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_OR -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_XOR -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_SHL -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: not %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_SHR -o - %s 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED

using tilef = float tile_size(128);
using tilei = int tile_size(128);

#if defined(CASE_MUL)
void test(tilef &o, const tilef &a, const tilef &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0.0f) o[i] = a[i] * b[i]; else o[i] = a[i] - b[i];
}
#elif defined(CASE_DIV)
void test(tilef &o, const tilef &a, const tilef &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0.0f) o[i] = a[i] / b[i]; else o[i] = a[i] - b[i];
}
#elif defined(CASE_REM)
void test(tilei &o, const tilei &a, const tilei &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0) o[i] = a[i] % b[i]; else o[i] = a[i] - b[i];
}
#elif defined(CASE_MAX)
void test(tilef &o, const tilef &a, const tilef &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0.0f) o[i] = a[i] > b[i] ? a[i] : b[i]; else o[i] = a[i] - b[i];
}
#elif defined(CASE_MIN)
void test(tilef &o, const tilef &a, const tilef &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0.0f) o[i] = a[i] < b[i] ? a[i] : b[i]; else o[i] = a[i] - b[i];
}
#elif defined(CASE_AND)
void test(tilei &o, const tilei &a, const tilei &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0) o[i] = a[i] & b[i]; else o[i] = a[i] - b[i];
}
#elif defined(CASE_OR)
void test(tilei &o, const tilei &a, const tilei &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0) o[i] = a[i] | b[i]; else o[i] = a[i] - b[i];
}
#elif defined(CASE_XOR)
void test(tilei &o, const tilei &a, const tilei &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0) o[i] = a[i] ^ b[i]; else o[i] = a[i] - b[i];
}
#elif defined(CASE_SHL)
void test(tilei &o, const tilei &a, const tilei &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0) o[i] = a[i] << 1; else o[i] = a[i] - b[i];
}
#elif defined(CASE_SHR)
void test(tilei &o, const tilei &a, const tilei &b) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) if (a[i] > 0) o[i] = a[i] >> 1; else o[i] = a[i] - b[i];
}
#endif

// UNSUPPORTED: error: cannot compile this unsupported Linx element-wise loop form yet
