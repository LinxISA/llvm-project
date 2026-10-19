; RUN: split-file %s %t
; RUN: llc < %t/base.ll -enable-all-vector-as-tilereg=true -march=linx64v5be -O3 | FileCheck %s
; RUN: llc < %t/base.ll -enable-all-vector-as-tilereg=true -march=linx64v5be -O3 -linxv5-enable-reg-to-offset=false | FileCheck %s --check-prefix=ABS
; RUN: llc < %t/base.ll -enable-all-vector-as-tilereg=true -march=linx64v5be -O3 -linxv5-enable-generic-element=true | FileCheck %s --check-prefix=GENERIC
; RUN: llc < %t/loop.ll -enable-all-vector-as-tilereg=true -march=linx64v5be -O3 -linxv5-enable-reg-to-offset=false | FileCheck %s --check-prefix=LOOP


; CHECK:VPAR copyin, <M: 4, N: 4, K: 1, MR> [a0], ->t<128B>
; CHECK:VPAR copyin, <M: 4, N: 4, K: 1, MR> [a1], ->t<128B>
; CHECK:VPAR tadd,   <M: 4, N: 4, K: 1, MR> t#2.reuse, t#1, ->t<128B>
; CHECK:VPAR tadd,   <M: 4, N: 4, K: 1, MR> t#3, t#1, ->t<128B>
; CHECK:VPAR copyout, <M: 4, N: 4, K: 1, MR> t#1, [a2]
; ABS-LABEL: tile_caller:
; ABS: VPAR tadd,{{.*}}tile_{{[tumn]}}{{[0-9]+}}.reuse, tile_{{[tumn]}}{{[0-9]+}},
; ABS: VPAR tadd,{{.*}}tile_{{[tumn]}}{{[0-9]+}}, tile_{{[tumn]}}{{[0-9]+}},
; ABS: VPAR copyout,{{.*}}tile_{{[tumn]}}{{[0-9]+}},
; GENERIC-LABEL: tile_caller:
; GENERIC-COUNT-2: VPAR tadd,{{.*}}.reuse, {{[^,]+}}.reuse,
; GENERIC: VPAR copyout,{{.*}}.reuse,

;--- base.ll
; Function Attrs: nounwind
define dso_local void @tile_caller(ptr noundef %p1, ptr noundef %p2, ptr noundef %p3) local_unnamed_addr #0 {
entry:
  %0 = tail call <16 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v16f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p1)
  %1 = tail call <16 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v16f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p2)
  %2 = tail call <16 x double> (ptr, i64, i64, i64, <16 x double>, <16 x double>, ...) @llvm.linx.vcall.par.1d2u.v16f64(ptr nonnull @tadd, i64 4, i64 4, i64 1, <16 x double> %0, <16 x double> %1)
  %3 = tail call <16 x double> (ptr, i64, i64, i64, <16 x double>, <16 x double>, ...) @llvm.linx.vcall.par.1d2u.v16f64(ptr nonnull @tadd, i64 4, i64 4, i64 1, <16 x double> %0, <16 x double> %2)
  tail call void (ptr, i64, i64, i64, <16 x double>, ...) @llvm.linx.vcall.par.0d1u.v16f64(ptr nonnull @copyout, i64 4, i64 4, i64 1, <16 x double> %3, ptr %p3)
  ret void
}

; A value used in both successors is retained across the branch. Within the
; chosen successor, duplicate operands share the same last-use decision.
; CHECK-LABEL: branch_duplicate:
; CHECK: VPAR tadd,{{.*}}[[DUP:[tumn]]]#1, [[DUP]]#1,
; CHECK: VPAR copyout,{{.*}}t#1,
; CHECK: VPAR copyout,{{.*}}{{[tumn]#1}},
define void @branch_duplicate(ptr %in, ptr %out, i1 %cond) {
entry:
  %tile = call <16 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v16f64(ptr @copyin, i64 4, i64 4, i64 1, ptr %in)
  br i1 %cond, label %left, label %right

left:
  %sum = call <16 x double> (ptr, i64, i64, i64, <16 x double>, <16 x double>, ...) @llvm.linx.vcall.par.1d2u.v16f64(ptr @tadd, i64 4, i64 4, i64 1, <16 x double> %tile, <16 x double> %tile)
  call void (ptr, i64, i64, i64, <16 x double>, ...) @llvm.linx.vcall.par.0d1u.v16f64(ptr @copyout, i64 4, i64 4, i64 1, <16 x double> %sum, ptr %out)
  ret void

right:
  call void (ptr, i64, i64, i64, <16 x double>, ...) @llvm.linx.vcall.par.0d1u.v16f64(ptr @copyout, i64 4, i64 4, i64 1, <16 x double> %tile, ptr %out)
  ret void
}

declare void @copyin(<16 x double> noundef, ptr noundef)
declare <16 x double> @llvm.linx.vcall.par.1d0u.v16f64(ptr, i64, i64, i64, ...)
declare void @tadd(<16 x double> noundef, <16 x double> noundef, <16 x double> noundef)
declare <16 x double> @llvm.linx.vcall.par.1d2u.v16f64(ptr, i64, i64, i64, <16 x double>, <16 x double>, ...)
declare void @copyout(<16 x double> noundef, ptr noundef)
declare void @llvm.linx.vcall.par.0d1u.v16f64(ptr, i64, i64, i64, <16 x double>, ...)

; Loop-carried liveness retains both duplicate reads in the loop and consumes
; the value only at the exit.
; LOOP-LABEL: loop_liveout:
; LOOP: B.IOT tile_{{[tumn]}}{{[0-9]+}}.reuse, tile_{{[tumn]}}{{[0-9]+}}.reuse, mask=1111, last
; LOOP: VPAR copyout,{{.*}}tile_{{[tumn]}}{{[0-9]+}},
;--- loop.ll
define void @loop_liveout(ptr %in, ptr %out, i64 %count) {
entry:
  %tile = call <16 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v16f64(ptr @copyin, i64 4, i64 4, i64 1, ptr %in)
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  call void asm sideeffect "BSTART.TEPL 0, U64\0AB.IOT $0, $0, mask=1111, last", "@2Tr"(<16 x double> %tile)
  %next = add i64 %i, 1
  %again = icmp ult i64 %next, %count
  br i1 %again, label %loop, label %exit

exit:
  call void (ptr, i64, i64, i64, <16 x double>, ...) @llvm.linx.vcall.par.0d1u.v16f64(ptr @copyout, i64 4, i64 4, i64 1, <16 x double> %tile, ptr %out)
  ret void
}

declare void @copyin(<16 x double> noundef, ptr noundef) #1

; Function Attrs: nounwind
declare <16 x double> @llvm.linx.vcall.par.1d0u.v16f64(ptr, i64, i64, i64, ...) #2

declare void @tadd(<16 x double> noundef, <16 x double> noundef, <16 x double> noundef) #1

; Function Attrs: nounwind
declare <16 x double> @llvm.linx.vcall.par.1d2u.v16f64(ptr, i64, i64, i64, <16 x double>, <16 x double>, ...) #2

declare void @copyout(<16 x double> noundef, ptr noundef) #1

; Function Attrs: nounwind
declare void @llvm.linx.vcall.par.0d1u.v16f64(ptr, i64, i64, i64, <16 x double>, ...) #2
