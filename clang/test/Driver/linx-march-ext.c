// RUN: %clang --target=linx64-unknown-linux-gnu -### -c %s 2>&1 | FileCheck %s --check-prefix=DEFAULT64
// RUN: %clang --target=linx64-unknown-linux-gnu -### -c -march=linx64+lnx-s64+lnx-v+lnx-m+lnx-sys %s 2>&1 | FileCheck %s --check-prefix=MARCH64
// RUN: %clang --target=linx64-unknown-linux-gnu -### -c -march=linx64+pto-v0591-scoped %s 2>&1 | FileCheck %s --check-prefix=SCOPED
// RUN: %clang --target=linx32-unknown-linux-gnu -### -c -march=linx32+lnx-s32+lnx-c+lnx-f+lnx-a %s 2>&1 | FileCheck %s --check-prefix=MARCH32
// RUN: not %clang --target=linx64-unknown-linux-gnu -### -c -march=linx64+lnx-bad %s 2>&1 | FileCheck %s --check-prefix=BADTOKEN
// RUN: not %clang --target=linx32-unknown-linux-gnu -### -c -march=linx32+lnx-s64 %s 2>&1 | FileCheck %s --check-prefix=BADCOMBO
// RUN: not %clang --target=linx32-unknown-linux-gnu -### -c -march=linx64+lnx-s64 %s 2>&1 | FileCheck %s --check-prefix=BADPROFILE
//
// DEFAULT64: "-target-feature" "+lnx-s32"
// DEFAULT64: "-target-feature" "+lnx-s64"
//
// MARCH64: "-target-feature" "+lnx-s32"
// MARCH64: "-target-feature" "+lnx-s64"
// MARCH64: "-target-feature" "+lnx-sys"
// MARCH64: "-target-feature" "+lnx-v"
// MARCH64: "-target-feature" "+lnx-m"
// SCOPED: "-target-feature" "+pto-v0591-scoped"
//
// MARCH32: "-target-feature" "+lnx-s32"
// MARCH32: "-target-feature" "+lnx-c"
// MARCH32: "-target-feature" "+lnx-f"
// MARCH32: "-target-feature" "+lnx-a"
// MARCH32-NOT: "-target-feature" "+lnx-s64"
//
// BADTOKEN: error: unsupported argument 'lnx-bad' to option '-march='
// BADCOMBO: error: unsupported argument 'lnx-s64' to option '-march='
// BADPROFILE: error: unsupported argument 'linx64+lnx-s64' to option '-march='

int linx_march_ext_driver_test(void) { return 0; }
