; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-prepare' -disable-output %t/offset.ll 2>&1 | FileCheck %s --check-prefix=POINTER
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-prepare' -disable-output %t/dynamic.ll 2>&1 | FileCheck %s --check-prefix=POINTER
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-prepare' -disable-output %t/select.ll 2>&1 | FileCheck %s --check-prefix=POINTER

; POINTER: PTO element region: logical view pointer must remain exact

;--- offset.ll
@contract = private constant [61 x i8] c"pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32\00"
@file = private constant [2 x i8] c"x\00"
declare ptr @llvm.ptr.annotation.p0(ptr, ptr, ptr, i32, ptr)
declare void @llvm.linx.experimental.element.region(metadata)
define void @offset() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  %storage = alloca <32 x i32>, align 32
  %annotated = call ptr @llvm.ptr.annotation.p0(ptr %storage, ptr @contract, ptr @file, i32 1, ptr null)
  %shifted = getelementptr i32, ptr %annotated, i64 1
  %value = load <32 x i32>, ptr %shifted, align 4
  ret void
}
!0 = distinct !{}

;--- dynamic.ll
@contract = private constant [61 x i8] c"pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32\00"
@file = private constant [2 x i8] c"x\00"
declare ptr @llvm.ptr.annotation.p0(ptr, ptr, ptr, i32, ptr)
declare void @llvm.linx.experimental.element.region(metadata)
define void @dynamic(i64 %index) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  %storage = alloca <32 x i32>, align 32
  %annotated = call ptr @llvm.ptr.annotation.p0(ptr %storage, ptr @contract, ptr @file, i32 1, ptr null)
  %shifted = getelementptr i32, ptr %annotated, i64 %index
  %value = load <32 x i32>, ptr %shifted, align 4
  ret void
}
!0 = distinct !{}

;--- select.ll
@contract = private constant [61 x i8] c"pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32\00"
@file = private constant [2 x i8] c"x\00"
declare ptr @llvm.ptr.annotation.p0(ptr, ptr, ptr, i32, ptr)
declare void @llvm.linx.experimental.element.region(metadata)
define void @select_merge(i1 %condition) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  %storage = alloca <32 x i32>, align 32
  %other = alloca <32 x i32>, align 32
  %annotated = call ptr @llvm.ptr.annotation.p0(ptr %storage, ptr @contract, ptr @file, i32 1, ptr null)
  %selected = select i1 %condition, ptr %annotated, ptr %other
  %value = load <32 x i32>, ptr %selected, align 32
  ret void
}
!0 = distinct !{}
