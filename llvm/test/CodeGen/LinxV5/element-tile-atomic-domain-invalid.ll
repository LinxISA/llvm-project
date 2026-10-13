; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %s 2>&1 | FileCheck %s
; CHECK: PTO element region: Tile legalization: atomic add requires an owning 32-element domain, v32i32 values, explicit GEP, aligned memory, v32i1 mask and EVL=32

declare void @llvm.linx.experimental.element.region(metadata)
declare <64 x i32> @llvm.linx.experimental.element.atomic.add.v64i32.v64p0(
    <64 x ptr>, <64 x i32>, <64 x i1>, i32)

define void @domain64_atomic(ptr %base) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0),
      !linx.pto.element.vp.domain !1
  %one = insertelement <64 x ptr> poison, ptr %base, i32 0,
      !linx.pto.element.vp.owner !0
  %bases = shufflevector <64 x ptr> %one, <64 x ptr> poison,
                         <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  %addresses = getelementptr i32, <64 x ptr> %bases,
               <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  %old = call <64 x i32>
      @llvm.linx.experimental.element.atomic.add.v64i32.v64p0(
          <64 x ptr> align 4 %addresses, <64 x i32> zeroinitializer,
          <64 x i1> zeroinitializer, i32 32),
      !linx.pto.element.vp.owner !0
  ret void
}

!0 = distinct !{}
!1 = !{i32 64}
