; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=PRED
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=FINAL
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ
;
; A loop-invariant pointer is represented as a zero-index vector GEP. The old
; result controls a branch and then becomes the second addend, so predication
; must preserve both the per-element data dependency and effect order.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)

define void @uniform_pointer_ordered(ptr noalias %counter,
                                     ptr noalias %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i64 [ 0, %entry ], [ %next, %latch ]
  %first.old = atomicrmw add ptr %counter, i32 1 monotonic, align 4
  %was.zero = icmp eq i32 %first.old, 0
  br i1 %was.zero, label %second.block, label %merge

second.block:
  %second.old = atomicrmw add ptr %counter, i32 %first.old monotonic, align 4
  br label %merge

merge:
  %observed = phi i32 [ %second.old, %second.block ], [ %first.old, %header ]
  %out.address = getelementptr i32, ptr %output, i64 %element
  store i32 %observed, ptr %out.address, align 4
  br label %latch

latch:
  %next = add nuw nsw i64 %element, 1
  %more = icmp ult i64 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  ret void
}

; PRED-LABEL: define void @uniform_pointer_ordered
; PRED: [[UNIFORM_FIRST:%.*]] = call <32 x i32> @llvm.linx.experimental.element.atomic.add
; PRED: icmp eq <32 x i32> [[UNIFORM_FIRST]], zeroinitializer
; PRED: call <32 x i32> @llvm.linx.experimental.element.atomic.add{{.*}}<32 x i32> [[UNIFORM_FIRST]],
; PRED: call void @llvm.vp.scatter
; PRED-NOT: atomicrmw
; PRED: ret void
; FINAL-LABEL: define void @uniform_pointer_ordered
; FINAL: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> {{.*}}, i64 32)
; FINAL: [[UNIFORM_FIRST_TILE:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; FINAL: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked{{.*}}<32 x i32> [[UNIFORM_FIRST_TILE]],
; FINAL: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, <32 x i32> {{.*}}, i64 32)
; FINAL: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
; FINAL-NOT: llvm.linx.experimental.element.atomic.add
; FINAL-NOT: llvm.vp.
; FINAL-NOT: atomicrmw
; FINAL: ret void
; OBJ-LABEL: <uniform_pointer_ordered>:
; OBJ: BSTART.TEPL TLEA, S32
; OBJ: BSTART.TLSU MGATHER.ADD, U32
; OBJ: BSTART.TLSU MGATHER.ADD, U32
; OBJ: BSTART.TEPL TLEA, U32
; OBJ: BSTART.TLSU MSCATTER, U32

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

