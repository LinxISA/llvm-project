// RUN: %clang -target linx64-unknown-linux-musl -std=c++20 -### -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=AUTO
// RUN: %clang -target linx64-unknown-linux-musl -std=c++20 -### -fsyntax-only -nobuiltininc %s 2>&1 | FileCheck %s --check-prefix=NOAUTO

// AUTO: "-include" "linx_blkc.h"
// NOAUTO-NOT: "-include" "linx_blkc.h"
