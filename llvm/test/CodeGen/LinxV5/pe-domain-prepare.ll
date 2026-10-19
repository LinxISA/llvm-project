; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-prepare -S %t/positive.ll | FileCheck %s --check-prefix=PREP
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-prepare -disable-output %t/invalid-mask.ll 2>&1 | FileCheck %s --check-prefix=MASK
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-prepare -disable-output %t/force-inline.ll 2>&1 | FileCheck %s --check-prefix=INLINE
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-prepare -disable-output %t/vector-boundary.ll 2>&1 | FileCheck %s --check-prefix=SIGNATURE
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-prepare -disable-output %t/tile-reference.ll 2>&1 | FileCheck %s --check-prefix=TILE
; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-pe-domain-prepare -disable-output %t/tile-transport-forward.ll 2>&1 | FileCheck %s --check-prefix=TILE
;
; PREP: define void @pe0(ptr %p) #[[PE0:[0-9]+]]
; PREP: define void @explicit_default(ptr %p) #[[ALL:[0-9]+]]
; PREP: attributes #[[PE0]] = { noinline "linx-tile-pe-domain"="8" }
; PREP: attributes #[[ALL]] = { noinline "linx-tile-pe-domain"="15" }
; MASK: PTO PE domain: annotation requires one defined function and a native nonzero architectural mask
; INLINE: PTO PE domain: explicit PE-domain boundaries may not be force-inlined
; SIGNATURE: PTO PE domain: cross-domain boundary requires a scalar/GM signature
; TILE: PTO PE domain: Tile-reference arguments may not cross a function PE domain boundary

;--- positive.ll
@.name = private constant [14 x i8] c"pto.pe_domain\00"
@.file = private constant [2 x i8] c"x\00"
@.args8 = private constant { i32 } { i32 8 }
@.args15 = private constant { i32 } { i32 15 }
@llvm.global.annotations = appending global [2 x { ptr, ptr, ptr, i32, ptr }] [
  { ptr, ptr, ptr, i32, ptr } { ptr @pe0, ptr @.name, ptr @.file, i32 1, ptr @.args8 },
  { ptr, ptr, ptr, i32, ptr } { ptr @explicit_default, ptr @.name, ptr @.file, i32 2, ptr @.args15 }
]
define void @pe0(ptr %p) { store i32 1, ptr %p ret void }
define void @explicit_default(ptr %p) { store i32 2, ptr %p ret void }

;--- invalid-mask.ll
@.name = private constant [14 x i8] c"pto.pe_domain\00"
@.file = private constant [2 x i8] c"x\00"
@.args = private constant { i32 } { i32 3 }
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [
  { ptr, ptr, ptr, i32, ptr } { ptr @bad, ptr @.name, ptr @.file, i32 1, ptr @.args }
]
define void @bad(ptr %p) { ret void }

;--- force-inline.ll
@.name = private constant [14 x i8] c"pto.pe_domain\00"
@.file = private constant [2 x i8] c"x\00"
@.args = private constant { i32 } { i32 8 }
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [
  { ptr, ptr, ptr, i32, ptr } { ptr @bad, ptr @.name, ptr @.file, i32 1, ptr @.args }
]
define void @bad(ptr %p) #0 { ret void }
attributes #0 = { alwaysinline }

;--- vector-boundary.ll
@.name = private constant [14 x i8] c"pto.pe_domain\00"
@.file = private constant [2 x i8] c"x\00"
@.args = private constant { i32 } { i32 8 }
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [
  { ptr, ptr, ptr, i32, ptr } { ptr @bad, ptr @.name, ptr @.file, i32 1, ptr @.args }
]
define <4 x i32> @bad(ptr %p) { ret <4 x i32> zeroinitializer }

;--- tile-reference.ll
@.name = private constant [14 x i8] c"pto.pe_domain\00"
@.file = private constant [2 x i8] c"x\00"
@.args = private constant { i32 } { i32 8 }
@.carrier = private constant [21 x i8] c"linx.tile.carrier:v1\00"
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [
  { ptr, ptr, ptr, i32, ptr } { ptr @bad, ptr @.name, ptr @.file, i32 1, ptr @.args }
]
declare ptr @llvm.ptr.annotation.p0(ptr, ptr, ptr, i32, ptr)
define void @bad(ptr %tile) {
  %annotated = call ptr @llvm.ptr.annotation.p0(
      ptr %tile, ptr @.carrier, ptr @.file, i32 1, ptr null)
  store i32 1, ptr %annotated
  ret void
}

;--- tile-transport-forward.ll
@.name = private constant [14 x i8] c"pto.pe_domain\00"
@.file = private constant [2 x i8] c"x\00"
@.args = private constant { i32 } { i32 8 }
@llvm.global.annotations = appending global [1 x { ptr, ptr, ptr, i32, ptr }] [
  { ptr, ptr, ptr, i32, ptr } { ptr @bad, ptr @.name, ptr @.file, i32 1, ptr @.args }
]
define internal void @use(ptr %tile, ptr %gm) {
  %data = getelementptr i8, ptr %tile, i64 32
  %value = load <128 x i32>, ptr %data, align 32, !linx.tile.transport !0
  %lane = extractelement <128 x i32> %value, i32 0
  store i32 %lane, ptr %gm
  ret void
}
define void @bad(ptr %tile, ptr %gm) { call void @use(ptr %tile, ptr %gm) ret void }
!0 = !{}
