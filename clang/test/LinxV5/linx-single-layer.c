// RUN: not %clang --target=linx64v5 -fsyntax-only %s 2>&1 | FileCheck %s
// RUN: not %clang --target=linx64v5 -mlinx-single-layer -fsyntax-only %s 2>&1 | FileCheck %s

// The current LinxV5 superscalar compiler rejects every SIMT entry point,
// regardless of whether -mlinx-single-layer is explicitly present.

__vec__ int vec_func(int a) { return a + 1; }
// CHECK: error: SIMT constructs are not supported by the superscalar compiler; check the source for unintended SIMT usage

__mtc__ int mtc_func(int a) { return a + 2; }
// CHECK: error: SIMT constructs are not supported by the superscalar compiler; check the source for unintended SIMT usage

int plain_func(int a) { return a + 3; }
