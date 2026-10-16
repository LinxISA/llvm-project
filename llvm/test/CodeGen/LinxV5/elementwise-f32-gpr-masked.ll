; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s
;
; Native instruction effects, independent of scalar unconstrained speculation.
define i64 @masked_tile_compare(ptr %ap, ptr %bp, i64 %active) {
  %a = load <32 x float>, ptr %ap
  %b = load <32 x float>, ptr %bp
  %r = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 2, i64 %active)
  ret i64 %r
}
; CHECK-LABEL: <masked_tile_compare>:
; CHECK: BSTART.TEPL TCMP, FP32
; CHECK-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, LT, RNONE, nosat, 0, 1
; CHECK: B.IOT {{.*}}, mask=1111, last
; CHECK-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; CHECK-NOT: B.IOR
; CHECK: BSTART.STD RET

define i64 @masked_scalar_zero(ptr %ap, i64 %active) {
  %a = load <32 x float>, ptr %ap
  %r = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 0, i64 3, i64 %active)
  ret i64 %r
}
; CHECK-LABEL: <masked_scalar_zero>:
; CHECK: BSTART.TEPL TCMPS, FP32
; CHECK-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, GT, RNONE, nosat, 0, 1
; CHECK: B.IOT {{.*}}, mask=1111, last
; CHECK-NEXT: {{.*}}B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; CHECK-NOT: B.IOR
; CHECK: BSTART.STD RET

define void @masked_add_sub_select(ptr %ap, ptr %bp, ptr %out, i64 %active) {
  %a = load <32 x float>, ptr %ap
  %b = load <32 x float>, ptr %bp
  %sum = call <32 x float> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, i64 0, <32 x float> %a, <32 x float> %b, i64 %active, i64 0, i64 0, i64 1)
  %difference = call <32 x float> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, i64 1, <32 x float> %a, <32 x float> %b, i64 %active, i64 0, i64 0, i64 1)
  %selected = call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64 32, i64 1, i64 1, i64 29, i64 %active, i64 0, <32 x float> %sum, <32 x float> %difference)
  store <32 x float> %selected, ptr %out
  ret void
}
; CHECK-LABEL: <masked_add_sub_select>:
; CHECK: BSTART.TEPL TADD, FP32
; CHECK: ExecMaskPresent
; CHECK: BSTART.TEPL TSUB, FP32
; CHECK: ExecMaskPresent
; CHECK: BSTART.TEPL TSEL, FP32
; CHECK-NOT: ExecMaskPresent
; CHECK: BSTART.TLSU TSTORE, FP32

declare i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64, i64, i64, i64, <32 x float>, <32 x float>, i64, i64)
declare i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64, i64, i64, i64, <32 x float>, i64, i64, i64)
declare <32 x float> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32(i64, i64, i64, i64, i64, <32 x float>, <32 x float>, i64, i64, i64, i64)
declare <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64, i64, i64, i64, i64, i64, <32 x float>, <32 x float>)
