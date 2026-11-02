// RUN: %clang_cc1 -triple linx64v5 -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -fsyntax-only -verify -DINVALID %s

#ifndef INVALID
#define BATCH 256
void grain128(unsigned *out) {
#pragma pto element for grain(128)
  for (unsigned i = 0; i < 32; ++i) out[i] = i;
}
void grain256(unsigned *out) {
#pragma pto element for grain(BATCH)
  for (unsigned i = 0; i < 129; ++i) out[i] = i;
}
void grain512(unsigned *out) {
#pragma pto element for grain(512)
  for (unsigned i = 0; i < 129; ++i) out[i] = i;
}
void grain1024(unsigned *out) {
#pragma pto element for grain(1024)
  for (unsigned i = 0; i < 129; ++i) out[i] = i;
}
void grain2048(unsigned *out) {
#pragma pto element for grain(2048)
  for (unsigned i = 0; i < 129; ++i) out[i] = i;
}
void grain4096(unsigned *out) {
#pragma pto element for grain(4096)
  for (unsigned i = 0; i < 129; ++i) out[i] = i;
}
void default_grain(unsigned *out) {
#pragma pto element for
  for (unsigned i = 0; i < 32; ++i) out[i] = i;
}
// CHECK-DAG: !{!"llvm.loop.linx.pto.element.grain", i32 128}
// CHECK-DAG: !{!"llvm.loop.linx.pto.element.grain", i32 256}
// CHECK-DAG: !{!"llvm.loop.linx.pto.element.grain", i32 512}
// CHECK-DAG: !{!"llvm.loop.linx.pto.element.grain", i32 1024}
// CHECK-DAG: !{!"llvm.loop.linx.pto.element.grain", i32 2048}
// CHECK-DAG: !{!"llvm.loop.linx.pto.element.grain", i32 4096}
#else
void invalid(unsigned *out) {
#pragma pto element for grain(0) // expected-error {{expected grain(128)}}
  for (unsigned i=0;i<32;++i) out[i]=i;
#pragma pto element for grain(192) // expected-error {{expected grain(128)}}
  for (unsigned i=0;i<32;++i) out[i]=i;
#pragma pto element for grain(8192) // expected-error {{expected grain(128)}}
  for (unsigned i=0;i<32;++i) out[i]=i;
#pragma pto element for grain() // expected-error {{expected grain(128)}}
  for (unsigned i=0;i<32;++i) out[i]=i;
#pragma pto element for grain(256 // expected-error {{expected grain(128)}}
  for (unsigned i=0;i<32;++i) out[i]=i;
#pragma pto element for grain(128) grain(256) // expected-error {{unexpected token}}
  for (unsigned i=0;i<32;++i) out[i]=i;
}
#endif
