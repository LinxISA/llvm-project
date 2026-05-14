; RUN: llc < %s --march=linx64 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=simt

target triple = "linx64-unknown-linux-musl"

; simt: name: feq64
; simt: SIMT_FEQ 0, [[REG1:%[0-9]+]], 0, [[REG2:%[0-9]+]], 0
define dso_local void @feq64(double noundef %a, double noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %cmp = fcmp oeq double %a, %b
  store i1 %cmp, ptr %p
  ret void
}

; simt: name: feq32
; simt: SIMT_FEQ 0, [[REG1:%[0-9]+]], 1, [[REG2:%[0-9]+]], 1
define dso_local void @feq32(float noundef %a, float noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %cmp = fcmp oeq float %a, %b
  store i1 %cmp, ptr %p
  ret void
}

; simt: name: feq16
; simt: SIMT_FEQ 0, [[REG1:%[0-9]+]], 2, [[REG2:%[0-9]+]], 2
define dso_local void @feq16(half noundef %a, half noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %cmp = fcmp oeq half %a, %b
  store i1 %cmp, ptr %p
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+m,+relax" }
attributes #1 = { noinline "__vec__" }