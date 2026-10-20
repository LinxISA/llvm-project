; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-unsupported -disable-output %t/raw.ll 2>&1 | FileCheck %s
; RUN: not llc -mtriple=linx64v5 -o /dev/null %t/raw.ll 2>&1 | FileCheck %s --check-prefix=LLC
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-unsupported -disable-output %t/linked-mixed.ll 2>&1 | FileCheck %s --check-prefix=MIXED
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-unsupported -disable-output %t/prepared-only.ll 2>&1 | FileCheck %s --check-prefix=PREPARED
; CHECK: PTO PE domain: -O0 cannot consume a PTO PE domain
; LLC: PTO PE domain: raw llc input cannot consume a PTO PE domain
; MIXED: PTO PE domain: linked annotation was not consumed by PE-domain preparation
; PREPARED: PTO PE domain: function domain was prepared but not specialized

;--- raw.ll
define void @root(ptr %p) #0 { store i32 1, ptr %p ret void }
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- linked-mixed.ll
@.name = private constant [14 x i8] c"pto.pe_domain\00"
@.file = private constant [2 x i8] c"x\00"
@.args = private constant { i32 } { i32 8 }
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [
  { ptr, ptr, ptr, i32, ptr } { ptr @new_raw, ptr @.name, ptr @.file, i32 1, ptr @.args }
]
define void @processed(ptr %p) #0 { ret void }
define void @new_raw(ptr %p) { ret void }
attributes #0 = { noinline "linx-tile-pe-domain"="8" "linx-tile-pe-domain-lowered" }
!llvm.module.flags = !{!0}
!0 = !{i32 1, !"linx.pe-domain.lowered", i32 1}

;--- prepared-only.ll
define void @prepared(ptr %p) #0 { ret void }
attributes #0 = { noinline "linx-tile-pe-domain"="8" }
!llvm.module.flags = !{!0}
!0 = !{i32 1, !"linx.pe-domain.lowered", i32 1}
