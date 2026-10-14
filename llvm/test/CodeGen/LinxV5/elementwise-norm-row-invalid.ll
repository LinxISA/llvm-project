; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true %t/cmp.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=CMP
; RUN: not --crash llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true %t/select.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=SELECT
; CMP: NORM TCMP requires matching numeric sources and packed predicate capacity
; SELECT: NORM TSEL requires a packed predicate and matching numeric carriers

;--- cmp.ll
; Packed words may not be reinterpreted as the comparison's numeric row.
declare <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v32i32.v32i32(i64, i64, i64, i64, <32 x i32>, <32 x i32>, i64, i64)
define void @invalid_cmp() {
  %p = call <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v32i32.v32i32(i64 1, i64 128, i64 25, i64 0, <32 x i32> zeroinitializer, <32 x i32> zeroinitializer, i64 0, i64 128)
  ret void
}

;--- select.ll
; Numeric row storage is not the packed Predicate storage carrier.
declare <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v128i32.v128i32.v128i32(i64, i64, i64, i64, <128 x i32>, <128 x i32>, <128 x i32>, i64)
define void @invalid_select() {
  %r = call <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v128i32.v128i32.v128i32(i64 1, i64 128, i64 25, i64 0, <128 x i32> zeroinitializer, <128 x i32> zeroinitializer, <128 x i32> zeroinitializer, i64 128)
  ret void
}
