; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-prepare' -disable-output %s 2>&1 | FileCheck %s
; CHECK: view access carrier is i32
; CHECK-SAME: expected exact <32 x i32>
@view = private constant [61 x i8] c"pto.element.view:v1;dtype=s32;rows=32;cols=1;layout=cube_m32\00"
declare ptr @llvm.ptr.annotation.p0.p0(ptr, ptr, ptr, i32, ptr)
declare void @llvm.linx.experimental.element.region(metadata)
define i32 @signed_scalar_view() {
entry:
  %storage=alloca <32 x i32>, align 32
  %pointer=call ptr @llvm.ptr.annotation.p0.p0(ptr %storage, ptr @view, ptr null, i32 1, ptr null)
  %scalar=load i32, ptr %pointer, align 4
  call void @llvm.linx.experimental.element.region(metadata !0)
  ret i32 %scalar
}
!0=distinct !{}
