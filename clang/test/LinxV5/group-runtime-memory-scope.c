// RUN: %clang --target=linx64v5 -ffreestanding -O0 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=O0 --implicit-check-not='"linx-scalar-shareability"="intra"'
// RUN: %clang --target=linx64v5 -ffreestanding -O2 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=OPT --implicit-check-not='"linx-scalar-shareability"="intra"'
// RUN: %clang --target=linx64v5 -ffreestanding -O0 -c %s -o %t.o0.o
// RUN: %clang --target=linx64v5 -ffreestanding -O2 -c %s -o %t.o2.o
// RUN: %clang --target=linx64v5 -ffreestanding -O3 -c %s -o %t.o3.o

// The actual runtime must share its descriptor, status and result explicitly.
#include "../../../compiler-rt/lib/builtins/linx64v5/group_worker.c"

// O0-LABEL: define{{.*}} @__linx_group_entry(
// O0: call i64 @llvm.linx.scalar.load({{.*}}i32 4, i32 1)
// O0: call i64 @llvm.linx.scalar.load({{.*}}i32 2, i32 1)
// O0: call i64 @llvm.linx.scalar.load({{.*}}i32 8, i32 1)
// O0-LABEL: define{{.*}} @group_store_state(
// O0: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
// OPT-LABEL: define{{.*}} @__linx_group_entry(
// OPT: call i64 @llvm.linx.scalar.load({{.*}}i32 4, i32 1)
// OPT: call i64 @llvm.linx.scalar.load({{.*}}i32 2, i32 1)
// OPT: call i64 @llvm.linx.scalar.load({{.*}}i32 8, i32 1)
// OPT: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
// OPT: call fastcc void @group_sync_core()
// OPT: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
// OPT: call fastcc void @group_sync_core()
// OPT-LABEL: define internal fastcc void @group_sync_core(
// OPT: call void @llvm.linx.sync.core()
