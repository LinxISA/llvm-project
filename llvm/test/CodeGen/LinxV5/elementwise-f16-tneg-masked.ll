; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s
; Raw FP16 encodings cover -0, qNaN payload 0x255, and sNaN payload 0x155.
; Selecting TNEG proves sign-bit XOR lowering rather than numeric 0-x.

target triple = "linx64v5"

declare <64 x half> @llvm.linx.experimental.ew.tneg.gpr.masked.v64f16(
    i64, i64, i64, i64, <64 x half>, i64)

define void @negative_zero() {
  %input = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 32768, i64 1)
  %neg = call <64 x half> @llvm.linx.experimental.ew.tneg.gpr.masked.v64f16(i64 32, i64 2, i64 4, i64 29, <64 x half> %input, i64 -1)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP16\0AB.DATR M322ND, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<64 x half> %neg)
  ret void
}

define void @quiet_nan_payload() {
  %input = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 32341, i64 1)
  %neg = call <64 x half> @llvm.linx.experimental.ew.tneg.gpr.masked.v64f16(i64 32, i64 2, i64 4, i64 29, <64 x half> %input, i64 -1)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP16\0AB.DATR M322ND, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<64 x half> %neg)
  ret void
}

define void @signaling_nan_payload() {
  %input = call <64 x half> asm sideeffect "BSTART.TEPL TEXPANDS, FP16\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 32085, i64 1)
  %neg = call <64 x half> @llvm.linx.experimental.ew.tneg.gpr.masked.v64f16(i64 32, i64 2, i64 4, i64 29, <64 x half> %input, i64 -1)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP16\0AB.DATR M322ND, DTYPE_NONE, Null\0AB.DIM zero, 2, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<64 x half> %neg)
  ret void
}

; CHECK-LABEL: <negative_zero>:
; CHECK: BSTART.TEPL TNEG, FP16
; CHECK: C.B.DIMI 2, {{.*}}->lb0
; CHECK: C.B.DIMI 32, {{.*}}->lb1
; CHECK-LABEL: <quiet_nan_payload>:
; CHECK: BSTART.TEPL TNEG, FP16
; CHECK-LABEL: <signaling_nan_payload>:
; CHECK: BSTART.TEPL TNEG, FP16
; CHECK-NOT: TSUB
