; RUN: split-file %s %t
; RUN: sed 's/%value = load i32/%value = load volatile i32/' %t/base.ll > %t/volatile.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-prepare' -disable-output %t/volatile.ll 2>&1 | FileCheck %s --check-prefix=ACCESS
; RUN: sed 's/%value = load i32, ptr %view, align 4/%value = load atomic i32, ptr %view monotonic, align 4/' %t/base.ll > %t/atomic.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-prepare' -disable-output %t/atomic.ll 2>&1 | FileCheck %s --check-prefix=ACCESS
; RUN: sed 's/alloca \[32 x i32\]/alloca i32/' %t/base.ll > %t/undersized.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-prepare' -disable-output %t/undersized.ll 2>&1 | FileCheck %s --check-prefix=RANGE
; RUN: sed 's/alloca \[32 x i32\], align 32/alloca [32 x i32], align 4/' %t/base.ll > %t/misaligned.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-prepare' -disable-output %t/misaligned.ll 2>&1 | FileCheck %s --check-prefix=ALIGN

; ACCESS: PTO element region: view access carrier is i32
; RANGE: PTO element region: logical view requires a proved 128-byte backing range
; ALIGN: PTO element region: logical view requires proved 32-byte alignment

;--- base.ll
target triple = "linx64v5"
@annotation = private constant [61 x i8] c"pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32\00"
@source = private constant [1 x i8] c"x"
declare ptr @llvm.ptr.annotation.p0(ptr, ptr, ptr, i32, ptr)
declare void @llvm.linx.experimental.element.region(metadata)

define i32 @scalarized_view() {
entry:
  %storage = alloca [32 x i32], align 32
  %view = call ptr @llvm.ptr.annotation.p0(
      ptr %storage, ptr @annotation, ptr @source, i32 1, ptr null)
  call void @llvm.linx.experimental.element.region(metadata !0)
  %value = load i32, ptr %view, align 4
  ret i32 %value
}
!0 = distinct !{}
