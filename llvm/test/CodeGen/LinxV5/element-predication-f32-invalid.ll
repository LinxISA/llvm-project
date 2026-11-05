; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/fast-add.ll 2>&1 | FileCheck %s --check-prefix=FASTADD
; FASTADD: floating arithmetic requires f32 add/sub/mul without fast-math
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/one.ll 2>&1 | FileCheck %s --check-prefix=ONE
; ONE: floating comparison predicate has no exact native mapping
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/ueq.ll 2>&1 | FileCheck %s --check-prefix=UEQ
; UEQ: floating comparison predicate has no exact native mapping
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/fast-cmp.ll 2>&1 | FileCheck %s --check-prefix=FASTCMP
; FASTCMP: floating comparison requires f32 without fast-math
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/fast-select.ll 2>&1 | FileCheck %s --check-prefix=FASTSELECT
; FASTSELECT: floating select requires no fast-math
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/cast.ll 2>&1 | FileCheck %s --check-prefix=CAST
; CAST: only integer casts are supported
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/f64.ll 2>&1 | FileCheck %s --check-prefix=F64
; F64: requires scalar integer or pointer results, f32 or typed carrier results
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -S %t/gm.ll | FileCheck %s --check-prefix=GM
; GM: @llvm.linx.experimental.ew.mgather.gpr.masked.v32f32
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/constrained.ll 2>&1 | FileCheck %s --check-prefix=CONSTRAINED
; CONSTRAINED: unsupported opcode or effect in generic IR stage
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/strictfp.ll 2>&1 | FileCheck %s --check-prefix=STRICTFP
; STRICTFP: strictfp/fenv requires constrained floating-point lowering
; Unsupported FP cases are diagnosed; none fall back to a kernel recognizer.

;--- fast-add.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global)  {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %result = fadd fast float %value, 1.0
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- one.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global)  {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %condition = fcmp one float %value, 0.0
  %result = select i1 %condition, float %value, float -0.0
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- fast-cmp.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global)  {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %condition = fcmp nnan ogt float %value, 0.0
  %result = select i1 %condition, float %value, float -0.0
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- ueq.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
define void @bad() {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %condition = fcmp ueq float %value, 0.0
  %result = select i1 %condition, float 11.0, float 22.0
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- fast-select.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global)  {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %result = select nnan i1 true, float %value, float -0.0
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- cast.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global)  {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %converted = fptosi float %value to i32
  %result = sitofp i32 %converted to float
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- f64.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global)  {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %wide = fadd double 0.0, 1.0
  %result = fptrunc double %wide to float
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- gm.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global)  {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %address = getelementptr float, ptr %global, i32 %element
  %result = load float, ptr %address
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- constrained.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global)  {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %result = call float @llvm.experimental.constrained.fadd.f32(float %value, float 1.0, metadata !"round.tonearest", metadata !"fpexcept.strict")
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- strictfp.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
define void @bad(ptr %global) strictfp {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %loop ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %result = fadd float %value, 1.0
  %insert = insertelement <32 x float> %carrier, float %result, i32 %element
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %insert, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
