; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s

; The compiler opcode is dense 0..9.  The backend maps it to the sparse PTO
; selector space (selector 5 is reserved) and keeps every intermediate in the
; native Tile register class.

; CHECK-LABEL: <element_binary_chain>:
; CHECK: BSTART.TEPL TADD, U32
; CHECK-NEXT: B.DATR CUBE_M32
; CHECK-NEXT: C.B.DIMI 1, ->lb0
; CHECK-NEXT: C.B.DIMI 32, ->lb1
; CHECK-NEXT: B.IOT {{[tmnu]}}#{{[0-9]+}}, {{[tmnu]}}#{{[0-9]+}}, mask=1111, last, ->{{[tmnu]}}<128B>
; CHECK: BSTART.TEPL TSUB, U32
; CHECK: BSTART.TEPL TMUL, U32
; CHECK: BSTART.TEPL TDIV, U32
; CHECK: BSTART.TEPL TREM, U32
; CHECK: BSTART.TEPL TAND, U32
; CHECK: BSTART.TEPL TOR, U32
; CHECK: BSTART.TEPL TXOR, U32
; CHECK: BSTART.TEPL TSHL, U32
; CHECK: BSTART.TEPL TSHR, U32
; CHECK-NOT: BSTART.TEPL 5,

define void @element_binary_chain(ptr %out, ptr %lhs_ptr, ptr %rhs_ptr) {
  %lhs = load <32 x i32>, ptr %lhs_ptr
  %rhs = load <32 x i32>, ptr %rhs_ptr
  %add = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 0,
      <32 x i32> %lhs, <32 x i32> %rhs)
  %sub = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 1,
      <32 x i32> %add, <32 x i32> %rhs)
  %mul = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 2,
      <32 x i32> %sub, <32 x i32> %lhs)
  %div = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 3,
      <32 x i32> %mul, <32 x i32> %rhs)
  %rem = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 4,
      <32 x i32> %div, <32 x i32> %lhs)
  %and = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 5,
      <32 x i32> %rem, <32 x i32> %rhs)
  %or = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 6,
      <32 x i32> %and, <32 x i32> %lhs)
  %xor = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 7,
      <32 x i32> %or, <32 x i32> %rhs)
  %shl = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 8,
      <32 x i32> %xor, <32 x i32> %lhs)
  %shr = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 9,
      <32 x i32> %shl, <32 x i32> %rhs)
  store <32 x i32> %shr, ptr %out
  ret void
}

declare <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
    i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>)
