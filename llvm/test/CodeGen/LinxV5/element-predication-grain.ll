; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=VP
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=TILE
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=obj -o %t
; RUN: sed 's/i32 256}/i32 192}/' %s > %t.invalid.ll
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t.invalid.ll 2>&1 | FileCheck %s --check-prefix=INVALID
; RUN: sed 's/!1, !2, !3}/!1, !2, !3, !3}/' %s > %t.duplicate.ll
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t.duplicate.ll 2>&1 | FileCheck %s --check-prefix=INVALID
; INVALID: invalid element grain metadata
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @batch_tail(ptr noalias %input, ptr noalias %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %iv = phi i32 [0, %entry], [%next, %latch]
  %source = getelementptr i32, ptr %input, i32 %iv
  %data = load i32, ptr %source, align 4
  %condition = icmp ugt i32 %data, 3
  br i1 %condition, label %body, label %latch
body:
  %result = and i32 %data, 7
  %dest = getelementptr i32, ptr %output, i32 %iv
  store i32 %result, ptr %dest, align 4
  br label %latch
latch:
  %next = add nuw i32 %iv, 1
  %more = icmp ult i32 %next, 129
  br i1 %more, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}
; VP-LABEL: define void @batch_tail
; VP-COUNT-3: call <64 x i32> @llvm.vp.gather
; VP-NOT: phi i32
; VP: ret void
; TILE-LABEL: define void @batch_tail
; TILE: BSTART.TLSU MGATHER, U32
; TILE: BSTART.TEPL TAND, U32
; TILE: BSTART.TLSU MSCATTER, U32
; TILE-NOT: llvm.vp.
; TILE: ret void
!0 = distinct !{}
!1 = distinct !{!1, !2, !3}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{!"llvm.loop.linx.pto.element.grain", i32 256}
