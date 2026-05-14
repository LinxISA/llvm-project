
; RUN: llc < %s -enable-all-vector-as-tilereg=true --march=linx64 -O2 | FileCheck %s --dump-input always -vv


@RowStride = external dso_local local_unnamed_addr global i32

; Scalar Load/Store and Scalar MAX/MIN Instruction Selection Verification
; C++ source code
; using tile = long tile_size(256);
; typedef long DType;
; extern int RowStride;
; // rowmajor
; void __vec__ rowmax(tile O, tile A) {
;   auto *pa = blkv_get_tile_ptr(A);
;   int l = blkv_get_index_x(); // 0 ~ LaneNum
;   int j = blkv_get_index_y(); // Rows
;   int Cols = 6;
;   int LaneNum = 3;
;   int ColStride = 8;
;   int groups = (Cols + LaneNum) / LaneNum;
;   DType max = 0;
;   for (int g = 0; g < groups; g++) {
;     int i = g * LaneNum + l;
;     DType local_max = blkv_rdmax(pa[ i * RowStride + j * ColStride]); // 这是reduce
;     max = blkv_max(local_max, max); // 这是标量max
;   }
;   auto *po = blkv_get_tile_ptr(O);
;   pa[2 * ColStride] = max; // 这是个标量的store，目标地址是tile memory
; }

; CHECK: v.rdmax	vt#1.sd, ->t.d
; CHECK: l.max t#1.sd, t#2.sd, ->t.d
; CHECK: l.sdi.local t#1.sd, [tb, 128]
; Function Attrs: mustprogress noinline nounwind
define dso_local void @_Z6rowmaxDv256_lS_(<256 x i64>  %O, <256 x i64>  %A) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256i64(<256 x i64> %A)
  %1 = tail call i16 @llvm.blkv.get.index.x()
  %conv = zext i16 %1 to i32
  %2 = tail call i16 @llvm.blkv.get.index.y()
  %conv1 = zext i16 %2 to i32
  %mul4 = shl nuw nsw i32 %conv1, 3
  %3 = load i32, ptr @RowStride
  %mul3 = mul nsw i32 %3, %conv
  %add5 = add nsw i32 %mul3, %mul4
  %idxprom = sext i32 %add5 to i64
  %arrayidx = getelementptr inbounds i64, ptr addrspace(6) %0, i64 %idxprom
  %4 = load i64, ptr addrspace(6) %arrayidx
  %5 = tail call i64 @llvm.linx.blkv.rdmax.i64(i64 %4)
  %6 = tail call i64 @llvm.linx.blkv.max.i64(i64 %5, i64 0)
  %add2.1 = add nuw nsw i32 %conv, 3
  %7 = load i32, ptr @RowStride
  %mul3.1 = mul nsw i32 %7, %add2.1
  %add5.1 = add nsw i32 %mul3.1, %mul4
  %idxprom.1 = sext i32 %add5.1 to i64
  %arrayidx.1 = getelementptr inbounds i64, ptr addrspace(6) %0, i64 %idxprom.1
  %8 = load i64, ptr addrspace(6) %arrayidx.1
  %9 = tail call i64 @llvm.linx.blkv.rdmax.i64(i64 %8)
  %10 = tail call i64 @llvm.linx.blkv.max.i64(i64 %9, i64 %6)
  %add2.2 = add nuw nsw i32 %conv, 6
  %11 = load i32, ptr @RowStride
  %mul3.2 = mul nsw i32 %11, %add2.2
  %add5.2 = add nsw i32 %mul3.2, %mul4
  %idxprom.2 = sext i32 %add5.2 to i64
  %arrayidx.2 = getelementptr inbounds i64, ptr addrspace(6) %0, i64 %idxprom.2
  %12 = load i64, ptr addrspace(6) %arrayidx.2
  %13 = tail call i64 @llvm.linx.blkv.rdmax.i64(i64 %12)
  %14 = tail call i64 @llvm.linx.blkv.max.i64(i64 %13, i64 %10)
  %arrayidx8 = getelementptr inbounds i64, ptr addrspace(6) %0, i64 16
  store i64 %14, ptr addrspace(6) %arrayidx8
  ret void
}

declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256i64(<256 x i64>)

declare i16 @llvm.blkv.get.index.x()

declare i16 @llvm.blkv.get.index.y()

declare i64 @llvm.linx.blkv.rdmax.i64(i64)

declare i64 @llvm.linx.blkv.max.i64(i64, i64)

attributes #0 = { mustprogress noinline nounwind "__vec__" "frame-pointer"="none" "min-legal-vector-width"="16384" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }