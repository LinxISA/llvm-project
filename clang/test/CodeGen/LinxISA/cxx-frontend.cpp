// RUN: %clang -target linx64-unknown-linux-musl -std=c++20 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang -target linx64-unknown-linux-musl -std=c++20 -S -O2 -DOMIT_SHARED %s -o - | FileCheck %s --check-prefix=ASM

using E8M0Tile = __fp8_e8m0 tile_size(4096);

unsigned thread_index() {
  return __builtin_linx_get_thread_idx();
}

// IR-LABEL: define{{.*}} i32 @_Z12thread_indexv()
// IR: call i32 asm "ssrget 0x0802, ->$0", "=r"()
// ASM-LABEL: _Z12thread_indexv:
// ASM: ssrget{{[[:space:]]+}}0x802,{{[[:space:]]+}}->a0

void tile_constraint(E8M0Tile &Dst, const E8M0Tile &Src) {
  asm volatile("" : "=Tr"(Dst) : "Tr"(Src));
}

// IR-LABEL: define{{.*}} void @_Z15tile_constraint
// IR: call <4096 x i8> asm sideeffect "", "=^Tr,^Tr"

#ifndef OMIT_SHARED
void shared_constraint(unsigned long &Dst, unsigned long Src) {
  asm volatile("" : "=Sr"(Dst) : "Sr"(Src));
}

// IR-LABEL: define{{.*}} void @_Z17shared_constraint
// IR: call i64 asm sideeffect "", "=^Sr,^Sr"
#endif
