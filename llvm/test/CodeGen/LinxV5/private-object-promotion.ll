; RUN: opt -mtriple=linx64v5 -passes=linx-v5-private-object-promotion -S %s | FileCheck %s

declare i64 @llvm.linx.scalar.load(ptr, i32 immarg, i32 immarg)
declare void @llvm.linx.scalar.store(ptr, i64, i32 immarg, i32 immarg)
declare void @escape(ptr)

; A scoped wrapper field can become private after inlining. Shape fields do
; not alias its opaque handle and must not prevent SSA promotion.
; CHECK-LABEL: define i64 @private_field
; CHECK-NOT: call {{.*}}@llvm.linx.scalar
; CHECK: ret i64 %handle
define i64 @private_field(i64 %handle) {
  %object = alloca {i64, i32}, align 8
  %shape = getelementptr {i64, i32}, ptr %object, i64 0, i32 1
  call void @llvm.linx.scalar.store(ptr %shape, i64 32, i32 4, i32 1), !linx.scalar.implicit !0
  call void @llvm.linx.scalar.store(ptr %object, i64 %handle, i32 8, i32 1), !linx.scalar.implicit !0
  %r = call i64 @llvm.linx.scalar.load(ptr %object, i32 8, i32 1), !linx.scalar.implicit !0
  ret i64 %r
}

; CHECK-LABEL: define i64 @escaped_field
; CHECK: call void @llvm.linx.scalar.store
; CHECK: call void @escape
; CHECK: call i64 @llvm.linx.scalar.load
define i64 @escaped_field(i64 %handle) {
  %object = alloca i64
  call void @llvm.linx.scalar.store(ptr %object, i64 %handle, i32 8, i32 1), !linx.scalar.implicit !0
  call void @escape(ptr %object)
  %r = call i64 @llvm.linx.scalar.load(ptr %object, i32 8, i32 1), !linx.scalar.implicit !0
  ret i64 %r
}

; Explicit scoped requests and source volatile accesses are not promotable.
; CHECK-LABEL: define i64 @explicit_field
; CHECK: call void @llvm.linx.scalar.store
; CHECK: call i64 @llvm.linx.scalar.load
define i64 @explicit_field(i64 %handle) {
  %object = alloca i64
  call void @llvm.linx.scalar.store(ptr %object, i64 %handle, i32 8, i32 1)
  %r = call i64 @llvm.linx.scalar.load(ptr %object, i32 8, i32 1)
  ret i64 %r
}

; CHECK-LABEL: define i64 @volatile_overlap
; CHECK: store volatile
; CHECK: call i64 @llvm.linx.scalar.load
define i64 @volatile_overlap(i64 %handle) {
  %object = alloca i64
  store volatile i64 %handle, ptr %object
  %r = call i64 @llvm.linx.scalar.load(ptr %object, i32 8, i32 1), !linx.scalar.implicit !0
  ret i64 %r
}

; CHECK-LABEL: define i64 @overlap
; CHECK: call void @llvm.linx.scalar.store
; CHECK: call i64 @llvm.linx.scalar.load
define i64 @overlap(i64 %handle) {
  %object = alloca i64
  call void @llvm.linx.scalar.store(ptr %object, i64 %handle, i32 8, i32 1), !linx.scalar.implicit !0
  store i32 0, ptr %object
  %r = call i64 @llvm.linx.scalar.load(ptr %object, i32 8, i32 1), !linx.scalar.implicit !0
  ret i64 %r
}
@immutable = private constant i32 8192
; CHECK-LABEL: define i64 @constant_shape
; CHECK-NOT: call
; CHECK: ret i64 8192
define i64 @constant_shape() {
  %r = call i64 @llvm.linx.scalar.load(ptr @immutable, i32 4, i32 1), !linx.scalar.implicit !0
  ret i64 %r
}
; The carrier transport itself stays in memory; adjacent scalar shape fields
; may still be promoted after the whole byte-range proof.
declare void @llvm.linx.blk.tstore.v32i32(i64, i64, i64, i64, i64, ptr, i64, <32 x i32>)
; CHECK-LABEL: define i64 @shape_beside_carrier
; CHECK: call void @llvm.linx.blk.tstore
; CHECK-NOT: call {{.*}}@llvm.linx.scalar
; CHECK: ret i64 32
define i64 @shape_beside_carrier(<32 x i32> %v) {
  %object = alloca {i64, <32 x i32>}, align 32
  %payload = getelementptr {i64, <32 x i32>}, ptr %object, i64 0, i32 1
  call void @llvm.linx.scalar.store(ptr %object, i64 32, i32 8, i32 1), !linx.scalar.implicit !0
  call void @llvm.linx.blk.tstore.v32i32(i64 1, i64 32, i64 1, i64 1, i64 24, ptr %payload, i64 4, <32 x i32> %v)
  %r = call i64 @llvm.linx.scalar.load(ptr %object, i32 8, i32 1), !linx.scalar.implicit !0
  ret i64 %r
}
!0 = !{}

; A strided transport reaches past the vector carrier into the scalar field.
; CHECK-LABEL: define i64 @strided_carrier_overlap
; CHECK: call void @llvm.linx.scalar.store
; CHECK: call void @llvm.linx.blk.tstore
; CHECK: call i64 @llvm.linx.scalar.load
define i64 @strided_carrier_overlap(<32 x i32> %v) {
  %object = alloca {<64 x i32>, i64}, align 32
  %shape = getelementptr {<64 x i32>, i64}, ptr %object, i64 0, i32 1
  call void @llvm.linx.scalar.store(ptr %shape, i64 32, i32 8, i32 1), !linx.scalar.implicit !0
  call void @llvm.linx.blk.tstore.v32i32(i64 1, i64 2, i64 1, i64 1, i64 24, ptr %object, i64 256, <32 x i32> %v)
  %r = call i64 @llvm.linx.scalar.load(ptr %shape, i32 8, i32 1), !linx.scalar.implicit !0
  ret i64 %r
}
