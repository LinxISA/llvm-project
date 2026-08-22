// RUN: %clang -target linx64-unknown-linux-musl -std=c++20 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang -target linx64-unknown-linux-musl -std=c++20 -c -O2 %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=OBJ

using E8M0Tile = __fp8_e8m0 tile_size(4096);
using FP16Tile = __half tile_size(2048);
using FP32Tile = __fp32 tile_size(1024);
using IntTile = int tile_size(1024);

unsigned thread_index() {
  return __builtin_linx_get_thread_idx();
}

// IR-LABEL: define{{.*}} i32 @_Z12thread_indexv()
// IR: call i32 asm "ssrget 0x0802, ->$0", "=r"()
// OBJ-LABEL: <_Z12thread_indexv>:
// OBJ: ssrget{{[[:space:]]+}}0x802,{{[[:space:]]+}}->a0

void tile_constraint(E8M0Tile &Dst, const E8M0Tile &Src) {
  asm volatile("" : "=Tr"(Dst) : "Tr"(Src));
}

// IR-LABEL: define{{.*}} void @_Z15tile_constraint
// IR: bitcast <4096 x i8> {{.*}} to <1024 x i32>
// IR: call <1024 x i32> asm sideeffect "", "=^Tr,^Tr"
// IR: bitcast <1024 x i32> {{.*}} to <4096 x i8>

void shared_chain() {
  unsigned long Handle;
  asm volatile("B.IOS mask=1111, ->%S0<4KB>" : "=Sr"(Handle));
  asm volatile("B.IOS %S0, mask=1111" : : "Sr"(Handle));
}

// IR-LABEL: define{{.*}} void @_Z12shared_chainv()
// IR: call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<4KB>", "=^Sr"()
// IR: call void asm sideeffect "B.IOS ${0:S}, mask=1111", "^Sr"(i64 {{.*}})
// OBJ-LABEL: <_Z12shared_chainv>:
// OBJ-NOT: addi
// OBJ-NOT: LDI
// OBJ-NOT: SDI
// OBJ: B.IOS{{[[:space:]]+}}mask=1111, ->S0<4KB>
// OBJ: B.IOS{{[[:space:]]+}}S0, mask=1111
// OBJ-NOT: addi
// OBJ-NOT: LDI
// OBJ-NOT: SDI

#define TR_SIZE_CODE_TEST(Name, Type, SizeCode)                              \
  void Name() {                                                              \
    Type Value;                                                              \
    asm volatile("B.IOT mask=1111, last, ->%0<%Z1>"                         \
                 : "=Tr"(Value)                                             \
                 : "i"(SizeCode));                                          \
  }

TR_SIZE_CODE_TEST(fp32_128b, FP32Tile, 1)
TR_SIZE_CODE_TEST(fp16_4kb, FP16Tile, 6)
TR_SIZE_CODE_TEST(int_64kb, IntTile, 10)
TR_SIZE_CODE_TEST(mx_8kb, E8M0Tile, 7)

// OBJ: B.IOT{{[[:space:]]+}}mask=1111, last, ->t<128B>
// OBJ: B.IOT{{[[:space:]]+}}mask=1111, last, ->t<4KB>
// OBJ: B.IOT{{[[:space:]]+}}mask=1111, last, ->t<64KB>
// OBJ: B.IOT{{[[:space:]]+}}mask=1111, last, ->t<8KB>

#define SR_SIZE_CODE_TEST(Name, SizeCode)                                    \
  void Name() {                                                              \
    unsigned long Handle;                                                    \
    asm volatile("B.IOS mask=1111, ->%S0<%Z1>"                             \
                 : "=Sr"(Handle)                                            \
                 : "i"(SizeCode));                                          \
  }

SR_SIZE_CODE_TEST(shared_128kb, 11)
SR_SIZE_CODE_TEST(shared_256kb, 12)

// OBJ: B.IOS{{[[:space:]]+}}mask=1111, ->S0<128KB>
// OBJ: B.IOS{{[[:space:]]+}}mask=1111, ->S0<256KB>

void tile_add(FP32Tile &Dst, const FP32Tile &A, const FP32Tile &B) {
  asm volatile("BSTART.TEPL 0, 0, S32\n"
               "B.IOT %1, %2, mask=1111, last, ->%0<4KB>"
               : "=&Tr"(Dst)
               : "Tr"(A), "Tr"(B));
}

// OBJ: BSTART.TLOAD{{[[:space:]]+}}S32
// OBJ: BSTART.TLOAD{{[[:space:]]+}}S32
// OBJ: BSTART.VEC{{[[:space:]]+}}TADD, S32
// OBJ: B.IOT{{[[:space:]]+}}t#2, t#1, mask=1111, last, ->t<4KB>
// OBJ: BSTART.TSTORE{{[[:space:]]+}}S32
