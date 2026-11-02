; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=ASM

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i32> @llvm.vp.add.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)

; CHECK-LABEL: define void @tile_mask_logic
; CHECK: @llvm.linx.experimental.ew.tcmp.v32i32.v32i32.v32i32
; CHECK: @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 27, i64 29, i64 5,
; CHECK: @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 27, i64 29, i64 6,
; CHECK: @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 27, i64 29, i64 7,
; CHECK: @llvm.linx.experimental.ew.tadd.masked.v32i32.v32i32.v32i32.v32i32
; CHECK: @llvm.linx.experimental.ew.tsel.v32i32.v32i32.v32i32.v32i32
; CHECK-NOT: .gpr
; CHECK: ret void
; ASM-LABEL: <tile_mask_logic>:
; ASM: BSTART.TEPL TCMP, U32
; ASM: BSTART.TEPL TAND, U8
; ASM: BSTART.TEPL TOR, U8
; ASM: BSTART.TEPL TXOR, U8
; ASM: BSTART.TEPL TADD, U32
; ASM: BSTART.TEPL TSEL, U32

define void @tile_mask_logic() {
entry:
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 16, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %lt = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %ne = icmp ne <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %both = and <32 x i1> %lt, %ne, !linx.pto.element.vp.owner !0
  %either = or <32 x i1> %both, %lt, !linx.pto.element.vp.owner !0
  %mask = xor <32 x i1> %either, %ne, !linx.pto.element.vp.owner !0
  %sum = call <32 x i32> @llvm.vp.add.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %choice = select <32 x i1> %mask, <32 x i32> %sum, <32 x i32> %av, !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %choice, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}
!0 = distinct !{}
!1 = !{i32 32}

; CHECK-LABEL: define void @tile_mask_numeric_logic
; CHECK: call <32 x i32> asm sideeffect "BSTART.TEPL TAND, U32
; CHECK: call <32 x i32> asm sideeffect "BSTART.TEPL TOR, U32
; CHECK: call <32 x i32> asm sideeffect "BSTART.TEPL TXOR, U32
; CHECK: @llvm.linx.experimental.ew.tsub.masked.v32i32.v32i32.v32i32.v32i32
; CHECK-NOT: .gpr
; CHECK: ret void
; ASM-LABEL: <tile_mask_numeric_logic>:
; ASM: BSTART.TEPL TAND, U32
; ASM-NEXT: B.DATR CUBE_M32, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111
; ASM-NEXT: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111, last,
; ASM: BSTART.TEPL TOR, U32
; ASM: BSTART.TEPL TXOR, U32
; ASM: BSTART.TEPL TSUB, U32
; ASM: C.B.DIMI 1, ->lb0
; ASM: C.B.DIMI 32, ->lb1
define void @tile_mask_numeric_logic() {
entry:
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 16, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %lt = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %ne = icmp ne <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %both = and <32 x i1> %lt, %ne, !linx.pto.element.vp.owner !0
  %either = or <32 x i1> %both, %lt, !linx.pto.element.vp.owner !0
  %mask = xor <32 x i1> %either, %ne, !linx.pto.element.vp.owner !0
  %band = call <32 x i32> @llvm.vp.and.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %bor = call <32 x i32> @llvm.vp.or.v32i32(<32 x i32> %band, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %bxor = call <32 x i32> @llvm.vp.xor.v32i32(<32 x i32> %bor, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %sum = call <32 x i32> @llvm.vp.sub.v32i32(<32 x i32> %bxor, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %choice = select <32 x i1> %mask, <32 x i32> %sum, <32 x i32> %av, !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %choice, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}
declare <32 x i32> @llvm.vp.and.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.or.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.xor.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.sub.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)

; CHECK-LABEL: define void @tile_mask_dense64
; CHECK: @llvm.linx.experimental.ew.tcmp.v32i32.v64i32.v64i32(i64 32, i64 2,
; CHECK: @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 2, i64 27,
; CHECK: @llvm.linx.experimental.ew.tadd.masked.v64i32.v64i32.v64i32.v32i32(i64 32, i64 2,
; CHECK-NOT: .gpr
; CHECK: ret void
; ASM-LABEL: <tile_mask_dense64>:
; ASM: BSTART.TEPL TCMP, U32
; ASM: BSTART.TEPL TAND, U8
; ASM: BSTART.TEPL TADD, U32
; ASM: C.B.DIMI 2, ->lb0
; ASM: C.B.DIMI 32, ->lb1
define void @tile_mask_dense64() {
entry:
  %a = call <64 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 2)
  %b = call <64 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 16, i64 2)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !2
  %av = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %a, i64 1, i64 1, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %b, i64 2, i64 2, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %lt = icmp ult <64 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %ne = icmp ne <64 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %both = and <64 x i1> %lt, %ne, !linx.pto.element.vp.owner !0
  %either = or <64 x i1> %both, %lt, !linx.pto.element.vp.owner !0
  %mask = xor <64 x i1> %either, %ne, !linx.pto.element.vp.owner !0
  %sum = call <64 x i32> @llvm.vp.add.v64i32(<64 x i32> %av, <64 x i32> %bv, <64 x i1> %mask, i32 64), !linx.pto.element.vp.owner !0
  %choice = select <64 x i1> %mask, <64 x i32> %sum, <64 x i32> %av, !linx.pto.element.vp.owner !0
  %published = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %choice, i64 3, i64 3, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x i32> %published)
  ret void
}

!2 = !{i32 64}
declare <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <64 x i32> @llvm.vp.add.v64i32(<64 x i32>, <64 x i32>, <64 x i1>, i32)

; CHECK-LABEL: define void @tile_mask_select
; CHECK: @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 27, i64 29, i64 7,
; CHECK: @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 27, i64 29, i64 5,
; CHECK: @llvm.linx.experimental.ew.tadd.masked
; CHECK-NOT: .gpr
; CHECK: ret void
define void @tile_mask_select() {
entry:
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 16, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %lt = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %ne = icmp ne <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %both = and <32 x i1> %lt, %ne, !linx.pto.element.vp.owner !0
  %either = or <32 x i1> %both, %lt, !linx.pto.element.vp.owner !0
  %mask = xor <32 x i1> %either, %ne, !linx.pto.element.vp.owner !0
  %blend = select <32 x i1> %lt, <32 x i1> %mask, <32 x i1> %ne, !linx.pto.element.vp.owner !0
  %sum = call <32 x i32> @llvm.vp.add.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %blend, i32 32), !linx.pto.element.vp.owner !0
  %choice = select <32 x i1> %mask, <32 x i32> %sum, <32 x i32> %av, !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %choice, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; CHECK-LABEL: define void @tile_mask_vp_logic
; CHECK: @llvm.linx.experimental.ew.tbinary.v32i32(i64 32, i64 1, i64 27, i64 29, i64 5,
; CHECK: @llvm.linx.experimental.ew.tadd.masked
; CHECK-NOT: .gpr
; CHECK: ret void
define void @tile_mask_vp_logic() {
entry:
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 16, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %lt = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %ne = icmp ne <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %both = call <32 x i1> @llvm.vp.and.v32i1(<32 x i1> %lt, <32 x i1> %ne, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !0
  %either = or <32 x i1> %both, %lt, !linx.pto.element.vp.owner !0
  %mask = xor <32 x i1> %either, %ne, !linx.pto.element.vp.owner !0
  %sum = call <32 x i32> @llvm.vp.add.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %choice = select <32 x i1> %mask, <32 x i32> %sum, <32 x i32> %av, !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %choice, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}
declare <32 x i1> @llvm.vp.and.v32i1(<32 x i1>, <32 x i1>, <32 x i1>, i32)
