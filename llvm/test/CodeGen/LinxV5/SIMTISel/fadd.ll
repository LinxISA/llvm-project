; RUN: llc < %s  --march=linx64v5 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=simt
target triple = "linx64v5-unknown-linux-musl"

; simt: name: fadd64
; simt: SIMT_FADD_SCAR 0, [[REG1:%[0-9]+]], 0, [[REG2:%[0-9]+]], 0
define dso_local void @fadd64(double noundef %a, double noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %fadd = fadd double %a, %b
  store double %fadd, ptr %p
  ret void
}

; simt: name: fadd32
; simt: SIMT_FADD_SCAR 1, [[REG1:%[0-9]+]], 1, [[REG2:%[0-9]+]], 1
define dso_local void @fadd32(float noundef %a, float noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %fadd = fadd float %a, %b
  store float %fadd, ptr %p
  ret void
}

; simt: name: fadd16
; simt: SIMT_FADD_SCAR 2, [[REG1:%[0-9]+]], 2, [[REG2:%[0-9]+]], 2
define dso_local void @fadd16(half noundef %a, half noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %fadd = fadd half %a, %b
  store half %fadd, ptr %p
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+m,+relax" }
attributes #1 = { noinline "__vec__" }