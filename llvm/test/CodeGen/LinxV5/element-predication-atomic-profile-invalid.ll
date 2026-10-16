; RUN: split-file %s %t
; RUN: sed 's/atomicrmw add/atomicrmw xor/' %t/profile.ll > %t/operation.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/operation.ll 2>&1 | FileCheck %s --check-prefix=PROFILE
; RUN: sed 's/ monotonic, align 4/ acquire, align 4/' %t/profile.ll > %t/ordering.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/ordering.ll 2>&1 | FileCheck %s --check-prefix=PROFILE
; RUN: sed 's/ monotonic, align 4/ syncscope("singlethread") monotonic, align 4/' %t/profile.ll > %t/scope.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/scope.ll 2>&1 | FileCheck %s --check-prefix=PROFILE
; RUN: sed 's/atomicrmw add/atomicrmw volatile add/' %t/profile.ll > %t/volatile.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/volatile.ll 2>&1 | FileCheck %s --check-prefix=PROFILE
; RUN: sed 's/align 4/align 2/' %t/profile.ll > %t/alignment.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/alignment.ll 2>&1 | FileCheck %s --check-prefix=PROFILE
; RUN: sed 's/; EXTRA/%%value = load i32, ptr %%address, align 4/' %t/alias.ll | sed 's/; OBSERVE/store i32 %%value, ptr %%sink.address, align 4/' > %t/same-load.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/same-load.ll 2>&1 | FileCheck %s --check-prefix=ALIAS
; RUN: sed 's/; EXTRA/%%value = load i32, ptr %%shift.address, align 4/' %t/alias.ll | sed 's/; OBSERVE/store i32 %%value, ptr %%sink.address, align 4/' > %t/shifted-load.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/shifted-load.ll 2>&1 | FileCheck %s --check-prefix=ALIAS
; RUN: sed 's/; EXTRA/store i32 0, ptr %%shift.address, align 4/' %t/alias.ll > %t/shifted-store.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/shifted-store.ll 2>&1 | FileCheck %s --check-prefix=ALIAS
;
; PROFILE: PTO element region: predication: atomic profile requires aligned nonvolatile system-scope monotonic i32 add
; ALIAS: PTO element region: predication: memory streams may overlap across elements

;--- profile.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)

define void @invalid_atomic_profile(ptr noalias %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  %address = getelementptr i32, ptr %histogram, i64 %i
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %next = add nuw nsw i64 %i, 1
  %more = icmp ult i64 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- alias.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)

define void @atomic_nonatomic_alias(ptr %histogram, ptr noalias %sink) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  %address = getelementptr i32, ptr %histogram, i64 %i
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %shift = add nuw i64 %i, 1
  %shift.address = getelementptr i32, ptr %histogram, i64 %shift
  %sink.address = getelementptr i32, ptr %sink, i64 %i
  ; EXTRA
  ; OBSERVE
  %next = add nuw nsw i64 %i, 1
  %more = icmp ult i64 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

