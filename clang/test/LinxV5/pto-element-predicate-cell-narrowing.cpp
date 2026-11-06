// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -linxv5-force-element-tile-mask=true -emit-llvm -S %s -o - | FileCheck %s --check-prefix=TILE
// RUN: %clang++ --target=linx64v5 -mlxbc -O3 -mllvm -linxv5-enable-generic-element=true -mllvm -linxv5-force-element-tile-mask=true -emit-llvm -S %s -o - | FileCheck %s --check-prefix=TILE
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o - | FileCheck %s --check-prefix=GPR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -linxv5-force-element-tile-mask=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -DSIGNED_SOURCE -mllvm -linxv5-enable-generic-element=true -mllvm -linxv5-force-element-tile-mask=true -emit-llvm -S %s -o - | FileCheck %s --check-prefix=SIGNED
// RUN: %clang++ --target=linx64v5 -mlxbc -O3 -DSIGNED_SOURCE -mllvm -linxv5-enable-generic-element=true -mllvm -linxv5-force-element-tile-mask=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.signed.o

#ifdef SIGNED_SOURCE
using wide_elements = long long tile_size(32);
#else
using wide_elements = unsigned long long tile_size(32);
#endif
using elements = unsigned tile_size(32);

#ifdef SIGNED_SOURCE
__attribute__((annotate("pto.element.view:v1;dtype=s64;rows=32;cols=1;layout=cube_m32"), always_inline))
#else
__attribute__((annotate("pto.element.view:v1;dtype=u64;rows=32;cols=1;layout=cube_m32"), always_inline))
#endif
inline wide_elements &view(wide_elements &value) { return value; }

__attribute__((annotate("pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline elements &view(elements &value) { return value; }

extern "C" void narrow(const void *input) {
  wide_elements source;
  asm volatile(
#ifdef SIGNED_SOURCE
               "BSTART.TLSU TLOAD, S64\n"
#else
               "BSTART.TLSU TLOAD, U64\n"
#endif
               "B.DATR ND2M32.normal, Null\n"
               "B.DIM zero, 1, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.DIM zero, 1, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\n"
               "B.IOR [%1, %3], []\n"
               : "=Tr"(source) : "r"(input), "i"(2), "r"(8UL) : "memory");
  elements result{};
  auto &from = view(source);
  auto &to = view(result);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    if (from[element] != 0)
      to[element] = static_cast<unsigned>(from[element]);
  asm volatile("" : : "Tr"(result) : "memory");
}

// TILE-LABEL: define{{.*}} @narrow(
// TILE: call <32 x i32> asm sideeffect "BSTART.TEPL TCVT, U64\0AB.DATR CUBE_M32, U32, Zero, byte0, Eq, RNONE, nosat, 0, 1\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $1, $2, mask=1111, last, ->$0<${3:Z}>\0A", "=@2Tr,@2Tr,@2Tr,i"(<32 x i64> {{.*}}, <128 x i8> {{.*}}, i64 1)
// TILE-NOT: @llvm.vp.trunc
// GPR-LABEL: define{{.*}} @narrow(
// GPR: @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64
// SIGNED: call <32 x i32> asm sideeffect "BSTART.TEPL TCVT, S64\0AB.DATR CUBE_M32, U32, Zero, byte0, Eq, RNONE, nosat, 0, 1
// OBJ-LABEL: <narrow>:
// OBJ: BSTART.TEPL TCVT, U64
// OBJ-NEXT: {{.*}}B.DATR CUBE_M32, U32, Zero, byte0, Eq, RNONE, nosat, 0, 1
// OBJ-NEXT: {{.*}}C.B.DIMI 1, {{.*}}->lb0
// OBJ-NEXT: {{.*}}C.B.DIMI 32, {{.*}}->lb1
// OBJ-NEXT: {{.*}}B.IOT t#{{[0-9]+}}.reuse, t#{{[0-9]+}}.reuse, mask=1111, last, {{.*}}->t<128B>
