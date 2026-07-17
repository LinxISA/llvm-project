; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

declare void @llvm.linx.vblock.launch(i32, ptr, i64, i64, i64, i32,
                                      i64, i64, i64, i64, i64, i64,
                                      i64, i64, i64, i64, i64, i64)

define void @vseq() {
entry:
  call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 2, i64 3, i64 4, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

define void @vpar() {
entry:
  call void @llvm.linx.vblock.launch(i32 1, ptr null, i64 5, i64 6, i64 7, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

; CHECK-LABEL: vseq:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}3,{{.*->lb1}}
; CHECK: C.B.DIMI{{[[:space:]]+}}4,{{.*->lb2}}
; CHECK-LABEL: vpar:
; CHECK: BSTART.MPAR
; CHECK: C.B.DIMI{{[[:space:]]+}}5,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}6,{{.*->lb1}}
; CHECK: C.B.DIMI{{[[:space:]]+}}7,{{.*->lb2}}
; CHECK-NOT: BSTART{{.}}PAR
