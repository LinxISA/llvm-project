; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s

define void @gpr_predication(ptr %out, ptr %base, ptr %lhs.ptr, ptr %rhs.ptr) {
  %lhs = load <32 x i32>, ptr %lhs.ptr
  %rhs = load <32 x i32>, ptr %rhs.ptr
  %cmp = call i64 @llvm.linx.experimental.ew.tcmp.gpr.v32i32(
      i64 32, i64 1, i64 25, i64 29, <32 x i32> %lhs, <32 x i32> %rhs, i64 2)
  %sum = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 0, <32 x i32> %lhs,
      <32 x i32> %rhs, i64 %cmp, i64 0, i64 0, i64 1)
  %selected = call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 %cmp, i64 0,
      <32 x i32> %sum, <32 x i32> %rhs)
  %offsets = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
      i64 32, i64 1, i64 25, i64 29, <32 x i32> %lhs, i64 32)
  call void @llvm.linx.experimental.ew.mscatter.gpr.masked.v32i64.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 24, ptr %base,
      <32 x i64> %offsets, <32 x i32> %selected, i64 %cmp, i64 0,
      i64 0, i64 0)
  store <32 x i32> %selected, ptr %out
  ret void
}

; CHECK-LABEL: <gpr_predication>:
; CHECK: BSTART.TEPL TCMP, U32
; CHECK: B.DATR NORM.normal, Zero
; CHECK: B.IOR [], ->{{[a-z0-9]+}}
; CHECK: BSTART.TEPL TADD, U32
; CHECK: ExecMaskPresent
; CHECK: BSTART.TEPL TSEL, U32
; CHECK: B.IOR [{{[a-z0-9]+}}], []
; CHECK-NOT: ExecMaskPresent
; CHECK: BSTART.TLSU MSCATTER, U32
; CHECK-NOT: MSCATTER.MASK
; CHECK: ExecMaskPresent

declare i64 @llvm.linx.experimental.ew.tcmp.gpr.v32i32(
    i64 immarg, i64 immarg, i64 immarg, i64 immarg, <32 x i32>,
    <32 x i32>, i64 immarg)
declare <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(
    i64 immarg, i64 immarg, i64 immarg, i64 immarg, i64 immarg,
    <32 x i32>, <32 x i32>, i64, i64, i64 immarg, i64 immarg)
declare <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32(
    i64 immarg, i64 immarg, i64 immarg, i64 immarg, i64, i64,
    <32 x i32>, <32 x i32>)
declare <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
    i64 immarg, i64 immarg, i64 immarg, i64 immarg, <32 x i32>, i64 immarg)
declare void @llvm.linx.experimental.ew.mscatter.gpr.masked.v32i64.v32i32(
    i64 immarg, i64 immarg, i64 immarg, i64 immarg, i64 immarg, ptr,
    <32 x i64>, <32 x i32>, i64, i64, i64 immarg, i64 immarg)
