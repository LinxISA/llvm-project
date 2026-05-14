; RUN: llc < %s -enable-all-vector-as-tilereg=true --march=linx64 -O2 | FileCheck %s --dump-input always -vv

; CHECK: .LBB0_0:
; CHECK: v.lw.local    [ta, lc0<<2, zero.sd],  ->vn.w
; CHECK: .LBB0_1:
; CHECK: v.lw.local    [tb, lc0<<2, zero.sd],  ->vn.w
; CHECK: .LBB0_3:
; CHECK:  v.lwi.local   [ta, lc0<<2, -4],       ->vt.w
define dso_local void @_Z11test_branchu11matrix_typeILm16ELm16EfES_S_i(<256 x float> noundef %TA, <256 x float> noundef %TB, <256 x float> __out__ noundef %TC, i32 noundef signext %num) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TA)
  %1 = tail call i16 @llvm.blkv.get.index.x()
  %idxprom = zext i16 %1 to i64
  %arrayidx = getelementptr inbounds float, ptr addrspace(6) %0, i64 %idxprom
  %2 = load float, ptr addrspace(6) %arrayidx
  %conv1 = fptosi float %2 to i32
  %3 = and i32 %conv1, 1
  %cmp = icmp eq i32 %3, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %sub = add nsw i64 %idxprom, -1
  %arrayidx5 = getelementptr inbounds float, ptr addrspace(6) %0, i64 %sub
  %4 = load float, ptr addrspace(6) %arrayidx5
  %mul = fmul float %2, %4
  br label %if.end

if.else:                                          ; preds = %entry
  %5 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TB)
  %arrayidx9 = getelementptr inbounds float, ptr addrspace(6) %5, i64 %idxprom
  %6 = load float, ptr addrspace(6) %arrayidx9
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %.sink = phi float [ %mul, %if.then ], [ %6, %if.else ]
  %7 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TC)
  %8 = getelementptr inbounds float, ptr addrspace(6) %7, i64 %idxprom
  store float %.sink, ptr addrspace(6) %8
  ret void
}

declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float>)

declare i16 @llvm.blkv.get.index.x()

attributes #0 = { mustprogress nofree noinline nosync nounwind willreturn "__vec__" "frame-pointer"="none" "min-legal-vector-width"="8192" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }