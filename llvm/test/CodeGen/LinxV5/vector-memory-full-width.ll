; RUN: llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true -stop-after=finalize-isel %s -o - | FileCheck %s

; SizeCode does not compensate for a short transfer rectangle.
; CHECK-LABEL: name: copy_u8
; CHECK: PseudoTLOAD_noDsrc_noDdst $r0, 2048, $r0, 1, $r0, 2048, 27, 5, 0, 0,
; CHECK: PseudoTSTORE_noDsrc_noDdst {{.*}}, $r0, 2048, $r0, 1, $r0, 2048, 27, 0,
; CHECK-LABEL: name: copy_i16
; CHECK: PseudoTLOAD_noDsrc_noDdst $r0, 2048, $r0, 1, $r0, 2048, 18, 6, 0, 0,
; CHECK: PseudoTSTORE_noDsrc_noDdst {{.*}}, $r0, 2048, $r0, 1, $r0, 2048, 18, 0,
; CHECK-LABEL: name: copy_fp16
; CHECK: PseudoTLOAD_noDsrc_noDdst $r0, 2048, $r0, 1, $r0, 2048, 4, 6, 0, 0,
; CHECK: PseudoTSTORE_noDsrc_noDdst {{.*}}, $r0, 2048, $r0, 1, $r0, 2048, 4, 0,
; CHECK-LABEL: name: copy_i32
; CHECK: PseudoTLOAD_noDsrc_noDdst $r0, 2048, $r0, 1, $r0, 2048, 17, 7, 0, 0,
; CHECK: PseudoTSTORE_noDsrc_noDdst {{.*}}, $r0, 2048, $r0, 1, $r0, 2048, 17, 0,
; CHECK-LABEL: name: copy_fp32
; CHECK: PseudoTLOAD_noDsrc_noDdst $r0, 2048, $r0, 1, $r0, 2048, 1, 7, 0, 0,
; CHECK: PseudoTSTORE_noDsrc_noDdst {{.*}}, $r0, 2048, $r0, 1, $r0, 2048, 1, 0,
; CHECK-LABEL: name: copy_i64
; CHECK: PseudoTLOAD_noDsrc_noDdst $r0, 1024, $r0, 1, $r0, 1024, 16, 7, 0, 0,
; CHECK: PseudoTSTORE_noDsrc_noDdst {{.*}}, $r0, 1024, $r0, 1, $r0, 1024, 16, 0,
; CHECK-LABEL: name: copy_fp64
; CHECK: PseudoTLOAD_noDsrc_noDdst $r0, 1024, $r0, 1, $r0, 1024, 16, 7, 0, 0,
; CHECK: PseudoTSTORE_noDsrc_noDdst {{.*}}, $r0, 1024, $r0, 1, $r0, 1024, 16, 0,

target triple = "linx64v5-unknown-linux-musl"
define void @copy_u8(ptr %src, ptr %dst) {
  %v = load volatile <2048 x i8>, ptr %src, align 32
  store volatile <2048 x i8> %v, ptr %dst, align 32
  ret void
}
define void @copy_i16(ptr %src, ptr %dst) {
  %v = load volatile <2048 x i16>, ptr %src, align 32
  store volatile <2048 x i16> %v, ptr %dst, align 32
  ret void
}
define void @copy_fp16(ptr %src, ptr %dst) {
  %v = load volatile <2048 x half>, ptr %src, align 32
  store volatile <2048 x half> %v, ptr %dst, align 32
  ret void
}
define void @copy_i32(ptr %src, ptr %dst) {
  %v = load volatile <2048 x i32>, ptr %src, align 32
  store volatile <2048 x i32> %v, ptr %dst, align 32
  ret void
}
define void @copy_fp32(ptr %src, ptr %dst) {
  %v = load volatile <2048 x float>, ptr %src, align 32
  store volatile <2048 x float> %v, ptr %dst, align 32
  ret void
}
define void @copy_i64(ptr %src, ptr %dst) {
  %v = load volatile <1024 x i64>, ptr %src, align 32
  store volatile <1024 x i64> %v, ptr %dst, align 32
  ret void
}
define void @copy_fp64(ptr %src, ptr %dst) {
  %v = load volatile <1024 x double>, ptr %src, align 32
  store volatile <1024 x double> %v, ptr %dst, align 32
  ret void
}

; 65536 is not a legal architectural dimension: execution truncates to 16 bits.
; Preserve the full 64 KiB using two contiguous rows and a real byte stride.
; CHECK-LABEL: name: copy_u8_64k
; CHECK: PseudoTLOAD_noDsrc_noDdst $r0, 32768, $r0, 2, $r0, 32768, 27, 10, 0, 0, {{.*}}, [[PITCH:%[0-9]+]]
; CHECK: PseudoTSTORE_noDsrc_noDdst {{.*}}, $r0, 32768, $r0, 2, $r0, 32768, 27, 0, {{.*}}, [[PITCH]]
define void @copy_u8_64k(ptr %src, ptr %dst) {
  %v = load volatile <65536 x i8>, ptr %src, align 32
  store volatile <65536 x i8> %v, ptr %dst, align 32
  ret void
}
