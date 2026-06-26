// RUN: %clang --target=linx64v5 -mlinx-single-layer -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=REJECT
// RUN: %clang --target=linx64v5 -fsyntax-only %s

// Without -mlinx-single-layer, __vec__/__mtc__ are accepted (default two-layer
// architecture). With -mlinx-single-layer, both are rejected with
// err_linx_simt_disabled, because they are the only entry points that force
// the SIMT execution model (vcall/mcall code generation).

__vec__ int vec_func(int a) { return a + 1; }
// REJECT: error: Linx SIMT execution model is disabled by '-mlinx-single-layer'
// REJECT-SAME: the ''__vec__'' attribute is not allowed in single-layer

__mtc__ int mtc_func(int a) { return a + 2; }
// REJECT: error: Linx SIMT execution model is disabled by '-mlinx-single-layer'
// REJECT-SAME: the ''__mtc__'' attribute is not allowed in single-layer

// A plain (non-SIMT) function is always fine, even with -mlinx-single-layer.
int plain_func(int a) { return a + 3; }
