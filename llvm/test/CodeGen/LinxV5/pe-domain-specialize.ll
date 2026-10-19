; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -S %t/positive.ll | FileCheck %s --check-prefix=SPEC
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/recursive.ll 2>&1 | FileCheck %s --check-prefix=RECURSE
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/external.ll 2>&1 | FileCheck %s --check-prefix=EXTERNAL
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/indirect.ll 2>&1 | FileCheck %s --check-prefix=INDIRECT
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/shared.ll 2>&1 | FileCheck %s --check-prefix=SHARED
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/raw-asm.ll 2>&1 | FileCheck %s --check-prefix=ASM
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/domain-reset-asm.ll 2>&1 | FileCheck %s --check-prefix=ASM
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/direct-asm.ll 2>&1 | FileCheck %s --check-prefix=ASM
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/icall-asm.ll 2>&1 | FileCheck %s --check-prefix=ASM
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/fill-asm.ll 2>&1 | FileCheck %s --check-prefix=ASM
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-specialize -disable-output %t/semicolon-asm.ll 2>&1 | FileCheck %s --check-prefix=SEPARATOR
;
; SPEC-LABEL: define linkonce_odr void @linkonce_helper
; SPEC: call <4 x i32> @tile_helper(<4 x i32> zeroinitializer, ptr %p)
; SPEC-LABEL: define void @root8
; SPEC: call void @middle.pto.pe8.{{[0-9]+}}(ptr %p)
; SPEC: call void @linkonce_helper.pto.pe8.{{[0-9]+}}(ptr %p)
; SPEC: call <4 x i32> @tile_helper.pto.pe8.{{[0-9]+}}(<4 x i32> zeroinitializer, ptr %p)
; SPEC-NOT: call void @llvm.memset
; SPEC-LABEL: define void @root15
; SPEC: call void @middle(ptr %p)
; SPEC: call <4 x i32> @tile_helper(<4 x i32> zeroinitializer, ptr %p)
; SPEC-DAG: define internal void @middle.pto.pe8.
; SPEC-DAG: call void @leaf.pto.pe8.{{[0-9]+}}(ptr %p)
; SPEC-DAG: define internal void @leaf.pto.pe8.
; SPEC-DAG: store i32 7, ptr %p
; SPEC-DAG: define internal void @linkonce_helper.pto.pe8.
; SPEC-DAG: call <4 x i32> @tile_helper.pto.pe8.{{[0-9]+}}(<4 x i32> zeroinitializer, ptr %p)
; SPEC-DAG: define internal <4 x i32> @tile_helper.pto.pe8.
; SPEC: attributes {{.*}} "linx-tile-pe-domain"="8"
; SPEC: attributes {{.*}} "linx-local-tile-abi" "linx-tile-pe-domain"="8"
; SPEC: !{i32 1, !"linx.pe-domain.lowered", i32 1}
; RECURSE: PTO PE domain: recursive helper graph has no proved domain closure
; EXTERNAL: PTO PE domain: external calls have no proved PE-domain ownership
; INDIRECT: PTO PE domain: indirect calls have no proved PE-domain ownership
; SHARED: PTO PE domain: nondefault domains support proved Local Tile operations only
; ASM: PTO PE domain: inline assembly has no proved local PE-domain transfer contract
; SEPARATOR: PTO PE domain: inline assembly statement separators are not allowed in a nondefault PE domain

;--- positive.ll
define internal void @leaf(ptr %p) { store i32 7, ptr %p ret void }
define internal void @middle(ptr %p) { call void @leaf(ptr %p) ret void }
define linkonce_odr void @linkonce_helper(ptr %p) {
  store i32 9, ptr %p
  %v = call <4 x i32> @tile_helper(<4 x i32> zeroinitializer, ptr %p)
  ret void
}
define internal <4 x i32> @tile_helper(<4 x i32> %v, ptr %out) #1 {
  %lane = extractelement <4 x i32> %v, i32 0
  store i32 %lane, ptr %out
  ret <4 x i32> %v
}
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1 immarg)
define void @root8(ptr %p) #0 {
  call void @middle(ptr %p)
  call void @linkonce_helper(ptr %p)
  %v = call <4 x i32> @tile_helper(<4 x i32> zeroinitializer, ptr %p)
  call void @llvm.memset.p0.i64(ptr %p, i8 0, i64 16, i1 false)
  call void asm sideeffect "BSTART.CUBE TMATMUL, FP32\0AB.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0\0AB.IOT mask=1000, last\0ABSTOP", ""()
  ret void
}
define void @root15(ptr %p) {
  call void @middle(ptr %p)
  %v = call <4 x i32> @tile_helper(<4 x i32> zeroinitializer, ptr %p)
  ret void
}
attributes #0 = { noinline "linx-tile-pe-domain"="8" }
attributes #1 = { "linx-local-tile-abi" }

;--- raw-asm.ll
define void @root() #0 {
  call void asm sideeffect ".4byte 0", ""()
  ret void
}
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- recursive.ll
define internal void @loop(ptr %p) { call void @loop(ptr %p) ret void }
define void @root(ptr %p) #0 { call void @loop(ptr %p) ret void }
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- domain-reset-asm.ll
define void @root() #0 {
  call void asm sideeffect ".pto_pe_domain 15", ""()
  ret void
}
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- semicolon-asm.ll
define void @root() #0 {
  call void asm sideeffect "BSTART.TMOV U32; bstart.std direct, target", ""()
  ret void
}
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- direct-asm.ll
define void @root() #0 {
  call void asm sideeffect "BSTART.TMOV U32\0Abstart.std direct, target", ""()
  ret void
}
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- icall-asm.ll
define void @root() #0 {
  call void asm sideeffect "bstart.icall", ""()
  ret void
}
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- fill-asm.ll
define void @root() #0 {
  call void asm sideeffect ".fill 4, 4, 0\0A.quad 0\0A.ascii \22x\22\0A.zero 4", ""()
  ret void
}
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- external.ll
declare void @unknown(ptr)
define void @root(ptr %p) #0 { call void @unknown(ptr %p) ret void }
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- indirect.ll
define void @root(ptr %fn, ptr %p) #0 { call void %fn(ptr %p) ret void }
attributes #0 = { noinline "linx-tile-pe-domain"="8" }

;--- shared.ll
define void @root() #0 {
  call void asm sideeffect "B.IOS mask=1000", ""()
  ret void
}
attributes #0 = { noinline "linx-tile-pe-domain"="8" }
