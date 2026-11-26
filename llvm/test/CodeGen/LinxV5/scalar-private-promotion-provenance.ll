; RUN: opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -S %s | FileCheck %s

@shape = private constant i32 8192

; Keep immutable tiling visible to constant propagation before scoped calls
; obscure it. Volatile and explicitly scoped loads retain their requests.
; CHECK-LABEL: define i32 @constant_shape
; CHECK-NEXT: ret i32 8192
define i32 @constant_shape() {
  %v = load i32, ptr @shape, align 4
  ret i32 %v
}

; CHECK-LABEL: define i64 @normal_load
; CHECK: call i64 @llvm.linx.scalar.load{{.*}}!linx.scalar.implicit
define i64 @normal_load(ptr %p) {
  %v = load i64, ptr %p, align 8
  ret i64 %v
}

; CHECK-LABEL: define void @normal_store
; CHECK: call void @llvm.linx.scalar.store{{.*}}!linx.scalar.implicit
define void @normal_store(ptr %p, i64 %v) {
  store i64 %v, ptr %p, align 8
  ret void
}

; CHECK-LABEL: define i64 @volatile_load
; CHECK: call i64 @llvm.linx.scalar.load
; CHECK-NOT: !linx.scalar.implicit
; CHECK: ret
define i64 @volatile_load(ptr %p) {
  %v = load volatile i64, ptr %p, align 8
  ret i64 %v
}

; CHECK-LABEL: define i64 @explicit_load
; CHECK: call i64 @llvm.linx.scalar.load
; CHECK-NOT: !linx.scalar.implicit
; CHECK: ret
define i64 @explicit_load(ptr %p) "linx-scalar-shareability"="intra" {
  %v = load i64, ptr %p, align 8
  ret i64 %v
}
