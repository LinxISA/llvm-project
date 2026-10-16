// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -Xclang -fexperimental-strict-floating-point -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -Werror=ignored-pragmas -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=FRONT
// FRONT: FENV_ACCESS
// FRONT-SAME: is not supported on this target
#pragma STDC FENV_ACCESS ON
extern "C" void fenv_loop(float *output, const float *input) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    output[element] = input[element] + 1.0f;
}
// CHECK: strictfp/fenv requires constrained floating-point lowering
