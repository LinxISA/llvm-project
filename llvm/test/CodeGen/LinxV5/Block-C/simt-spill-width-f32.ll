; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true | FileCheck %s --check-prefix=ASM

target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; 这个用例验证 f32 SIMT 向量值在产生大量 spill 时，
; 最终应当使用 W 宽 spill/reload，而不是错误地退化成 D 宽。
;
; ASM-LABEL: forced_then_spill_width_f32:
; ASM: v.swi.u.local
; ASM: v.lwi.u.local
; ASM-NOT: v.sdi.u.local
; ASM-NOT: v.ldi.u.local

define dso_local void @forced_then_spill_width_f32(<256 x float> %TA,
                                                   <256 x float> %TB,
                                                   <256 x float> %TC) local_unnamed_addr #0 {
entry:
  %a = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TA)
  %b = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TB)
  %x = tail call i16 @llvm.blkv.get.index.x()
  %idx = zext i16 %x to i64
  %pa = getelementptr inbounds float, ptr addrspace(6) %a, i64 %idx
  %ld0 = load volatile float, ptr addrspace(6) %pa, align 4, !tbaa !6
  %condi = fptosi float %ld0 to i32
  %bit = and i32 %condi, 1
  %cond = icmp eq i32 %bit, 0
  br i1 %cond, label %if.then, label %if.else

if.then:
  %ti0 = add i64 %idx, 0
  %tp0 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti0
  %tv0 = load volatile float, ptr addrspace(6) %tp0, align 4, !tbaa !6
  %tr0 = fadd float %tv0, 1.000000e+00
  %ti1 = add i64 %idx, 1
  %tp1 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti1
  %tv1 = load volatile float, ptr addrspace(6) %tp1, align 4, !tbaa !6
  %tr1 = fadd float %tv1, 2.000000e+00
  %ti2 = add i64 %idx, 2
  %tp2 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti2
  %tv2 = load volatile float, ptr addrspace(6) %tp2, align 4, !tbaa !6
  %tr2 = fadd float %tv2, 3.000000e+00
  %ti3 = add i64 %idx, 3
  %tp3 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti3
  %tv3 = load volatile float, ptr addrspace(6) %tp3, align 4, !tbaa !6
  %tr3 = fadd float %tv3, 4.000000e+00
  %ti4 = add i64 %idx, 4
  %tp4 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti4
  %tv4 = load volatile float, ptr addrspace(6) %tp4, align 4, !tbaa !6
  %tr4 = fadd float %tv4, 5.000000e+00
  %ti5 = add i64 %idx, 5
  %tp5 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti5
  %tv5 = load volatile float, ptr addrspace(6) %tp5, align 4, !tbaa !6
  %tr5 = fadd float %tv5, 6.000000e+00
  %ti6 = add i64 %idx, 6
  %tp6 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti6
  %tv6 = load volatile float, ptr addrspace(6) %tp6, align 4, !tbaa !6
  %tr6 = fadd float %tv6, 7.000000e+00
  %ti7 = add i64 %idx, 7
  %tp7 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti7
  %tv7 = load volatile float, ptr addrspace(6) %tp7, align 4, !tbaa !6
  %tr7 = fadd float %tv7, 8.000000e+00
  %ti8 = add i64 %idx, 8
  %tp8 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti8
  %tv8 = load volatile float, ptr addrspace(6) %tp8, align 4, !tbaa !6
  %tr8 = fadd float %tv8, 9.000000e+00
  %ti9 = add i64 %idx, 9
  %tp9 = getelementptr inbounds float, ptr addrspace(6) %a, i64 %ti9
  %tv9 = load volatile float, ptr addrspace(6) %tp9, align 4, !tbaa !6
  %tr9 = fadd float %tv9, 1.000000e+01
  br label %if.end

if.else:
  %ei0 = add i64 %idx, 0
  %ep0 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei0
  %ev0 = load volatile float, ptr addrspace(6) %ep0, align 4, !tbaa !6
  %er0 = fsub float %ev0, 1.000000e+00
  %ei1 = add i64 %idx, 1
  %ep1 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei1
  %ev1 = load volatile float, ptr addrspace(6) %ep1, align 4, !tbaa !6
  %er1 = fsub float %ev1, 2.000000e+00
  %ei2 = add i64 %idx, 2
  %ep2 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei2
  %ev2 = load volatile float, ptr addrspace(6) %ep2, align 4, !tbaa !6
  %er2 = fsub float %ev2, 3.000000e+00
  %ei3 = add i64 %idx, 3
  %ep3 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei3
  %ev3 = load volatile float, ptr addrspace(6) %ep3, align 4, !tbaa !6
  %er3 = fsub float %ev3, 4.000000e+00
  %ei4 = add i64 %idx, 4
  %ep4 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei4
  %ev4 = load volatile float, ptr addrspace(6) %ep4, align 4, !tbaa !6
  %er4 = fsub float %ev4, 5.000000e+00
  %ei5 = add i64 %idx, 5
  %ep5 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei5
  %ev5 = load volatile float, ptr addrspace(6) %ep5, align 4, !tbaa !6
  %er5 = fsub float %ev5, 6.000000e+00
  %ei6 = add i64 %idx, 6
  %ep6 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei6
  %ev6 = load volatile float, ptr addrspace(6) %ep6, align 4, !tbaa !6
  %er6 = fsub float %ev6, 7.000000e+00
  %ei7 = add i64 %idx, 7
  %ep7 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei7
  %ev7 = load volatile float, ptr addrspace(6) %ep7, align 4, !tbaa !6
  %er7 = fsub float %ev7, 8.000000e+00
  %ei8 = add i64 %idx, 8
  %ep8 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei8
  %ev8 = load volatile float, ptr addrspace(6) %ep8, align 4, !tbaa !6
  %er8 = fsub float %ev8, 9.000000e+00
  %ei9 = add i64 %idx, 9
  %ep9 = getelementptr inbounds float, ptr addrspace(6) %b, i64 %ei9
  %ev9 = load volatile float, ptr addrspace(6) %ep9, align 4, !tbaa !6
  %er9 = fsub float %ev9, 1.000000e+01
  br label %if.end

if.end:
  %m0 = phi float [ %tr0, %if.then ], [ %er0, %if.else ]
  %m1 = phi float [ %tr1, %if.then ], [ %er1, %if.else ]
  %m2 = phi float [ %tr2, %if.then ], [ %er2, %if.else ]
  %m3 = phi float [ %tr3, %if.then ], [ %er3, %if.else ]
  %m4 = phi float [ %tr4, %if.then ], [ %er4, %if.else ]
  %m5 = phi float [ %tr5, %if.then ], [ %er5, %if.else ]
  %m6 = phi float [ %tr6, %if.then ], [ %er6, %if.else ]
  %m7 = phi float [ %tr7, %if.then ], [ %er7, %if.else ]
  %m8 = phi float [ %tr8, %if.then ], [ %er8, %if.else ]
  %m9 = phi float [ %tr9, %if.then ], [ %er9, %if.else ]
  %sum1 = fadd float %m0, %m1
  %sum2 = fadd float %sum1, %m2
  %sum3 = fadd float %sum2, %m3
  %sum4 = fadd float %sum3, %m4
  %sum5 = fadd float %sum4, %m5
  %sum6 = fadd float %sum5, %m6
  %sum7 = fadd float %sum6, %m7
  %sum8 = fadd float %sum7, %m8
  %sum9 = fadd float %sum8, %m9
  %c = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TC)
  %pc = getelementptr inbounds float, ptr addrspace(6) %c, i64 %idx
  store volatile float %sum9, ptr addrspace(6) %pc, align 4, !tbaa !6
  ret void
}

declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float>) #1
declare i16 @llvm.blkv.get.index.x() #1

attributes #0 = { mustprogress nofree noinline nosync nounwind willreturn "__vec__" "frame-pointer"="none" "min-legal-vector-width"="8192" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nofree nosync nounwind readnone }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}
!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4"}
!6 = !{!7, !7, i64 0}
!7 = !{!"float", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
