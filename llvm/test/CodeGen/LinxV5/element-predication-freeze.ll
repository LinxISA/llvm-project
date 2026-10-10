; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=SSA
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=TILE
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ
;
; LLVM introduces freeze when sharing division/remainder operands. Preserve
; one element's chosen value in vector SSA, then reuse the fully initialized
; masked-gather Tile. Inactive input poison has a defined physical zero.

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)

define void @frozen_divrem(ptr noalias %input, ptr noalias %output,
                          i32 %divisor, i32 %valid) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %active = icmp ult i32 %element, %valid
  br i1 %active, label %body, label %latch
body:
  %source = getelementptr i32, ptr %input, i32 %element
  %loaded = load i32, ptr %source, align 4
  %chosen = freeze i32 %loaded
  %quotient = udiv i32 %chosen, %divisor
  %remainder = urem i32 %chosen, %divisor
  %sum = add i32 %quotient, %remainder
  %destination = getelementptr i32, ptr %output, i32 %element
  store i32 %sum, ptr %destination, align 4
  br label %latch
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}

; SSA-LABEL: define void @frozen_divrem
; SSA: [[LOADED:%.*]] = call <32 x i32> @llvm.vp.gather
; SSA: [[CHOSEN:%.*]] = freeze <32 x i32> [[LOADED]]
; SSA: @llvm.vp.udiv.v32i32(<32 x i32> [[CHOSEN]],
; SSA: @llvm.vp.urem.v32i32(<32 x i32> [[CHOSEN]],
; TILE-LABEL: define void @frozen_divrem
; TILE: [[INPUT:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; TILE: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 3, <32 x i32> [[INPUT]],
; TILE: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 4, <32 x i32> [[INPUT]],
; TILE-NOT: freeze <32 x
; TILE-NOT: llvm.vp.
; TILE-NOT: llvm.linx.experimental.element.
; TILE: ret void
; OBJ-LABEL: <frozen_divrem>:
; OBJ: BSTART.TLSU MGATHER, U32
; OBJ: BSTART.TEPL TDIV, U32
; OBJ: BSTART.TLSU MSCATTER, U32

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
