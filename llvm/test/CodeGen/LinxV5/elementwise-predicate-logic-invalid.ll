; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/arithmetic.ll 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/capacity.ll 2>&1 | FileCheck %s
; CHECK: element binary requires an exact U32/S32 CUBE_M32 or FP32 NORM profile

;--- arithmetic.ll
define void @predicate_add(ptr %left, ptr %right, ptr %out) {
  %first = load <32 x i32>, ptr %left
  %second = load <32 x i32>, ptr %right
  %result = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 27, i64 29, i64 0,
      <32 x i32> %first, <32 x i32> %second)
  store <32 x i32> %result, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
    i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>)

;--- capacity.ll
define void @predicate_wrong_capacity(ptr %left, ptr %right, ptr %out) {
  %first = load <64 x i32>, ptr %left
  %second = load <64 x i32>, ptr %right
  %result = call <64 x i32> @llvm.linx.experimental.ew.tbinary.v64i32(
      i64 32, i64 2, i64 27, i64 29, i64 5,
      <64 x i32> %first, <64 x i32> %second)
  store <64 x i32> %result, ptr %out
  ret void
}
declare <64 x i32> @llvm.linx.experimental.ew.tbinary.v64i32(
    i64, i64, i64, i64, i64, <64 x i32>, <64 x i32>)
