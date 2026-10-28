; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true %s -o /dev/null 2>&1 | FileCheck %s
; CHECK: predicate MSCATTER_ADD requires Local CUBE M32

define void @reject_m16(ptr %base) {
  call void @llvm.linx.experimental.ew.mscatter.add.predicate.v64i32.v128i32.v32i32(i64 16, i64 4, i64 4, i64 25, i64 31, ptr %base, <64 x i32> zeroinitializer, <128 x i32> zeroinitializer, <32 x i32> zeroinitializer, i64 0)
  ret void
}

declare void @llvm.linx.experimental.ew.mscatter.add.predicate.v64i32.v128i32.v32i32(i64, i64, i64, i64, i64, ptr, <64 x i32>, <128 x i32>, <32 x i32>, i64)
