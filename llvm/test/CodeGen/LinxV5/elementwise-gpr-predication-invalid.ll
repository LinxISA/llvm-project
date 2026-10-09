; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tbinary.ll 2>&1 | FileCheck %s --check-prefix=TBINARY
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tcmp.ll 2>&1 | FileCheck %s --check-prefix=TCMP
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tsel.ll 2>&1 | FileCheck %s --check-prefix=TSEL
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/mscatter.ll 2>&1 | FileCheck %s --check-prefix=MSCATTER

; TBINARY: GPR-masked element binary requires exact 32x1 S32/U32 CUBE_M32 Tiles, one low mask word and zero inactive lanes
; TCMP: TCMP GPR requires exact 32x1 S32/U32 CUBE_M32 Tiles
; TSEL: TSEL GPR requires exact 32x1 S32/U32 CUBE_M32 Tiles and one low mask word
; MSCATTER: GPR-masked MSCATTER requires exact 32x1 U32 values, S64/U64 byte offsets, CUBE_M32, one low mask word and merge-inactive stores

;--- tbinary.ll
define void @bad(ptr %out, ptr %ap, ptr %bp, i64 %m) {
  %a = load <32 x i32>, ptr %ap
  %b = load <32 x i32>, ptr %bp
  %r = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 0, <32 x i32> %a, <32 x i32> %b, i64 %m, i64 1, i64 0, i64 1)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>, i64, i64, i64, i64)

;--- tcmp.ll
define i64 @bad(ptr %ap, ptr %bp) {
  %a = load <32 x i32>, ptr %ap
  %b = load <32 x i32>, ptr %bp
  %r = call i64 @llvm.linx.experimental.ew.tcmp.gpr.v32i32(i64 16, i64 2, i64 25, i64 29, <32 x i32> %a, <32 x i32> %b, i64 0)
  ret i64 %r
}
declare i64 @llvm.linx.experimental.ew.tcmp.gpr.v32i32(i64, i64, i64, i64, <32 x i32>, <32 x i32>, i64)

;--- tsel.ll
define void @bad(ptr %out, ptr %ap, ptr %bp, i64 %m) {
  %a = load <32 x i32>, ptr %ap
  %b = load <32 x i32>, ptr %bp
  %r = call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32(i64 32, i64 1, i64 25, i64 29, i64 %m, i64 1, <32 x i32> %a, <32 x i32> %b)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32(i64, i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>)

;--- mscatter.ll
define void @bad(ptr %base, ptr %offset.ptr, ptr %value.ptr, i64 %m) {
  %offsets = load <32 x i64>, ptr %offset.ptr
  %values = load <32 x i32>, ptr %value.ptr
  call void @llvm.linx.experimental.ew.mscatter.gpr.masked.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, i64 24, ptr %base, <32 x i64> %offsets, <32 x i32> %values, i64 %m, i64 0, i64 0, i64 1)
  ret void
}
declare void @llvm.linx.experimental.ew.mscatter.gpr.masked.v32i64.v32i32(i64, i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>, i64, i64, i64, i64)
