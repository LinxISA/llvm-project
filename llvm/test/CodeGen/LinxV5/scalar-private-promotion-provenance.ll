; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -S %s | FileCheck %s
; Ordinary temporary fields promote, while explicit requests retain effects.
declare i64 @llvm.linx.scalar.load(ptr, i32 immarg, i32 immarg)
declare void @llvm.linx.scalar.store(ptr, i64, i32 immarg, i32 immarg)

; CHECK-LABEL: define i64 @private_field(
; CHECK-NEXT: ret i64 %value
define i64 @private_field(i64 %value) {
  %object = alloca { i64, i32 }
  store i64 %value, ptr %object
  %r = load i64, ptr %object
  ret i64 %r
}

; CHECK-LABEL: define i64 @explicit_shared(
; CHECK: call void @llvm.linx.scalar.store(ptr {{.*}}%object, i64 %value, i32 8, i32 1)
; CHECK: call i64 @llvm.linx.scalar.load(ptr {{.*}}%object, i32 8, i32 1)
define i64 @explicit_shared(i64 %value) {
  %object = alloca i64
  call void @llvm.linx.scalar.store(ptr %object, i64 %value, i32 8, i32 1)
  %r = call i64 @llvm.linx.scalar.load(ptr %object, i32 8, i32 1)
  ret i64 %r
}
