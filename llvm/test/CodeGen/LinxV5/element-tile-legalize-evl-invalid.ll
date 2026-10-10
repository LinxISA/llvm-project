; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %s 2>&1 | FileCheck %s
; CHECK: Tile legalization: requires a 32-bit mask and constant EVL=32
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.vp.gather.v32i32.v32p0(<32 x ptr>, <32 x i1>, i32)
declare void @llvm.vp.scatter.v32i32.v32p0(<32 x i32>, <32 x ptr>, <32 x i1>, i32)

define void @raw_index(ptr %base) {
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0, !linx.pto.element.vp.owner !0
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison, <32 x i32> zeroinitializer, !linx.pto.element.vp.owner !0
  %address = getelementptr i32, <32 x ptr> %bases, <32 x i32> <i32 -32, i32 -31, i32 -30, i32 -29, i32 -28, i32 -27, i32 -26, i32 -25, i32 -24, i32 -23, i32 -22, i32 -21, i32 -20, i32 -19, i32 -18, i32 -17, i32 -16, i32 -15, i32 -14, i32 -13, i32 -12, i32 -11, i32 -10, i32 -9, i32 -8, i32 -7, i32 -6, i32 -5, i32 -4, i32 -3, i32 -2, i32 -1>, !linx.pto.element.vp.owner !0
  %value = call <32 x i32> @llvm.vp.gather.v32i32.v32p0(<32 x ptr> %address, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 17), !linx.pto.element.vp.owner !0
  call void @llvm.vp.scatter.v32i32.v32p0(<32 x i32> %value, <32 x ptr> %address, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 17), !linx.pto.element.vp.owner !0
  ret void
}

!0 = distinct !{}
!1 = !{i32 32}
