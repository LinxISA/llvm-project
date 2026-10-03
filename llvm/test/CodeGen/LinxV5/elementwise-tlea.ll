; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s

; TLEA widens logical S/U32 indices before scaling and retains S/U64 input
; signedness.  These four functions also lock distinct source/destination
; Tile capacities: 128 x i32 is 512B while 128 x i64 is 1KB.

; CHECK-LABEL: <tlea_s32>:
; CHECK: BSTART.TEPL TLEA, S32
; CHECK: B.DATR CUBE_M32
; CHECK: C.B.DIMI 4, ->lb0
; CHECK: C.B.DIMI 32, ->lb1
; CHECK: B.IOT t#{{[0-9]+}}, mask=1111, last, ->t<1KB>
; CHECK: B.IOR [{{[a-z0-9]+}}], []

define void @tlea_s32(ptr %out, ptr %in) {
  %indices = load <128 x i32>, ptr %in
  %r = call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i32(
      i64 32, i64 4, i64 17, i64 29, <128 x i32> %indices, i64 32)
  store <128 x i64> %r, ptr %out
  ret void
}

; CHECK-LABEL: <tlea_u32>:
; CHECK: BSTART.TEPL TLEA, U32
define void @tlea_u32(ptr %out, ptr %in) {
  %indices = load <128 x i32>, ptr %in
  %r = call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i32(
      i64 32, i64 4, i64 25, i64 29, <128 x i32> %indices, i64 64)
  store <128 x i64> %r, ptr %out
  ret void
}

; CHECK-LABEL: <tlea_s64>:
; CHECK: BSTART.TEPL TLEA, S64
define void @tlea_s64(ptr %out, ptr %in) {
  %indices = load <128 x i64>, ptr %in
  %r = call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i64(
      i64 32, i64 4, i64 16, i64 29, <128 x i64> %indices, i64 8)
  store <128 x i64> %r, ptr %out
  ret void
}

; CHECK-LABEL: <tlea_u64>:
; CHECK: BSTART.TEPL TLEA, U64
define void @tlea_u64(ptr %out, ptr %in) {
  %indices = load <128 x i64>, ptr %in
  %r = call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i64(
      i64 32, i64 4, i64 24, i64 29, <128 x i64> %indices, i64 16)
  store <128 x i64> %r, ptr %out
  ret void
}

declare <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i32(
    i64, i64, i64, i64, <128 x i32>, i64)
declare <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i64(
    i64, i64, i64, i64, <128 x i64>, i64)
