; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=linx64 -O2 -filetype=obj < %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=ENC

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)

define void @tlsu_desc_roundtrip(ptr %src, ptr %dst) {
entry:
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: tlsu_desc_roundtrip:
; CHECK: BSTART.TLOAD{{[[:space:]]+}}
; CHECK: C.B.DIMI{{.*}}->lb0
; CHECK: C.B.DIMI{{.*}}->lb1
; CHECK: B.IOR
; CHECK: B.IOT
; CHECK: BSTART.TSTORE{{[[:space:]]+}}
; CHECK: B.IOR
; CHECK: B.IOT
; CHECK-NOT: B.ARG

; The ordinary TSTORE lowering must select the TLSU function-1 base form
; (match 0x00111181), not the distinct TSTORE.SPART encoding variant
; (match 0x00e11181), even though both forms share the same assembly format.
; ENC: 81 11 01 00  BSTART.TLOAD{{[[:space:]]+}}FP64
; ENC: 81 11 11 00  BSTART.TSTORE{{[[:space:]]+}}FP64
