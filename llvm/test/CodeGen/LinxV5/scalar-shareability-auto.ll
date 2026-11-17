; RUN: opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -verify-each -S %s | FileCheck %s --check-prefix=IR
; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each -S %s | FileCheck %s --check-prefix=OPT
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true %s -o - | FileCheck %s --check-prefix=ASM

target triple = "linx64v5"
@g = global i32 0

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1 immarg)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1 immarg)
declare void @llvm.memmove.p0.p0.i64(ptr, ptr, i64, i1 immarg)
declare void @borrow(ptr nocapture) "linx-scalar-shareability"="intra"

; A noalias parameter can still be passed to Tile by a caller.
; IR-LABEL: define i32 @parameter
; IR: call void @llvm.linx.scalar.store(ptr %{{[^,]+}}, i64 42, i32 4, i32 1)
; IR: call i64 @llvm.linx.scalar.load(ptr %{{[^,]+}}, i32 4, i32 1)
; OPT-LABEL: define i32 @parameter
; OPT: call void @llvm.linx.scalar.store
; OPT: call i64 @llvm.linx.scalar.load
; ASM-LABEL: parameter:
; ASM: swi.intra
; ASM: lwui.intra
define i32 @parameter(ptr noalias %p) {
  store i32 42, ptr %p, align 4
  %v = load i32, ptr %p, align 4
  ret i32 %v
}

; IR-LABEL: define i32 @private_stack
; IR-NOT: @llvm.linx.scalar
; IR: store volatile i32 %x
; IR: load volatile i32
; IR: ret i32
; ASM-LABEL: private_stack:
; ASM-NOT: .intra
; ASM: c.setc.tgt
define i32 @private_stack(i32 %x) {
  %p = alloca i32, align 4
  store volatile i32 %x, ptr %p, align 4
  %v = load volatile i32, ptr %p, align 4
  ret i32 %v
}

; Even nocapture cannot prove an object is not accessed by Tile.
; IR-LABEL: define i32 @escaping_stack
; IR: call void @llvm.linx.scalar.store
; IR: call void @borrow
; IR: call i64 @llvm.linx.scalar.load
; OPT-LABEL: define i32 @escaping_stack
; OPT: call void @llvm.linx.scalar.store
; OPT: call void @borrow
; OPT: call i64 @llvm.linx.scalar.load
; ASM-LABEL: escaping_stack:
; ASM: swi.intra
; ASM: lwui.intra
define i32 @escaping_stack() {
  %p = alloca i32, align 4
  store i32 0, ptr %p, align 4
  call void @borrow(ptr %p)
  %v = load i32, ptr %p, align 4
  ret i32 %v
}

; IR-LABEL: define i32 @unknown_address
; IR: call i64 @llvm.linx.scalar.load(ptr %{{[^,]+}}, i32 4, i32 1)
define i32 @unknown_address(i64 %a) {
  %p = inttoptr i64 %a to ptr
  %v = load i32, ptr %p, align 4
  ret i32 %v
}

; IR-LABEL: define i32 @global
; IR: call i64 @llvm.linx.scalar.load(ptr @g, i32 4, i32 1)
define i32 @global() {
  %v = load i32, ptr @g, align 4
  ret i32 %v
}

; IR-LABEL: define i32 @merged
; IR: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
; IR: call i64 @llvm.linx.scalar.load(ptr %{{[^,]+}}, i32 4, i32 1)
define i32 @merged(ptr %external, i1 %b) {
  %local = alloca i32
  store i32 0, ptr %local
  %p = select i1 %b, ptr %local, ptr %external
  %v = load i32, ptr %p, align 4
  ret i32 %v
}

; IR-LABEL: define void @copies
; IR-NOT: call void @llvm.mem
; IR: call i64 @llvm.linx.scalar.load
; IR: call void @llvm.linx.scalar.store
; IR: ret void
; OPT-LABEL: define void @copies
; OPT-NOT: call void @llvm.mem
; OPT: call void @llvm.linx.scalar.store
define void @copies(ptr %p, ptr %q, i64 %n) {
  call void @llvm.memset.p0.i64(ptr align 1 %p, i8 -1, i64 %n, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr align 1 %p, ptr align 1 %q, i64 %n, i1 false)
  call void @llvm.memmove.p0.p0.i64(ptr align 1 %p, ptr align 1 %q, i64 %n, i1 false)
  ret void
}

; Tile carriers must reach their existing fixed-IntraCore lowering intact.
; IR-LABEL: define void @tile_carrier
; IR: load <32 x i64>
; IR: store <32 x i64>
; ASM-LABEL: tile_carrier:
; ASM: TLOAD
; ASM: TSTORE
define void @tile_carrier(ptr %src, ptr %dst) {
  %v = load <32 x i64>, ptr %src, align 8
  store <32 x i64> %v, ptr %dst, align 8
  ret void
}

; Small vectors become Scalar requests and must not use Private.
; IR-LABEL: define void @small_vector
; IR: call i64 @llvm.linx.scalar.load(ptr %{{[^,]+}}, i32 8, i32 1)
; IR: call void @llvm.linx.scalar.store(ptr %{{[^,]+}}, i64 {{.*}}, i32 8, i32 1)
define void @small_vector(ptr %src, ptr %dst) {
  %v = load <2 x i32>, ptr %src, align 8
  store <2 x i32> %v, ptr %dst, align 8
  ret void
}

; Scalar initialization of a nonescaping object still aliases its Tile read.
; IR-LABEL: define void @stack_tile
; IR: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
; IR: load <32 x i64>
; ASM-LABEL: stack_tile:
; ASM: swi.intra
; ASM: TLOAD
define void @stack_tile(ptr %dst) {
  %p = alloca <32 x i64>, align 128
  store i32 42, ptr %p, align 4
  %v = load <32 x i64>, ptr %p, align 128
  store <32 x i64> %v, ptr %dst, align 8
  ret void
}
