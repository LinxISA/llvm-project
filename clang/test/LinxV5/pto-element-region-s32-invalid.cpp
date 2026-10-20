// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S -o - %s | FileCheck %s --check-prefix=MIXED --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S -o - -DTEST_GATHER %s | FileCheck %s --check-prefix=GATHER --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S -o - -DTEST_ATOMIC %s | FileCheck %s --check-prefix=ATOMIC --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S -o - -DTEST_INDEX %s | FileCheck %s --check-prefix=INDEX --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S -o - -DTEST_KEY %s | FileCheck %s --check-prefix=KEY --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
using elements = unsigned tile_size(32);
__attribute__((annotate("pto.element.view:v1;dtype=s32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline elements &signed_view(elements &v) { return v; }
__attribute__((annotate("pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline elements &unsigned_view(elements &v) { return v; }
void rejected_profile(elements *sink, const elements *source,
                      unsigned *memory, unsigned valid, unsigned selected) {
  elements input, output{};
  asm volatile("" : "=Tr"(input));
#if defined(TEST_INDEX) || defined(TEST_KEY)
  auto &in=signed_view(input);
  auto &out=unsigned_view(output);
#else
  auto &in=unsigned_view(input);
  auto &out=signed_view(output);
#endif
#ifdef TEST_KEY
  elements index_copy;
  asm volatile("" : "=Tr"(index_copy));
  auto &index_elements=unsigned_view(index_copy);
#endif
#pragma pto element for
  for(unsigned e=0;e<32;++e) {
#if defined(TEST_GATHER) || defined(TEST_INDEX)
    if(e<valid) out[e]=memory[in[e]];
    else out[e]=0;
#elif defined(TEST_ATOMIC)
    if(e<valid) out[e]=__atomic_fetch_add(&memory[in[e]],1u,__ATOMIC_RELAXED);
    else out[e]=0;
#elif defined(TEST_KEY)
    if(e<valid && in[e]==selected)
      out[e]=__atomic_fetch_add(&memory[index_elements[e]],1u,__ATOMIC_RELAXED);
    else out[e]=0;
#else
    out[e]=in[e]+1;
#endif
  }
  asm volatile("" : : "Tr"(output));
}
// MIXED-LABEL: define{{.*}}rejected_profile
// MIXED: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 0,
// GATHER-LABEL: define{{.*}}rejected_profile
// GATHER: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
// GATHER: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32(i64 32, i64 1, i64 17, i64 29,
// ATOMIC-LABEL: define{{.*}}rejected_profile
// ATOMIC: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
// ATOMIC: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32(i64 32, i64 1, i64 17, i64 29,
// INDEX-LABEL: define{{.*}}rejected_profile
// INDEX: [[INDEX:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 0,
// INDEX: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, <32 x i32> [[INDEX]], i64 32)
// INDEX: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
// KEY-LABEL: define{{.*}}rejected_profile
// KEY: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
// KEY: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32{{.*}}i64 0)
// KEY: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
