; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,verify' -verify-each -S %s | FileCheck %s
; Raw narrow GEP indices sign-extend; explicit zext must remain unsigned.
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.vp.gather.v32i32.v32p0(<32 x ptr>, <32 x i1>, i32)
declare void @llvm.vp.scatter.v32i32.v32p0(<32 x i32>, <32 x ptr>, <32 x i1>, i32)

define void @raw_index(ptr %base) {
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0, !linx.pto.element.vp.owner !0
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison, <32 x i32> zeroinitializer, !linx.pto.element.vp.owner !0
  %address = getelementptr i32, <32 x ptr> %bases, <32 x i32> <i32 -32, i32 -31, i32 -30, i32 -29, i32 -28, i32 -27, i32 -26, i32 -25, i32 -24, i32 -23, i32 -22, i32 -21, i32 -20, i32 -19, i32 -18, i32 -17, i32 -16, i32 -15, i32 -14, i32 -13, i32 -12, i32 -11, i32 -10, i32 -9, i32 -8, i32 -7, i32 -6, i32 -5, i32 -4, i32 -3, i32 -2, i32 -1>, !linx.pto.element.vp.owner !0
  %value = call <32 x i32> @llvm.vp.gather.v32i32.v32p0(<32 x ptr> %address, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !0
  call void @llvm.vp.scatter.v32i32.v32p0(<32 x i32> %value, <32 x ptr> %address, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !0
  ret void
}
; CHECK-LABEL: define void @raw_index
; CHECK: [[RAW_TYPED:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 17, i64 29,
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked
; CHECK: @llvm.linx.experimental.ew.tlea{{.*}}i64 32, i64 1, i64 17, i64 29, <32 x i32> [[RAW_TYPED]], i64 32)
; CHECK: @llvm.linx.experimental.ew.mgather.gpr.masked{{.*}}i64 25, i64 0, i64 29, i64 16,
; CHECK: @llvm.linx.experimental.ew.mscatter.gpr.masked{{.*}}i64 25, i64 29, i64 16,
; CHECK-NOT: llvm.vp.
; CHECK: ret void
!0 = distinct !{}
!1 = !{i32 32}

define void @signed_index(ptr %base) {
  call void @llvm.linx.experimental.element.region(metadata !2), !linx.pto.element.vp.domain !3
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0, !linx.pto.element.vp.owner !2
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison, <32 x i32> zeroinitializer, !linx.pto.element.vp.owner !2
  %index = sext <32 x i32> <i32 -32, i32 -31, i32 -30, i32 -29, i32 -28, i32 -27, i32 -26, i32 -25, i32 -24, i32 -23, i32 -22, i32 -21, i32 -20, i32 -19, i32 -18, i32 -17, i32 -16, i32 -15, i32 -14, i32 -13, i32 -12, i32 -11, i32 -10, i32 -9, i32 -8, i32 -7, i32 -6, i32 -5, i32 -4, i32 -3, i32 -2, i32 -1> to <32 x i64>, !linx.pto.element.vp.owner !2
  %address = getelementptr i32, <32 x ptr> %bases, <32 x i64> %index, !linx.pto.element.vp.owner !2
  %value = call <32 x i32> @llvm.vp.gather.v32i32.v32p0(<32 x ptr> %address, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !2
  call void @llvm.vp.scatter.v32i32.v32p0(<32 x i32> %value, <32 x ptr> %address, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !2
  ret void
}
; CHECK-LABEL: define void @signed_index
; CHECK: [[SEXT_TYPED:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 17, i64 29,
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked
; CHECK: @llvm.linx.experimental.ew.tlea{{.*}}i64 32, i64 1, i64 17, i64 29, <32 x i32> [[SEXT_TYPED]], i64 32)
; CHECK: @llvm.linx.experimental.ew.mgather.gpr.masked{{.*}}i64 25, i64 0, i64 29, i64 16,
; CHECK: @llvm.linx.experimental.ew.mscatter.gpr.masked{{.*}}i64 25, i64 29, i64 16,
; CHECK-NOT: llvm.vp.
; CHECK: ret void
!2 = distinct !{}
!3 = !{i32 32}

define void @unsigned_index(ptr %base) {
  call void @llvm.linx.experimental.element.region(metadata !4), !linx.pto.element.vp.domain !5
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0, !linx.pto.element.vp.owner !4
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison, <32 x i32> zeroinitializer, !linx.pto.element.vp.owner !4
  %index = zext <32 x i32> <i32 -32, i32 -31, i32 -30, i32 -29, i32 -28, i32 -27, i32 -26, i32 -25, i32 -24, i32 -23, i32 -22, i32 -21, i32 -20, i32 -19, i32 -18, i32 -17, i32 -16, i32 -15, i32 -14, i32 -13, i32 -12, i32 -11, i32 -10, i32 -9, i32 -8, i32 -7, i32 -6, i32 -5, i32 -4, i32 -3, i32 -2, i32 -1> to <32 x i64>, !linx.pto.element.vp.owner !4
  %address = getelementptr i32, <32 x ptr> %bases, <32 x i64> %index, !linx.pto.element.vp.owner !4
  %value = call <32 x i32> @llvm.vp.gather.v32i32.v32p0(<32 x ptr> %address, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !4
  call void @llvm.vp.scatter.v32i32.v32p0(<32 x i32> %value, <32 x ptr> %address, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !4
  ret void
}
; CHECK-LABEL: define void @unsigned_index
; CHECK: @llvm.linx.experimental.ew.tlea{{.*}}i64 32, i64 1, i64 25, i64 29,
; CHECK: @llvm.linx.experimental.ew.mgather.gpr.masked{{.*}}i64 25, i64 0, i64 29, i64 24,
; CHECK: @llvm.linx.experimental.ew.mscatter.gpr.masked{{.*}}i64 25, i64 29, i64 24,
; CHECK-NOT: llvm.vp.
; CHECK: ret void
!4 = distinct !{}
!5 = !{i32 32}
