; RUN: llc < %s -enable-all-vector-as-tilereg=true --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv

; CHECK-LABEL: vfoo:
; CHECK:      v.ld.local [ta, zero.sd], ->vt.d
; CHECK-NEXT: v.ld.local [tc, zero.sd], ->vt.d
; CHECK-NEXT: v.lw.local [td, zero.sd], ->vt.w
; CHECK-NEXT: v.sd.local vt#3.sd, [to, zero.sd<<3]
; CHECK-NEXT: v.sd.local vt#2.sd, [to1, zero.sd<<3]
; CHECK-NEXT: v.sw.local vt#1.sw, [to2, zero.sd<<2]
define void @vfoo(<1024 x double> __out__ %to, <512 x double> __out__ %to1, <256 x float> __out__ %to2, <1024 x double> %ta, <512 x double> %tb, <512 x double> %tc, <256 x float> %td) #0 {
  %po = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v1024f64(<1024 x double> %to)
  %po1 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v512f64(<512 x double> %to1)
  %po2 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %to2)
  %pa = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v1024f64(<1024 x double> %ta)
  %pb = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v512f64(<512 x double> %tb)
  %pc = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v512f64(<512 x double> %tc)
  %pd = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %td)
  %va = load double, ptr addrspace(6) %pa
  %vb = load double, ptr addrspace(6) %pb
  %vc = load double, ptr addrspace(6) %pc
  %vd = load float, ptr addrspace(6) %pd
  store double %va, ptr addrspace(6) %po
  store double %vc, ptr addrspace(6) %po1
  store float %vd, ptr addrspace(6) %po2
  ret void
}

; CHECK-LABEL: foo:
; CHECK:       VPAR       vfoo,   <M: 16, N: 16, K: 16, MR>   t#4, t#3, t#2, t#1, ->t<8KB>, t<4KB>, t<1KB>
; CHECK:       TSTORE.ND2ZN    <LB0: 16, LB1: 16, LB2: 16, FP32>, t#3, [a0,a1]
; CHECK:       TSTORE.ND2ZN    <LB0: 16, LB1: 16, LB2: 16, FP32>, t#2, [a0,a1]
; CHECK:       TSTORE.ND2ZN    <LB0: 16, LB1: 16, LB2: 16, FP32>, t#1, [a0,a1]
define void @foo(ptr %p) {
  %a = tail call <1024 x double> @llvm.linx.blk.tload.v1024f64(i64 16, i64 16, i64 16, i64 1, i64 0, i64 3, ptr %p, i64 4)
  %b = tail call <512 x double> @llvm.linx.blk.tload.v512f64(i64 16, i64 16, i64 16, i64 1, i64 0, i64 3, ptr %p, i64 4)
  %c = tail call <512 x double> @llvm.linx.blk.tload.v512f64(i64 16, i64 16, i64 16, i64 1, i64 0, i64 3, ptr %p, i64 4)
  %d = tail call <256 x float> @llvm.linx.blk.tload.v256f32(i64 16, i64 16, i64 16, i64 1, i64 0, i64 3, ptr %p, i64 4)
  %m = tail call { <1024 x double>, <512 x double>, <256 x float> } (ptr, i64, i64, i64, <1024 x double>, <512 x double>, <512 x double>, <256 x float>, ...) @llvm.linx.vcall.par.3d4u.v1024f64.v512f64.v256f32.v1024f64.v512f64.v512f64.v256f32(ptr @vfoo, i64 16, i64 16, i64 16, <1024 x double> %a, <512 x double> %b, <512 x double> %c, <256 x float> %d)
  %x = extractvalue { <1024 x double>, <512 x double>, <256 x float> } %m, 0
  %y = extractvalue { <1024 x double>, <512 x double>, <256 x float> } %m, 1
  %z = extractvalue { <1024 x double>, <512 x double>, <256 x float> } %m, 2
  tail call void @llvm.linx.blk.tstore.v1024f64(i64 16, i64 16, i64 16, i64 1, i64 3, ptr %p, i64 4, <1024 x double> %x)
  tail call void @llvm.linx.blk.tstore.v512f64(i64 16, i64 16, i64 16, i64 1, i64 3, ptr %p, i64 4, <512 x double> %y)
  tail call void @llvm.linx.blk.tstore.v256f32(i64 16, i64 16, i64 16, i64 1, i64 3, ptr %p, i64 4, <256 x float> %z)
  ret void
}

declare <1024 x double> @llvm.linx.blk.tload.v1024f64(i64, i64, i64, i64, i64, i64, ptr, i64)
declare <512 x double> @llvm.linx.blk.tload.v512f64(i64, i64, i64, i64, i64, i64, ptr, i64)
declare <256 x float> @llvm.linx.blk.tload.v256f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v1024f64(i64, i64, i64, i64, i64, ptr, i64, <1024 x double>)
declare void @llvm.linx.blk.tstore.v512f64(i64, i64, i64, i64, i64, ptr, i64, <512 x double>)
declare void @llvm.linx.blk.tstore.v256f32(i64, i64, i64, i64, i64, ptr, i64, <256 x float>)

declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v1024f64(<1024 x double>)
declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v512f64(<512 x double>)
declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float>)

declare { <1024 x double>, <512 x double>, <256 x float> } @llvm.linx.vcall.par.3d4u.v1024f64.v512f64.v256f32.v1024f64.v512f64.v512f64.v256f32(ptr, i64, i64, i64, <1024 x double>, <512 x double>, <512 x double>, <256 x float>, ...)

attributes #0 = { "__vec__" }
