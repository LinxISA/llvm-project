// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -DREF_SECOND -c %s -o %t.second.o
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DREF_SECOND -S -emit-llvm %s -o - | FileCheck %s --check-prefix=SECOND
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DOTHER_LIVE_WIDE -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=LIVE
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DREF_ALIAS -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALIAS
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DREF_ESCAPE -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ESCAPE
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DOTHER_LIVE -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=LIVE

using u32 = unsigned;
using row128 = u32 tile_size(128);
struct alignas(32) Row {
  u32 header[8];
  row128 data;
};
void *escaped;
__attribute__((always_inline, annotate("pto.element.view:v2;dtype=u32;layout=norm;physical_rows=1;physical_cols=128;valid_rows=1;valid_cols=128")))
inline row128 &view(Row &x) { return x.data; }

// noinline is a regression-test requirement: source outlining must survive.
#ifdef REF_SECOND
#define CALL_UPDATE(ROW, OUT, SELECTED)                                      \
  update(SELECTED, ROW, OUT, static_cast<unsigned short>(SELECTED))
static __attribute__((noinline)) void update(u32 selected, Row &row, u32 *out,
                                             unsigned short tag) {
#else
#define CALL_UPDATE(ROW, OUT, SELECTED)                                      \
  update(ROW, OUT, SELECTED, static_cast<unsigned short>(SELECTED))
static __attribute__((noinline)) void update(Row &row, u32 *out, u32 selected,
                                             unsigned short tag) {
#endif
  auto &elements = view(row);
#pragma pto element for
  for (u32 i = 0; i < 128; ++i)
    if (i == selected)
      elements[i] = 0;
  *out = selected + tag;
}

void reference_caller(u32 *input, u32 *output, u32 selected) {
  Row row;
  asm volatile("BSTART.TLSU TLOAD, U32\nB.DIM zero, 128, ->lb0\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\nB.IOR [%1], []\n"
               : "=Tr"(row.data) : "r"(input), "i"(3) : "memory");
  // A zero-trip inlined source helper can leave an unused view annotation in
  // a caller that also contains the promoted Local Tile call.
  auto &unused = view(row);
  (void)unused;
#if defined(OTHER_LIVE) || defined(OTHER_LIVE_WIDE)
#ifdef OTHER_LIVE_WIDE
  using Wide = float tile_size(16384);
  Wide live;
#else
  row128 live;
#endif
  asm volatile("BSTART.TLSU TLOAD, U32\nB.DIM zero, 128, ->lb0\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\nB.IOR [%1], []\n"
               : "=Tr"(live) : "r"(input), "i"(3) : "memory");
#endif
#ifdef REF_ESCAPE
  escaped = &row;
#endif
#ifdef REF_ALIAS
  CALL_UPDATE(row, reinterpret_cast<u32 *>(&row), selected);
#else
  CALL_UPDATE(row, output, selected);
#endif
  asm volatile("BSTART.TLSU TSTORE, U32\nB.DIM zero, 128, ->lb0\n"
               "B.IOT %0, mask=1111, last\nB.IOR [%1], []\n"
               : : "Tr"(row.data), "r"(output) : "memory");
#if defined(OTHER_LIVE) || defined(OTHER_LIVE_WIDE)
  asm volatile("" : : "Tr"(live) : "memory");
#endif
}

// CHECK-LABEL: define{{.*}}reference_caller
// CHECK-NOT: alloca %struct.Row
// CHECK-NOT: load <128 x i32>
// CHECK-NOT: store <128 x i32>
// CHECK: call{{.*}} inreg <128 x i32> @{{.*}}update{{.*}}.pto.local(<128 x i32> {{.*}}, ptr{{.*}} %output, i32 noundef signext %selected, i16 noundef zeroext
// CHECK-LABEL: define internal{{.*}} <128 x i32> @{{.*}}update{{.*}}.pto.local(<128 x i32>
// CHECK: @llvm.linx.experimental.ew.tsel.row
// CHECK: ret <128 x i32>
// CHECK: "linx-local-tile-abi"="0:128:128"
// ALIAS: Local Tile reference aliases another call argument
// ESCAPE: Local Tile ABI requires caller-private nonescaping storage
// LIVE: Local Tile ABI cannot preserve another live Tile across a call

// SECOND-LABEL: define{{.*}}reference_caller
// SECOND: call inreg <128 x i32> @{{.*}}update{{.*}}.pto.local(i32 noundef signext %selected, <128 x i32> {{.*}}, ptr{{.*}} %output, i16 noundef zeroext
// SECOND: "linx-local-tile-abi"="1:128:128"
