; RUN: clang %s --target=linx64v5 -S -O2 -mllvm -stop-after=prologepilog -fstack-protector-strong -mllvm -linxv5-enable-stack-guard-with-cwr=true -o - | FileCheck %s --check-prefixes=CHECK

target triple = "linx64v5"

; CHECK-LABEL: name: stack_check1
; CHECK: SSR_GET 2080

define dso_local signext i32 @stack_check1(i32 noundef signext %0) local_unnamed_addr #0 {
  %2 = alloca [100 x i32], align 4
  call void @llvm.lifetime.start.p0(i64 400, ptr nonnull %2)
  call void @bar(ptr noundef nonnull %2)
  %3 = sext i32 %0 to i64
  %4 = getelementptr inbounds [100 x i32], ptr %2, i64 0, i64 %3
  %5 = load i32, ptr %4, align 4
  call void @llvm.lifetime.end.p0(i64 400, ptr nonnull %2)
  ret i32 %5
}

attributes #0 = { sspstrong }

declare dso_local void @bar(ptr noundef) local_unnamed_addr
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture)
