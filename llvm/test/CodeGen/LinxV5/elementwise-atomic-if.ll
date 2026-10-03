; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s

; CHECK-LABEL: <atomic_if_u32>:
; CHECK: BSTART.TEPL TCI, U32
; CHECK: BSTART.TEPL TCMPS, U32
; CHECK: BSTART.TEPL TCMPS, U32
; CHECK: and
; CHECK: BSTART.TEPL TLEA, U32
; CHECK: BSTART.TLSU MGATHER.ADD, U32
; CHECK: B.DATR CUBE_M32
; CHECK: B.IOR
; CHECK: ExecMaskPresent

define void @atomic_if_u32(ptr %out, ptr %hist, ptr %low.ptr, ptr %high.ptr,
                           i64 %valid, i64 %selected) {
  %low = load <32 x i32>, ptr %low.ptr
  %high = load <32 x i32>, ptr %high.ptr
  %lanes = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 0, i64 4294967296)
  %tail.mask = call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32(
      i64 32, i64 1, i64 25, i64 29, <32 x i32> %lanes, i64 %valid, i64 2)
  %key.mask = call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32(
      i64 32, i64 1, i64 25, i64 29, <32 x i32> %high, i64 %selected, i64 0)
  %mask = and i64 %tail.mask, %key.mask
  %offsets = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
      i64 32, i64 1, i64 25, i64 29, <32 x i32> %low, i64 32)
  %ones = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 1, i64 0)
  %old = call <32 x i32>
      @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(
          i64 32, i64 1, i64 25, i64 3, i64 29, ptr %hist,
          <32 x i64> %offsets, <32 x i32> %ones,
          i64 %mask, i64 0, i64 0, i64 1)
  store <32 x i32> %old, ptr %out
  ret void
}

declare <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
    i64, i64, i64, i64, i64, i64)
declare i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32(
    i64, i64, i64, i64, <32 x i32>, i64, i64)
declare <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
    i64, i64, i64, i64, <32 x i32>, i64)
declare <32 x i32>
    @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(
        i64, i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>,
        i64, i64, i64, i64)
