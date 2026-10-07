; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s

define void @gather(ptr %out, ptr %base, ptr %index.ptr, i64 %mask) {
  %indices = load <32 x i32>, ptr %index.ptr
  %offsets = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
      i64 32, i64 1, i64 25, i64 29, <32 x i32> %indices, i64 32)
  %loaded = call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
      i64 32, i64 1, i64 25, i64 0, i64 29, i64 24, ptr %base,
      <32 x i64> %offsets, i64 %mask, i64 0, i64 0, i64 1)
  store <32 x i32> %loaded, ptr %out
  ret void
}

; CHECK-LABEL: <gather>:
; CHECK: BSTART.TEPL TLEA, U32
; CHECK: BSTART.TLSU MGATHER, U32
; CHECK-NOT: MGATHER.ADD
; CHECK: B.DATR CUBE_M32
; CHECK: B.IOT {{.*}} ->t<128B>
; CHECK: B.IOR
; CHECK: ExecMaskPresent

declare <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
    i64 immarg, i64 immarg, i64 immarg, i64 immarg, <32 x i32>, i64 immarg)
declare <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
    i64 immarg, i64 immarg, i64 immarg, i64 immarg, i64 immarg, i64 immarg,
    ptr, <32 x i64>, i64, i64, i64 immarg, i64 immarg)
