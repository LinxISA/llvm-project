; RUN: llc < %s  --march=linx64 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=simt
target triple = "linx64-unknown-linux-musl"

; simt: name: mul64
; simt: SIMT_MUL_SCAR 0, [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 4
define dso_local void @mul64(i64 noundef %a, i64 noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %mul = mul i64 %a, %b
  store i64 %mul, ptr %p
  ret void
}

; simt: name: mul32
; simt: SIMT_MUL_SCAR 1, [[REG1:%[0-9]+]], 5, [[REG2:%[0-9]+]], 5
define dso_local void @mul32(i32 noundef %a, i32 noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %mul = mul i32 %a, %b
  store i32 %mul, ptr %p
  ret void
}

; simt: name: mul16
; simt: SIMT_MUL_SCAR 2, [[REG1:%[0-9]+]], 6, [[REG2:%[0-9]+]], 6
define dso_local void @mul16(i16 noundef %a, i16 noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %mul = mul i16 %a, %b
  store i16 %mul, ptr %p
  ret void
}

; simt: name: mul8
; simt: SIMT_MUL_SCAR 3, [[REG1:%[0-9]+]], 7, [[REG2:%[0-9]+]], 7
define dso_local void @mul8(i8 noundef %a, i8 noundef %b, ptr %p) local_unnamed_addr #0 #1 {
entry:
  %mul = mul i8 %a, %b
  store i8 %mul, ptr %p
  ret void
}

; simt: name: vmul
; simt: SIMT_MUL 0, [[REG1:%[0-9]+]], 2, [[REG2:%[0-9]+]], 2
define dso_local void @vmul(ptr %p) local_unnamed_addr #0 #1 {
entry:
  %0 = tail call i16 @llvm.blkv.get.index.x()
  %1 = tail call i16 @llvm.blkv.get.index.y()
  %conv = zext i16 %0 to i64
  %conv1 = zext i16 %1 to i64
  %mul = mul nuw nsw i64 %conv1, %conv
  store i64 %mul, ptr %p
  ret void
}

declare i16 @llvm.blkv.get.index.x() #0 #1
declare i16 @llvm.blkv.get.index.y() #0 #1

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+m,+relax" }
attributes #1 = { noinline "__vec__" }