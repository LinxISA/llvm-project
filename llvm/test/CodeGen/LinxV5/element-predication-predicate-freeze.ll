; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=SSA
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=TILE
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %s | llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true -filetype=obj -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)

define void @frozen_condition(ptr noalias %input, ptr noalias %table,
                             ptr noalias %output, i32 %valid) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %active = icmp ult i32 %element, %valid
  br i1 %active, label %body, label %latch
body:
  %source = getelementptr i32, ptr %input, i32 %element
  %index = load i32, ptr %source, align 4
  %present = icmp ne i32 %index, 0
  %chosen = freeze i1 %present
  br i1 %chosen, label %gather, label %latch
gather:
  %address = getelementptr i8, ptr %table, i32 %index
  %value = load i32, ptr %address, align 4
  %destination = getelementptr i32, ptr %output, i32 %element
  store i32 %value, ptr %destination, align 4
  br label %latch
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}

; SSA-LABEL: define void @frozen_condition
; SSA: [[CHOSEN:%.*]] = freeze <32 x i1>
; SSA: [[ACTIVE:%.*]] = select <32 x i1> {{.*}}, <32 x i1> [[CHOSEN]], <32 x i1> zeroinitializer
; SSA: @llvm.vp.gather.v32i32.v32p0({{.*}}<32 x i1> [[ACTIVE]], i32 32)
; TILE-LABEL: define void @frozen_condition
; TILE: @llvm.linx.experimental.ew.tlea{{.*}}i64 8)
; TILE: @llvm.linx.experimental.ew.mgather.gpr.masked
; TILE-NOT: freeze <32 x i1>
; TILE-NOT: llvm.vp.
; TILE: ret void
; OBJ-LABEL: <frozen_condition>:
; OBJ: BSTART.MGATHER
; OBJ: ExecMaskPresent

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
