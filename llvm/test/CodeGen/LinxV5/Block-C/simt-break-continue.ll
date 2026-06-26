; Test SIMT control flow with break and continue in loops.
;
; Test 1: simt_break — loop with break (exit loop early for certain lanes)
; Test 2: simt_continue — loop with continue (skip rest of current iteration)
;
; Break semantics:
;   - blkv_if_break(exit_cond, broken) accumulates the break mask across iterations
;   - blkv_loop(broken) computes AND(loop_active_mask, ~broken) as the new active mask
;   - If all active lanes have broken (mask == 0), the loop exits
;   - Mask behavior: CopyP reads old mask, MERGE_PREDICATION predicates exit_cond,
;     CopyP reads exit mask, Copy2P restores old mask, OR accumulates broken mask,
;     XOR inverts broken, AND computes remaining active lanes, Copy2PTerm sets loop mask
;
; Continue semantics:
;   - No dedicated continue intrinsic — it's an inner divergent if inside the loop
;   - blkv_if(condition) deactivates "then" lanes (those hitting continue)
;   - blkv_end_cf(saved_mask) restores mask, all lanes re-converge
;   - blkv_merge_cf(then_val, else_val) merges divergent PHI values
;   - Mask behavior: CopyFromP saves mask, Copy2PTerm restores at convergence,
;     PSEL merges divergent values at convergence point

; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true -stop-after=linx-annotate-control-flow -o - 2>&1 | FileCheck %s --check-prefix=ANN
; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true -stop-after=virtregrewriter -o - 2>/dev/null | FileCheck %s --check-prefix=VRR
; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true -o - 2>/dev/null | FileCheck %s --check-prefix=ASM

target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

declare i16 @llvm.blkv.get.index.x()

; === Test 1: break ===
; void __mtc__ simt_break(int *in, int *out, int count) {
;   int tid = blkv_get_index_x();
;   int sum = 0;
;   for (int i = 0; i < count; i++) {
;     int v = in[tid + i];
;     if (v == 42) { break; }
;     sum += v;
;   }
;   out[tid] = sum;
; }

; ANN-LABEL: @simt_break
; ANN: %phi.broken = phi i64
; ANN: call { i1, i64 } @llvm.blkv.if.i64
; ANN: call i32 @llvm.blkv.merge.cf.i32
; ANN: call i1 @llvm.blkv.merge.cf.i1
; ANN: call void @llvm.blkv.end.cf.i64
; ANN: call i64 @llvm.blkv.if.break.i64
; ANN: call i1 @llvm.blkv.loop.i64

; VRR-LABEL: name: simt_break
; VRR: LinxV5PseudoCopyFromP implicit $simt_p
; VRR: SIMT_CMP_NEI_P
; VRR: LinxV5PseudoCopyFromP implicit $simt_p
; VRR: LinxV5PseudoCopy2P
; VRR: SIMT_OR_SCAR
; VRR: SIMT_XORI_SCAR {{.*}} -1
; VRR: LinxV5PseudoCopyFromP implicit $simt_p
; VRR: SIMT_AND_SCAR
; VRR: LinxV5PseudoCopy2PTerm

define dso_local void @simt_break(ptr noundef %in, ptr nocapture noundef writeonly %out, i32 noundef signext %count) local_unnamed_addr #0 {
entry:
  %0 = tail call i16 @llvm.blkv.get.index.x()
  %cmp8 = icmp sgt i32 %count, 0
  br i1 %cmp8, label %for.body.lr.ph, label %cleanup

for.body.lr.ph:
  %1 = zext i16 %0 to i64
  %wide.trip.count = zext i32 %count to i64
  br label %for.body

for.body:
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %for.inc ]
  %sum.010 = phi i32 [ 0, %for.body.lr.ph ], [ %add, %for.inc ]
  %2 = add nuw nsw i64 %indvars.iv, %1
  %arrayidx = getelementptr inbounds i32, ptr %in, i64 %2
  %3 = load volatile i32, ptr %arrayidx, align 4
  %cmp1 = icmp eq i32 %3, 42
  br i1 %cmp1, label %cleanup, label %for.inc

for.inc:
  %add = add nsw i32 %3, %sum.010
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %cleanup, label %for.body, !llvm.loop !10

cleanup:
  %sum.1 = phi i32 [ 0, %entry ], [ %3, %for.body ], [ %add, %for.inc ]
  %idxprom = zext i16 %0 to i64
  %arrayidx3 = getelementptr inbounds i32, ptr %out, i64 %idxprom
  store volatile i32 %sum.1, ptr %arrayidx3, align 4
  ret void
}

; === Test 2: continue ===
; void __mtc__ simt_continue(int *in, int *out, int count) {
;   int tid = blkv_get_index_x();
;   int sum = 0;
;   for (int i = 0; i < count; i++) {
;     int v = in[tid + i];
;     if (v == 42) { continue; }
;     sum += v;
;   }
;   out[tid] = sum;
; }

; ANN-LABEL: @simt_continue
; ANN: call { i1, i64 } @llvm.blkv.if.i64
; ANN: call i32 @llvm.blkv.merge.cf.i32
; ANN: call void @llvm.blkv.end.cf.i64

; VRR-LABEL: name: simt_continue
; VRR: LinxV5PseudoCopyFromP implicit $simt_p
; VRR: SIMT_CMP_NEI_P {{.*}} implicit-def $simt_p, implicit $simt_p
; VRR: LinxV5PseudoCopyFromP implicit $simt_p
; VRR: LinxV5PseudoCopyFromP implicit $simt_p
; VRR: LinxV5PseudoCopy2PTerm
; VRR: SIMT_PSEL

define dso_local void @simt_continue(ptr noundef %in, ptr nocapture noundef writeonly %out, i32 noundef signext %count) local_unnamed_addr #0 {
entry:
  %0 = tail call i16 @llvm.blkv.get.index.x()
  %cmp10 = icmp sgt i32 %count, 0
  br i1 %cmp10, label %for.body.lr.ph, label %for.end

for.body.lr.ph:
  %1 = zext i16 %0 to i64
  %wide.trip.count = zext i32 %count to i64
  br label %for.body

for.body:
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %if.end ]
  %sum.012 = phi i32 [ 0, %for.body.lr.ph ], [ %sum.1, %if.end ]
  %2 = add nuw nsw i64 %indvars.iv, %1
  %arrayidx = getelementptr inbounds i32, ptr %in, i64 %2
  %3 = load volatile i32, ptr %arrayidx, align 4
  %cmp1 = icmp eq i32 %3, 42
  br i1 %cmp1, label %if.then, label %if.else

if.then:                                            ; continue path - skip sum += v
  br label %if.end

if.else:                                            ; normal path - do sum += v
  %add = add nsw i32 %3, %sum.012
  br label %if.end

if.end:                                             ; converge point
  %sum.1 = phi i32 [ %sum.012, %if.then ], [ %add, %if.else ]
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %for.end, label %for.body, !llvm.loop !10

for.end:
  %sum.2 = phi i32 [ 0, %entry ], [ %sum.1, %if.end ]
  %idxprom = zext i16 %0 to i64
  %arrayidx5 = getelementptr inbounds i32, ptr %out, i64 %idxprom
  store volatile i32 %sum.2, ptr %arrayidx5, align 4
  ret void
}

; ASM checks verify the full assembly emission completes without the
; VecReg→ScalarReg copy crash that previously occurred when blkv_merge_cf.i1's
; i8 PSEL result was treated as uniform.

; ASM-LABEL: simt_break:
; ASM: v.psel
; ASM: v.andi {{.*}} 1
; ASM: v.cmp.nei {{.*}} ->p
; ASM: L.BSTOP

; ASM-LABEL: simt_continue:
; ASM: v.cmp.nei {{.*}} ->p
; ASM: v.psel
; ASM: L.BSTOP

attributes #0 = { argmemonly mustprogress nofree noinline nounwind "__mtc__" "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }


!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4"}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.mustprogress"}
