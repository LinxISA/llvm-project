; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/same-region.ll 2>&1 | FileCheck %s --check-prefix=DTYPE
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/cross-region.ll 2>&1 | FileCheck %s --check-prefix=DTYPE
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/incoming-view.ll 2>&1 | FileCheck %s --check-prefix=DTYPE
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/owned-publication.ll 2>&1 | FileCheck %s --check-prefix=DTYPE
;
; DTYPE: PTO element region: Tile legalization: imported carrier PHI requires one consistent dtype

;--- same-region.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @same_region_dtype_conflict(i1 %has.seed, ptr %signed.histogram,
                                        ptr %unsigned.histogram) {
entry:
  br i1 %has.seed, label %seed.path, label %empty.path
seed.path:
  %real = call <32 x i32> asm sideeffect "", "=@2Tr"()
  br label %preheader
empty.path:
  br label %preheader
preheader:
  %seed = phi <32 x i32> [ %real, %seed.path ], [ poison, %empty.path ]
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %loop ]
  %signed.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %seed, i64 1, i64 1, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %signed.index = extractelement <32 x i32> %signed.view, i32 %element
  %signed.address = getelementptr i32, ptr %signed.histogram,
                                  i32 %signed.index
  %signed.old = atomicrmw add ptr %signed.address, i32 1 monotonic, align 4
  %unsigned.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %seed, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %unsigned.index = extractelement <32 x i32> %unsigned.view, i32 %element
  %wide = zext i32 %unsigned.index to i64
  %unsigned.address = getelementptr i32, ptr %unsigned.histogram, i64 %wide
  %unsigned.old = atomicrmw add ptr %unsigned.address, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- owned-publication.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @owned_publication_dtype_conflict(i1 %use.bypass,
                                              ptr %histogram) {
entry:
  br i1 %use.bypass, label %bypass, label %signed.preheader

bypass:
  %u32.real = call <32 x i32> asm sideeffect "", "=@2Tr"()
  br label %merge

signed.preheader:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %signed.loop

signed.loop:
  %signed.element = phi i32 [ 0, %signed.preheader ],
                              [ %signed.next, %signed.loop ]
  %carrier = phi <32 x i32> [ poison, %signed.preheader ],
                              [ %signed.published, %signed.loop ]
  %inserted = insertelement <32 x i32> %carrier, i32 42,
                             i32 %signed.element
  %signed.published = call <32 x i32>
      @llvm.linx.experimental.element.view.v32i32(
          <32 x i32> %inserted, i64 10, i64 10, i64 0, i64 128, i64 17,
          i64 32, i64 1, i64 29)
  %signed.next = add nuw nsw i32 %signed.element, 1
  %signed.more = icmp ult i32 %signed.next, 32
  br i1 %signed.more, label %signed.loop, label %signed.exit, !llvm.loop !1

signed.exit:
  %signed.result = phi <32 x i32> [ %signed.published, %signed.loop ]
  br label %merge

merge:
  %external = phi <32 x i32> [ %u32.real, %bypass ],
                               [ %signed.result, %signed.exit ]
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %unsigned.loop

unsigned.loop:
  %unsigned.element = phi i32 [ 0, %merge ],
                                [ %unsigned.next, %unsigned.loop ]
  %unsigned.view = call <32 x i32>
      @llvm.linx.experimental.element.view.v32i32(
          <32 x i32> %external, i64 11, i64 11, i64 0, i64 128, i64 25,
          i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %unsigned.view, i32 %unsigned.element
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %unsigned.next = add nuw nsw i32 %unsigned.element, 1
  %unsigned.more = icmp ult i32 %unsigned.next, 32
  br i1 %unsigned.more, label %unsigned.loop, label %exit, !llvm.loop !4

exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}

;--- cross-region.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @cross_region_dtype_conflict(i1 %has.seed, ptr %signed.histogram,
                                         ptr %unsigned.histogram) {
entry:
  br i1 %has.seed, label %seed.path, label %empty.path
seed.path:
  %real = call <32 x i32> asm sideeffect "", "=@2Tr"()
  br label %first.preheader
empty.path:
  br label %first.preheader
first.preheader:
  %seed = phi <32 x i32> [ %real, %seed.path ], [ undef, %empty.path ]
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %first.loop
first.loop:
  %first.element = phi i32 [ 0, %first.preheader ],
                             [ %first.next, %first.loop ]
  %signed.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %seed, i64 1, i64 1, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %signed.index = extractelement <32 x i32> %signed.view, i32 %first.element
  %signed.address = getelementptr i32, ptr %signed.histogram,
                                  i32 %signed.index
  %signed.old = atomicrmw add ptr %signed.address, i32 1 monotonic, align 4
  %first.next = add nuw nsw i32 %first.element, 1
  %first.more = icmp ult i32 %first.next, 32
  br i1 %first.more, label %first.loop, label %second.preheader,
      !llvm.loop !1
second.preheader:
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %second.loop
second.loop:
  %second.element = phi i32 [ 0, %second.preheader ],
                              [ %second.next, %second.loop ]
  %unsigned.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %seed, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %unsigned.index = extractelement <32 x i32> %unsigned.view,
                                   i32 %second.element
  %wide = zext i32 %unsigned.index to i64
  %unsigned.address = getelementptr i32, ptr %unsigned.histogram, i64 %wide
  %unsigned.old = atomicrmw add ptr %unsigned.address, i32 1 monotonic, align 4
  %second.next = add nuw nsw i32 %second.element, 1
  %second.more = icmp ult i32 %second.next, 32
  br i1 %second.more, label %second.loop, label %exit, !llvm.loop !4
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}

;--- incoming-view.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @incoming_view_dtype_conflict(i1 %has.seed, ptr %histogram) {
entry:
  br i1 %has.seed, label %seed.path, label %wrapped.path
seed.path:
  %real = call <32 x i32> asm sideeffect "", "=@2Tr"()
  br label %preheader
wrapped.path:
  %unsigned.empty = call <32 x i32>
      @llvm.linx.experimental.element.view.v32i32(
          <32 x i32> poison, i64 1, i64 1, i64 0, i64 128, i64 25,
          i64 32, i64 1, i64 29)
  br label %preheader
preheader:
  %seed = phi <32 x i32> [ %real, %seed.path ],
                           [ %unsigned.empty, %wrapped.path ]
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %loop ]
  %signed.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %seed, i64 2, i64 2, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %signed.view, i32 %element
  %address = getelementptr i32, ptr %histogram, i32 %index
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

