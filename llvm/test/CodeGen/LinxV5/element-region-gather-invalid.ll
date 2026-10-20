; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %t/base.ll | FileCheck %s --check-prefix=POSITIVE --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/br i1 %active, label %load, label %merge/br i1 %active, label %merge, label %load/' %t/base.ll > %t/false-edge.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/false-edge.ll | FileCheck %s --check-prefix=FALSE --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/%active = icmp ult i32 %i, %valid/%active = icmp ult i32 %other, %valid/' %t/base.ll > %t/wrong-iv.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/wrong-iv.ll | FileCheck %s --check-prefix=INDEX --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/%active = icmp ult i32 %i, %valid/%active = icmp ult i32 %i, %other/' %t/base.ll > %t/varying.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/varying.ll | FileCheck %s --check-prefix=VARYING --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/%wide = zext i32 %index to i64/%wide = sext i32 %index to i64/' %t/base.ll > %t/sext.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/sext.ll | FileCheck %s --check-prefix=SIGNED --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/getelementptr i32, ptr %base/getelementptr i64, ptr %base/' %t/base.ll > %t/gep64.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/gep64.ll | FileCheck %s --check-prefix=STRIDE --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/; CLOBBER/store i32 0, ptr %base/' %t/base.ll > %t/clobber.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/clobber.ll 2>&1 | FileCheck %s --check-prefix=CLOBBER
; RUN: sed 's/; EXTRA/%extra = load i32, ptr %base, align 4/' %t/base.ll > %t/extra.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/extra.ll | FileCheck %s --check-prefix=EXTRA --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement

; POSITIVE: llvm.linx.experimental.ew.mgather.gpr.masked
; POSITIVE-NOT: extractelement
; POSITIVE-NOT: insertelement
; FALSE: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; FALSE: xor i64
; FALSE: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; INDEX: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
; INDEX: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; INDEX: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; VARYING: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; SIGNED: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, {{.*}}i64 32)
; SIGNED: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 25, i64 0, i64 29, i64 16,
; STRIDE: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32({{.*}}i64 64)
; STRIDE: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; EXTRA: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; EXTRA-NOT: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; EXTRA: ret void
; CLOBBER: PTO element region: predication: unordered scatter requires the unordered/source loop contract

;--- base.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @gather_cfg(ptr %base, i32 %valid) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %merge ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %merge ]
  %other = add i32 %i, 1
  ; CLOBBER
  ; EXTRA
  %active = icmp ult i32 %i, %valid
  br i1 %active, label %load, label %merge
load:
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %indices, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %i
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %base, i64 %wide
  %loaded = load i32, ptr %address, align 4
  br label %merge
merge:
  %value = phi i32 [ %loaded, %load ], [ 0, %loop ]
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
