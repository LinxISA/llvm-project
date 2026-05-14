; RUN: llc < %s --march=linx64v5 -stop-after=finalize-isel -enable-all-vector-as-tilereg=true -O2 2>&1 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

target triple = "linx64v5-unknown-linux-musl"

; CHECK: test
; CHECK: simtcgs = COPY $simt_tc
; CHECK: simtcgs = COPY $simt_tb
; CHECK: simtcgs = COPY $simt_to
; CHECK: simtcgs = COPY $simt_ta
; CHECK: simtcgv = COPY $simt_lc0
; CHECK: simtcgs = COPY $simt_lc1
; CHECK: simtcgs = COPY $simt_lc2
; Function Attrs: mustprogress nofree nosync nounwind willreturn
define dso_local void @test(<64 x double> noundef %in1, <64 x double> __out__ noundef %out, <64 x double> noundef %in2, <64 x double> noundef %in3, ptr nocapture noundef writeonly %p) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr @llvm.blkv.get.tile.ptr.v64f64(<64 x double> %in1)
  %1 = tail call ptr @llvm.blkv.get.tile.ptr.v64f64(<64 x double> %in2)
  %2 = tail call ptr @llvm.blkv.get.tile.ptr.v64f64(<64 x double> %in3)
  %3 = tail call ptr @llvm.blkv.get.tile.ptr.v64f64(<64 x double> %out)
  %4 = tail call i16 @llvm.blkv.get.index.x()
  %5 = tail call i16 @llvm.blkv.get.index.y()
  %6 = tail call i16 @llvm.blkv.get.index.z()
  %idxprom = sext i16 %4 to i64
  %arrayidx = getelementptr inbounds double, ptr %3, i64 %idxprom
  %7 = load double, ptr %arrayidx
  %arrayidx2 = getelementptr inbounds double, ptr %0, i64 %idxprom
  %8 = load double, ptr %arrayidx2
  %add = fadd double %7, %8
  %idxprom3 = sext i16 %5 to i64
  %arrayidx4 = getelementptr inbounds double, ptr %1, i64 %idxprom3
  %9 = load double, ptr %arrayidx4
  %add5 = fadd double %add, %9
  %idxprom6 = sext i16 %6 to i64
  %arrayidx7 = getelementptr inbounds double, ptr %2, i64 %idxprom6
  %10 = load double, ptr %arrayidx7
  %add8 = fadd double %add5, %10
  %arrayidx10 = getelementptr inbounds double, ptr %p, i64 %idxprom
  store double %add8, ptr %arrayidx10
  ret void
}

; Function Attrs: nofree nosync nounwind readnone
declare ptr @llvm.blkv.get.tile.ptr.v64f64(<64 x double>) #1

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.x() #1

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.y() #1

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.z() #1

attributes #0 = { "__vec__" }
