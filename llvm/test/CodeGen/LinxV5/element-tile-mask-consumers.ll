; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s -o %t.ll
; RUN: FileCheck %s --check-prefix=IR < %t.ll
; RUN: not grep -E "call .*ew\..*gpr|ExecMaskPresent" %t.ll
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %t.ll -o %t.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x float> @llvm.vp.fmul.v32float(<32 x float>, <32 x float>, <32 x i1>, i32)
declare <32 x float> @llvm.vp.fneg.v32float(<32 x float>, <32 x i1>, i32)
declare <32 x i1> @llvm.vp.fcmp.v32float(<32 x float>, <32 x float>, metadata, <32 x i1>, i32)
declare <32 x i32> @llvm.linx.experimental.element.atomic.add.v32i32.v32p0(<32 x ptr>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i32> @llvm.vp.and.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.ashr.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.gather.v32i32.v32p0(<32 x ptr>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.lshr.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.mul.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.or.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.sdiv.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.shl.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.srem.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.trunc.v32i32.v32i64(<32 x i64>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.udiv.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.urem.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i32> @llvm.vp.xor.v32i32(<32 x i32>, <32 x i32>, <32 x i1>, i32)
declare <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i64> @llvm.vp.and.v32i64(<32 x i64>, <32 x i64>, <32 x i1>, i32)
declare <32 x i64> @llvm.vp.or.v32i64(<32 x i64>, <32 x i64>, <32 x i1>, i32)
declare <32 x i64> @llvm.vp.xor.v32i64(<32 x i64>, <32 x i64>, <32 x i1>, i32)
declare <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <64 x half> @llvm.vp.fmul.v64half(<64 x half>, <64 x half>, <64 x i1>, i32)
declare <64 x half> @llvm.vp.fneg.v64half(<64 x half>, <64 x i1>, i32)
declare <64 x i1> @llvm.vp.fcmp.v64half(<64 x half>, <64 x half>, metadata, <64 x i1>, i32)
declare <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <64 x i32> @llvm.vp.and.v64i32(<64 x i32>, <64 x i32>, <64 x i1>, i32)
declare <64 x i32> @llvm.vp.or.v64i32(<64 x i32>, <64 x i32>, <64 x i1>, i32)
declare <64 x i32> @llvm.vp.xor.v64i32(<64 x i32>, <64 x i32>, <64 x i1>, i32)
declare void @llvm.vp.scatter.v32i32.v32p0(<32 x i32>, <32 x ptr>, <32 x i1>, i32)

; IR-LABEL: define void @mask_mul
; IR-NOT: .gpr
; OBJ-LABEL: <mask_mul>:
define void @mask_mul(ptr %base) {
; OBJ: BSTART.TEPL TMUL
; OBJ-NEXT: B.DATR CUBE_M32, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111
; OBJ-NEXT: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111, last,
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.mul.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_udiv
; IR-NOT: .gpr
; OBJ-LABEL: <mask_udiv>:
define void @mask_udiv(ptr %base) {
; OBJ: BSTART.TEPL TDIV, U32
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.udiv.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_sdiv
; IR-NOT: .gpr
; OBJ-LABEL: <mask_sdiv>:
define void @mask_sdiv(ptr %base) {
; OBJ: BSTART.TEPL TDIV, S32
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.sdiv.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_urem
; IR-NOT: .gpr
; OBJ-LABEL: <mask_urem>:
define void @mask_urem(ptr %base) {
; OBJ: BSTART.TEPL TREM
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.urem.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_srem
; IR-NOT: .gpr
; OBJ-LABEL: <mask_srem>:
define void @mask_srem(ptr %base) {
; OBJ: BSTART.TEPL TDIV
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.srem.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_and
; IR-NOT: .gpr
; OBJ-LABEL: <mask_and>:
define void @mask_and(ptr %base) {
; OBJ: BSTART.TEPL TAND
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.and.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_or
; IR-NOT: .gpr
; OBJ-LABEL: <mask_or>:
define void @mask_or(ptr %base) {
; OBJ: BSTART.TEPL TOR
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.or.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_xor
; IR-NOT: .gpr
; OBJ-LABEL: <mask_xor>:
define void @mask_xor(ptr %base) {
; OBJ: BSTART.TEPL TXOR
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.xor.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_shl
; IR-NOT: .gpr
; OBJ-LABEL: <mask_shl>:
define void @mask_shl(ptr %base) {
; OBJ: BSTART.TEPL TSHL
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.shl.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_lshr
; IR-NOT: .gpr
; OBJ-LABEL: <mask_lshr>:
define void @mask_lshr(ptr %base) {
; OBJ: BSTART.TEPL TSHR
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.lshr.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @mask_ashr
; IR-NOT: .gpr
; OBJ-LABEL: <mask_ashr>:
define void @mask_ashr(ptr %base) {
; OBJ: BSTART.TEPL TSHR
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.ashr.v32i32(<32 x i32> %av, <32 x i32> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @dense_and
; IR-NOT: .gpr
; OBJ-LABEL: <dense_and>:
define void @dense_and(ptr %base) {
; OBJ: BSTART.TEPL TAND
  %a = call <64 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 2)
  %b = call <64 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 2)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %a, i64 1, i64 1, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %b, i64 2, i64 2, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <64 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <64 x i32> @llvm.vp.and.v64i32(<64 x i32> %av, <64 x i32> %bv, <64 x i1> %mask, i32 64), !linx.pto.element.vp.owner !0
  %published = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %result, i64 3, i64 3, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x i32> %published)
  ret void
}

; IR-LABEL: define void @wide_and
; IR-NOT: .gpr
; OBJ-LABEL: <wide_and>:
define void @wide_and(ptr %base) {
; OBJ: BSTART.TEPL TAND
  %a = call <32 x i64> asm sideeffect "BSTART.TEPL TEXPANDS, U64\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 2)
  %b = call <32 x i64> asm sideeffect "BSTART.TEPL TEXPANDS, U64\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 2)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %a, i64 1, i64 1, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %b, i64 2, i64 2, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i64> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i64> @llvm.vp.and.v32i64(<32 x i64> %av, <32 x i64> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %result, i64 3, i64 3, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i64> %published)
  ret void
}

; IR-LABEL: define void @dense_or
; IR-NOT: .gpr
; OBJ-LABEL: <dense_or>:
define void @dense_or(ptr %base) {
; OBJ: BSTART.TEPL TOR
  %a = call <64 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 2)
  %b = call <64 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 2)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %a, i64 1, i64 1, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %b, i64 2, i64 2, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <64 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <64 x i32> @llvm.vp.or.v64i32(<64 x i32> %av, <64 x i32> %bv, <64 x i1> %mask, i32 64), !linx.pto.element.vp.owner !0
  %published = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %result, i64 3, i64 3, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x i32> %published)
  ret void
}

; IR-LABEL: define void @wide_or
; IR-NOT: .gpr
; OBJ-LABEL: <wide_or>:
define void @wide_or(ptr %base) {
; OBJ: BSTART.TEPL TOR
  %a = call <32 x i64> asm sideeffect "BSTART.TEPL TEXPANDS, U64\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 2)
  %b = call <32 x i64> asm sideeffect "BSTART.TEPL TEXPANDS, U64\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 2)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %a, i64 1, i64 1, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %b, i64 2, i64 2, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i64> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i64> @llvm.vp.or.v32i64(<32 x i64> %av, <32 x i64> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %result, i64 3, i64 3, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i64> %published)
  ret void
}

; IR-LABEL: define void @dense_xor
; IR-NOT: .gpr
; OBJ-LABEL: <dense_xor>:
define void @dense_xor(ptr %base) {
; OBJ: BSTART.TEPL TXOR
  %a = call <64 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 2)
  %b = call <64 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 2)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %a, i64 1, i64 1, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %b, i64 2, i64 2, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <64 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <64 x i32> @llvm.vp.xor.v64i32(<64 x i32> %av, <64 x i32> %bv, <64 x i1> %mask, i32 64), !linx.pto.element.vp.owner !0
  %published = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(<64 x i32> %result, i64 3, i64 3, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x i32> %published)
  ret void
}

; IR-LABEL: define void @wide_xor
; IR-NOT: .gpr
; OBJ-LABEL: <wide_xor>:
define void @wide_xor(ptr %base) {
; OBJ: BSTART.TEPL TXOR
  %a = call <32 x i64> asm sideeffect "BSTART.TEPL TEXPANDS, U64\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 2)
  %b = call <32 x i64> asm sideeffect "BSTART.TEPL TEXPANDS, U64\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 2)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %a, i64 1, i64 1, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %b, i64 2, i64 2, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i64> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i64> @llvm.vp.xor.v32i64(<32 x i64> %av, <32 x i64> %bv, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %result, i64 3, i64 3, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i64> %published)
  ret void
}

; IR-LABEL: define void @fp32_fmul
; IR-NOT: .gpr
; OBJ-LABEL: <fp32_fmul>:
define void @fp32_fmul(ptr %base) {
; OBJ: BSTART.TEPL TMUL
  %a = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %result = call <32 x float> @llvm.vp.fmul.v32float(<32 x float> %av, <32 x float> %bv, <32 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %result, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x float> %published)
  ret void
}

; IR-LABEL: define void @fp32_fneg
; IR-NOT: .gpr
; OBJ-LABEL: <fp32_fneg>:
define void @fp32_fneg(ptr %base) {
; OBJ: BSTART.TEPL TNEG
  %a = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %result = call <32 x float> @llvm.vp.fneg.v32float(<32 x float> %av, <32 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %result, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x float> %published)
  ret void
}

; IR-LABEL: define void @fp32_oeq
; IR-NOT: .gpr
; OBJ-LABEL: <fp32_oeq>:
define void @fp32_oeq(ptr %base) {
; OBJ: BSTART.TEPL TCMP, FP32
  %a = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32float(<32 x float> %av, <32 x float> %bv, metadata !"oeq", <32 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false>, i32 19), !linx.pto.element.vp.owner !0
  %result = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %result, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x float> %published)
  ret void
}

; IR-LABEL: define void @fp32_une
; IR-NOT: .gpr
; OBJ-LABEL: <fp32_une>:
define void @fp32_une(ptr %base) {
; OBJ: BSTART.TEPL TCMP, FP32
  %a = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32float(<32 x float> %av, <32 x float> %bv, metadata !"une", <32 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false>, i32 19), !linx.pto.element.vp.owner !0
  %result = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %result, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x float> %published)
  ret void
}

; IR-LABEL: define void @fp32_olt
; IR-NOT: .gpr
; OBJ-LABEL: <fp32_olt>:
define void @fp32_olt(ptr %base) {
; OBJ: BSTART.TEPL TCMP, FP32
  %a = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32float(<32 x float> %av, <32 x float> %bv, metadata !"olt", <32 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false>, i32 19), !linx.pto.element.vp.owner !0
  %result = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %result, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x float> %published)
  ret void
}

; IR-LABEL: define void @fp32_uge
; IR-NOT: .gpr
; OBJ-LABEL: <fp32_uge>:
define void @fp32_uge(ptr %base) {
; OBJ: BSTART.TEPL TCMP, FP32
  %a = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32float(<32 x float> %av, <32 x float> %bv, metadata !"uge", <32 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false>, i32 19), !linx.pto.element.vp.owner !0
  %result = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32float(<32 x float> %result, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x float> %published)
  ret void
}

; IR-LABEL: define void @fp16_fmul
; IR-NOT: .gpr
; OBJ-LABEL: <fp16_fmul>:
define void @fp16_fmul(ptr %base) {
; OBJ: BSTART.TEPL TMUL
  %a = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %result = call <64 x half> @llvm.vp.fmul.v64half(<64 x half> %av, <64 x half> %bv, <64 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true>, i32 64), !linx.pto.element.vp.owner !0
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %result, i64 3, i64 3, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x half> %published)
  ret void
}

; IR-LABEL: define void @fp16_fneg
; IR-NOT: .gpr
; OBJ-LABEL: <fp16_fneg>:
define void @fp16_fneg(ptr %base) {
; OBJ: BSTART.TEPL TNEG
  %a = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %result = call <64 x half> @llvm.vp.fneg.v64half(<64 x half> %av, <64 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true>, i32 64), !linx.pto.element.vp.owner !0
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %result, i64 3, i64 3, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x half> %published)
  ret void
}

; IR-LABEL: define void @fp16_oeq
; IR-NOT: .gpr
; OBJ-LABEL: <fp16_oeq>:
define void @fp16_oeq(ptr %base) {
; OBJ: BSTART.TEPL TCMP, FP16
  %a = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <64 x i1> @llvm.vp.fcmp.v64half(<64 x half> %av, <64 x half> %bv, metadata !"oeq", <64 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true>, i32 19), !linx.pto.element.vp.owner !0
  %result = select <64 x i1> %cmp, <64 x half> %av, <64 x half> %bv, !linx.pto.element.vp.owner !0
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %result, i64 3, i64 3, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x half> %published)
  ret void
}

; IR-LABEL: define void @fp16_une
; IR-NOT: .gpr
; OBJ-LABEL: <fp16_une>:
define void @fp16_une(ptr %base) {
; OBJ: BSTART.TEPL TCMP, FP16
  %a = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <64 x i1> @llvm.vp.fcmp.v64half(<64 x half> %av, <64 x half> %bv, metadata !"une", <64 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true>, i32 19), !linx.pto.element.vp.owner !0
  %result = select <64 x i1> %cmp, <64 x half> %av, <64 x half> %bv, !linx.pto.element.vp.owner !0
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %result, i64 3, i64 3, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x half> %published)
  ret void
}

; IR-LABEL: define void @fp16_olt
; IR-NOT: .gpr
; OBJ-LABEL: <fp16_olt>:
define void @fp16_olt(ptr %base) {
; OBJ: BSTART.TEPL TCMP, FP16
  %a = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <64 x i1> @llvm.vp.fcmp.v64half(<64 x half> %av, <64 x half> %bv, metadata !"olt", <64 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true>, i32 19), !linx.pto.element.vp.owner !0
  %result = select <64 x i1> %cmp, <64 x half> %av, <64 x half> %bv, !linx.pto.element.vp.owner !0
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %result, i64 3, i64 3, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x half> %published)
  ret void
}

; IR-LABEL: define void @fp16_uge
; IR-NOT: .gpr
; OBJ-LABEL: <fp16_uge>:
define void @fp16_uge(ptr %base) {
; OBJ: BSTART.TEPL TCMP, FP16
  %a = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !64
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <64 x i1> @llvm.vp.fcmp.v64half(<64 x half> %av, <64 x half> %bv, metadata !"uge", <64 x i1> <i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true, i1 false, i1 false, i1 true>, i32 19), !linx.pto.element.vp.owner !0
  %result = select <64 x i1> %cmp, <64 x half> %av, <64 x half> %bv, !linx.pto.element.vp.owner !0
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64half(<64 x half> %result, i64 3, i64 3, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<64 x half> %published)
  ret void
}

; IR-LABEL: define void @narrow
; IR-NOT: .gpr
; OBJ-LABEL: <narrow>:
define void @narrow(ptr %base) {
; OBJ: BSTART.TEPL TCVT
  %a = call <32 x i64> asm sideeffect "BSTART.TEPL TEXPANDS, U64\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 2)
  %b = call <32 x i64> asm sideeffect "BSTART.TEPL TEXPANDS, U64\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 2)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %a, i64 1, i64 1, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(<32 x i64> %b, i64 2, i64 2, i64 0, i64 256, i64 24, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i64> %av, %bv, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.trunc.v32i32.v32i64(<32 x i64> %av, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @memory_gather
; IR-NOT: .gpr
; OBJ-LABEL: <memory_gather>:
define void @memory_gather(ptr %base) {
; OBJ: BSTART.{{(TLSU )?}}MGATHER{{,?}} U32
; IR: asm sideeffect "BSTART.TLSU MGATHER, U32
; IR-SAME: ~{memory}
; OBJ: C.B.DIMI 1, {{.*}}->lb2
; OBJ: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111, last,
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %base.one = insertelement <32 x ptr> poison, ptr %base, i32 0, !linx.pto.element.vp.owner !0
  %bases = shufflevector <32 x ptr> %base.one, <32 x ptr> poison, <32 x i32> zeroinitializer, !linx.pto.element.vp.owner !0
  %addresses = getelementptr i32, <32 x ptr> %bases, <32 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23, i32 24, i32 25, i32 26, i32 27, i32 28, i32 29, i32 30, i32 31>, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.vp.gather.v32i32.v32p0(<32 x ptr> %addresses, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @memory_scatter
; IR-NOT: .gpr
; OBJ-LABEL: <memory_scatter>:
define void @memory_scatter(ptr %base) {
; OBJ: BSTART.{{(TLSU )?}}MSCATTER{{,?}} U32
; IR: asm sideeffect "BSTART.TLSU MSCATTER, U32
; IR-SAME: ~{memory}
; OBJ: C.B.DIMI 1, {{.*}}->lb2
; OBJ: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111
; OBJ-NEXT: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111, last
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %base.one = insertelement <32 x ptr> poison, ptr %base, i32 0, !linx.pto.element.vp.owner !0
  %bases = shufflevector <32 x ptr> %base.one, <32 x ptr> poison, <32 x i32> zeroinitializer, !linx.pto.element.vp.owner !0
  %addresses = getelementptr i32, <32 x ptr> %bases, <32 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23, i32 24, i32 25, i32 26, i32 27, i32 28, i32 29, i32 30, i32 31>, !linx.pto.element.vp.owner !0
  call void @llvm.vp.scatter.v32i32.v32p0(<32 x i32> %av, <32 x ptr> %addresses, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %result = freeze <32 x i32> %av, !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

; IR-LABEL: define void @memory_atomic
; IR-NOT: .gpr
; OBJ-LABEL: <memory_atomic>:
define void @memory_atomic(ptr %base) {
; OBJ: BSTART.{{(TLSU )?}}MGATHER.ADD{{,?}} U32
; IR: asm sideeffect "BSTART.TLSU MGATHER.ADD, U32
; IR-SAME: ~{memory}
; OBJ: C.B.DIMI 1, {{.*}}->lb2
; OBJ: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111
; OBJ-NEXT: B.IOT {{[tmnu]}}#{{[0-9]+}}{{(\.reuse)?}}, mask=1111, last,
  %a = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4, i64 1)
  %b = call <32 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 1, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2, i64 1)
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !32
  %av = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %a, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %b, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %mask = icmp ult <32 x i32> %av, %bv, !linx.pto.element.vp.owner !0
  %base.one = insertelement <32 x ptr> poison, ptr %base, i32 0, !linx.pto.element.vp.owner !0
  %bases = shufflevector <32 x ptr> %base.one, <32 x ptr> poison, <32 x i32> zeroinitializer, !linx.pto.element.vp.owner !0
  %addresses = getelementptr i32, <32 x ptr> %bases, <32 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23, i32 24, i32 25, i32 26, i32 27, i32 28, i32 29, i32 30, i32 31>, !linx.pto.element.vp.owner !0
  %result = call <32 x i32> @llvm.linx.experimental.element.atomic.add.v32i32.v32p0(<32 x ptr> align 4 %addresses, <32 x i32> %av, <32 x i1> %mask, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %result, i64 3, i64 3, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}

!0 = distinct !{}
!32 = !{i32 32}
!64 = !{i32 64}
