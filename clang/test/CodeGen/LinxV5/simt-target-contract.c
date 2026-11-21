// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-cpu janus -emit-llvm -o - %s | FileCheck %s --check-prefix=JANUS
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-cpu simt -emit-llvm -o - %s | FileCheck %s --check-prefix=SIMT
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-cpu simt -target-feature -simt -emit-llvm -o - %s | FileCheck %s --check-prefix=DISABLED
// JANUS-NOT: "target-features"="-simt"
// JANUS: "target-cpu"="janus"
// JANUS-NOT: "target-features"="-simt"
// SIMT: "target-cpu"="simt" "target-features"="+simt"
// DISABLED: "target-cpu"="simt" "target-features"="-simt"

// In particular, do not add a disabling TU feature to Janus implicitly:
// legacy function attributes may select the SIMT CPU later in CodeGen.
int plain(int value) { return value; }
