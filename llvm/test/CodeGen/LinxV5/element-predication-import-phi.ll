; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=IR
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ
;
; Undef and poison external-carrier PHI inputs have no defining Tile operation.
; Materialize a real zero Tile in each predecessor before the PHI reaches TSEL
; or register allocation.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @undef_poison_import(i32 %choice, ptr noalias %output) {
entry:
  %is.actual = icmp eq i32 %choice, 2
  br i1 %is.actual, label %actual.path, label %empty.test

empty.test:
  %is.undef = icmp eq i32 %choice, 0
  br i1 %is.undef, label %undef.path, label %poison.path

actual.path:
  %full = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${3:D}\0A.if ${7:c} == 3\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Null\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Null\0A.else\0AB.DATR ND2N8.normal, Null\0A.endif\0A.elseif ${7:c} == 2\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Min\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Min\0A.else\0AB.DATR ND2N8.normal, Min\0A.endif\0A.elseif ${7:c} == 1\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Max\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Max\0A.else\0AB.DATR ND2N8.normal, Max\0A.endif\0A.else\0A.if ${8:c} == 21\0AB.DATR ND2M32.normal, Zero\0A.elseif ${8:c} == 22\0AB.DATR ND2M16.normal, Zero\0A.else\0AB.DATR ND2N8.normal, Zero\0A.endif\0A.endif\0AB.DIM zero, ${5:c}, ->lb0\0AB.DIM zero, ${6:c}, ->lb1\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,i,i,i,i,i,~{memory}"(ptr %output, i64 16, i32 25, i32 3, i32 4, i32 32, i32 0, i32 21)
  %actual = call <32 x i32> asm sideeffect "BSTART.TEPL ${10:c}, ${1:D}\0A.if ${11:c} == 29\0AB.DATR CUBE_M32, Null\0A.elseif ${11:c} == 31\0AB.DATR CUBE_M16, Null\0A.endif\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $2, mask=1111, last, ->$0<${6:Z}>\0AB.SUBVIEW 0, $9, 0, ${7:c}\0AB.IOR [$8],[]\0A", "=@2Tr,i,@2Tr,i,i,i,i,i,r,r,i,i,~{memory}"(i32 25, <128 x i32> %full, i32 1, i32 32, i32 1, i32 1, i32 1, i32 0, i64 0, i32 32, i32 29)
  br label %preheader

undef.path:
  br label %preheader

poison.path:
  br label %preheader

preheader:
  %import = phi <32 x i32> [ undef, %undef.path ],
                             [ poison, %poison.path ],
                             [ %actual, %actual.path ]
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %import, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %view, i32 %element
  %address = getelementptr i32, ptr %output, i32 %element
  store i32 %value, ptr %address, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1

exit:
  ret void
}

; IR-LABEL: define void @undef_poison_import
; IR: actual.path:
; IR: [[ACTUAL:%.*]] = call <32 x i32> asm sideeffect
; IR: br label %preheader
; IR: undef.path:
; IR: [[UNDEF_SCALAR:%.*]] = freeze i32 0
; IR: [[UNDEF_BITS:%.*]] = zext i32 [[UNDEF_SCALAR]] to i64
; IR: [[UNDEF_ZERO:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 25, i64 29, i64 [[UNDEF_BITS]], i64 0)
; IR: br label %preheader
; IR: poison.path:
; IR: [[POISON_SCALAR:%.*]] = freeze i32 0
; IR: [[POISON_BITS:%.*]] = zext i32 [[POISON_SCALAR]] to i64
; IR: [[POISON_ZERO:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 25, i64 29, i64 [[POISON_BITS]], i64 0)
; IR: br label %preheader
; IR: preheader:
; IR: %import = phi <32 x i32> [ [[UNDEF_ZERO]], %undef.path ], [ [[POISON_ZERO]], %poison.path ], [ [[ACTUAL]], %actual.path ]
; IR-NOT: phi <32 x i32> [ undef
; IR-NOT: phi <32 x i32> [ poison
; IR-NOT: llvm.linx.experimental.element.
; IR-NOT: llvm.vp.
; IR: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
; IR: ret void
; OBJ-LABEL: <undef_poison_import>:
; OBJ: BSTART.TLSU TLOAD, U32
; OBJ: BSTART.TEPL TCI, U32
; OBJ: BSTART.TLSU TMOV, DTYPE_NONE
; OBJ: BSTART.TEPL TCI, U32
; OBJ: BSTART.TLSU TMOV, DTYPE_NONE
; OBJ: BSTART.TEPL TCI, U32
; OBJ: BSTART.TLSU MSCATTER, U32

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

