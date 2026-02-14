; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

declare <1024 x i32> @llvm.linx.tma.tload.desc(ptr, i32, i32, i32, i32, i32)
declare void @llvm.linx.tma.tstore.desc(ptr, <1024 x i32>, i32, i32, i32, i32, i32)

define void @tma_desc_roundtrip(ptr %src, ptr %dst) {
entry:
  %t = call <1024 x i32> @llvm.linx.tma.tload.desc(ptr %src, i32 0, i32 8, i32 8, i32 8, i32 8)
  call void @llvm.linx.tma.tstore.desc(ptr %dst, <1024 x i32> %t, i32 0, i32 8, i32 8, i32 8, i32 8)
  ret void
}

; CHECK-LABEL: tma_desc_roundtrip:
; CHECK: BSTART.TMA{{[[:space:]]+}}TLOAD,
; CHECK: C.B.DIMI{{.*}}->lb0
; CHECK: C.B.DIMI{{.*}}->lb1
; CHECK: C.B.DIMI{{.*}}->lb2
; CHECK: B.ARG
; CHECK: B.IOR
; CHECK: B.IOTI
; CHECK: BSTART.TMA{{[[:space:]]+}}TSTORE,
; CHECK: C.B.DIMI{{.*}}->lb2
; CHECK: B.ARG
; CHECK: B.IOR
; CHECK: B.IOTI
