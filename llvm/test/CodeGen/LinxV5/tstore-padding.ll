; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=asm %s -o %t.s
; RUN: llvm-mc -triple=linx64v5 -mcpu=janus -filetype=obj %t.s -o %t.roundtrip.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.roundtrip.o | FileCheck %s

; Ordinary intrinsic stores must satisfy the schema's PadValue=Zero rule in
; direct object emission and when reassembling the legacy pseudo spelling.
; CHECK-LABEL: <ordinary_store>:
; CHECK: BSTART.TSTORE FP32
; CHECK-NEXT: B.DATR NORM.normal, Zero
define void @ordinary_store(ptr %src, ptr %dst) {
  %t = call <32 x float> @llvm.linx.blk.tload.v32f32(i64 32, i64 1, i64 32, i64 1, i64 0, i64 0, ptr %src, i64 128)
  call void @llvm.linx.blk.tstore.v32f32(i64 32, i64 1, i64 32, i64 1, i64 0, ptr %dst, i64 128, <32 x float> %t)
  ret void
}

; CUBE conversion stores keep their layout and Null padding.
; CHECK-LABEL: <cube_store>:
; CHECK: BSTART.TSTORE FP32
; CHECK-NEXT: B.DATR M322ND.normal, Null
define void @cube_store(ptr %src, ptr %dst) {
  %t = call <32 x float> @llvm.linx.blk.tload.v32f32(i64 1, i64 32, i64 1, i64 1, i64 0, i64 21, ptr %src, i64 4)
  call void @llvm.linx.blk.tstore.v32f32(i64 1, i64 32, i64 1, i64 1, i64 24, ptr %dst, i64 4, <32 x float> %t)
  ret void
}

declare <32 x float> @llvm.linx.blk.tload.v32f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v32f32(i64, i64, i64, i64, i64, ptr, i64, <32 x float>)
