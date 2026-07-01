// Verify the legacy Linx benchmark driver flag selects the v0.57 Linx target.
//
// RUN: %clang -mlxbc -### -c %s 2>&1 | FileCheck %s --check-prefix=MLXBC
// RUN: %clang -mlxbc --target=linx32-unknown-linux-gnu -### -c %s 2>&1 | FileCheck %s --check-prefix=EXPLICIT

// MLXBC: Target: linx64-linx-none-elf
// MLXBC: "-triple" "linx64-linx-none-elf"
// MLXBC: "-target-feature" "+lnx-s32"
// MLXBC: "-target-feature" "+lnx-s64"

// EXPLICIT: Target: linx32-unknown-linux-gnu
// EXPLICIT: "-triple" "linx32-unknown-linux-gnu"
// EXPLICIT: "-target-feature" "+lnx-s32"
// EXPLICIT-NOT: "-target-feature" "+lnx-s64"

int linx_mlxbc_driver_test(void) { return 0; }
