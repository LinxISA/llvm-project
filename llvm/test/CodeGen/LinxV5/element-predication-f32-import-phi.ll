; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
; Edge-local undefined FP32 seeds are real TEXPANDS producers. A real seed
; retains raw -zero/NaN bits and duplicate edges share one predecessor seed.
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @actual_and_undef(i1 %take.real, i1 %duplicate) {
entry:
  br i1 %take.real, label %real.path, label %empty.path
real.path:
  %actual = call <32 x float> asm sideeffect "", "=@2Tr"()
  br label %preheader
empty.path:
  br label %preheader
preheader:
  %import = phi <32 x float> [ %actual, %real.path ], [ undef, %empty.path ]
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %preheader ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %import, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %element.value = extractelement <32 x float> %view, i32 %element
  %insert = insertelement <32 x float> %carrier, float %element.value, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
; CHECK-LABEL: define void @actual_and_undef
; CHECK: real.path:
; CHECK: [[ACTUAL0:%.*]] = call <32 x float> asm sideeffect "", "=@2Tr"()
; CHECK: empty.path:
; CHECK: [[ZERO0:%.*]] = freeze float 0.000000e+00
; CHECK: [[SEED0:%.*]] = call <32 x float> @llvm.linx.experimental.ew.texpands.v32f32.f32(i64 32, i64 1, i64 1, i64 29, float [[ZERO0]])
; CHECK-NOT: @llvm.linx.experimental.ew.texpands
; CHECK: preheader:
; CHECK: %import = phi <32 x float> [ [[ACTUAL0]], %real.path ], [ [[SEED0]], %empty.path ]
; CHECK-NOT: pto.index.typed
; CHECK: call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %import)
; CHECK: ret void

define void @duplicate_poison(i1 %take.real, i1 %duplicate) {
entry:
  br i1 %take.real, label %real.path, label %empty.path
real.path:
  %actual = call <32 x float> asm sideeffect "", "=@2Tr"()
  br label %preheader
empty.path:
  br i1 %duplicate, label %preheader, label %preheader
preheader:
  %import = phi <32 x float> [ %actual, %real.path ], [ poison, %empty.path ], [ poison, %empty.path ]
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %loop
loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %preheader ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %import, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %element.value = extractelement <32 x float> %view, i32 %element
  %insert = insertelement <32 x float> %carrier, float %element.value, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !4
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
; CHECK-LABEL: define void @duplicate_poison
; CHECK: real.path:
; CHECK: [[ACTUAL1:%.*]] = call <32 x float> asm sideeffect "", "=@2Tr"()
; CHECK: empty.path:
; CHECK: [[ZERO1:%.*]] = freeze float 0.000000e+00
; CHECK: [[SEED1:%.*]] = call <32 x float> @llvm.linx.experimental.ew.texpands.v32f32.f32(i64 32, i64 1, i64 1, i64 29, float [[ZERO1]])
; CHECK-NOT: @llvm.linx.experimental.ew.texpands
; CHECK: preheader:
; CHECK: %import = phi <32 x float> [ [[ACTUAL1]], %real.path ], [ [[SEED1]], %empty.path ], [ [[SEED1]], %empty.path ]
; CHECK-NOT: pto.index.typed
; CHECK: call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %import)
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}
