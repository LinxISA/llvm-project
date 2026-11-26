; RUN: opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -S %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t.o

target datalayout = "e-p:64:64-i64:64-n32:64-S128"

%Record = type { i8, [2 x i64] }

define void @copy_record(ptr %out, ptr %in) {
  %value = load %Record, ptr %in, align 8
  store %Record %value, ptr %out, align 8
  ret void
}

; CHECK-LABEL: define void @copy_record(
; CHECK: call i64 @llvm.linx.scalar.load(ptr {{.*}}, i32 1, i32 1)
; CHECK: call i64 @llvm.linx.scalar.load(ptr {{.*}}, i32 8, i32 1)
; CHECK: call i64 @llvm.linx.scalar.load(ptr {{.*}}, i32 8, i32 1)
; CHECK: call void @llvm.linx.scalar.store(ptr {{.*}}, i64 {{.*}}, i32 1, i32 1)
; CHECK: call void @llvm.linx.scalar.store(ptr {{.*}}, i64 {{.*}}, i32 8, i32 1)
; CHECK: call void @llvm.linx.scalar.store(ptr {{.*}}, i64 {{.*}}, i32 8, i32 1)
