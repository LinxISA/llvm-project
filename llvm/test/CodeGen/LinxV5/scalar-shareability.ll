; RUN: llc -mtriple=linx64v5 -mcpu=janus -verify-machineinstrs -linxv5-enable-clock-hand-opt=false < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj < %s -o %t.o
; RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s --check-prefix=OBJ

declare i64 @llvm.linx.scalar.load(ptr, i32 immarg, i32 immarg)
declare void @llvm.linx.scalar.store(ptr, i64, i32 immarg, i32 immarg)
declare i64 @llvm.linx.scalar.atomic(ptr, i64, i64, i32 immarg, i32 immarg, i32 immarg, i32 immarg)

; CHECK-LABEL: load_intra:
; CHECK: lwui.intra
; OBJ: lwui.intra
define i64 @load_intra(ptr %p) {
  %v = call i64 @llvm.linx.scalar.load(ptr %p, i32 4, i32 1)
  ret i64 %v
}
; CHECK-LABEL: store_inter:
; CHECK: sdi.inter
; OBJ: sdi.inter
define void @store_inter(ptr %p, i64 %v) {
  call void @llvm.linx.scalar.store(ptr %p, i64 %v, i32 8, i32 2)
  ret void
}
; CHECK-LABEL: add_inter:
; CHECK: ld.add.aqrl.inter
; OBJ: ld.add.aqrl.inter
define i64 @add_inter(ptr %p, i64 %v) {
  %old = call i64 @llvm.linx.scalar.atomic(ptr %p, i64 %v, i64 0, i32 8, i32 2, i32 3, i32 0)
  ret i64 %old
}
; CHECK-LABEL: cas_intra:
; CHECK: casw.aq.intra
; OBJ: casw.aq.intra
define i64 @cas_intra(ptr %p, i64 %expected, i64 %desired) {
  %old = call i64 @llvm.linx.scalar.atomic(ptr %p, i64 %expected, i64 %desired, i32 4, i32 1, i32 2, i32 2)
  ret i64 %old
}
; CHECK-LABEL: lrsc:
; CHECK: lr.d.aq.intra
; CHECK: sc.d.rl.intra
; OBJ: lr.d.aq.intra
; OBJ: sc.d.rl.intra
define i64 @lrsc(ptr %p, i64 %v) {
  %old = call i64 @llvm.linx.scalar.atomic(ptr %p, i64 0, i64 0, i32 8, i32 1, i32 2, i32 3)
  %status = call i64 @llvm.linx.scalar.atomic(ptr %p, i64 %v, i64 0, i32 8, i32 1, i32 1, i32 4)
  ret i64 %status
}
; CHECK-LABEL: sequential:
; CHECK: fence.d
; CHECK: swapb.aqrl.inter
; CHECK: fence.d
; OBJ: swapb.aqrl.inter
define i64 @sequential(ptr %p, i64 %v) {
  %old = call i64 @llvm.linx.scalar.atomic(ptr %p, i64 %v, i64 0, i32 1, i32 2, i32 4, i32 1)
  ret i64 %old
}

; Stack memory explicitly exposed to shared consumers keeps its request scope.
; CHECK-LABEL: shared_stack:
; CHECK: sdi.intra
; CHECK: ldi.intra
; OBJ: sdi.intra
; OBJ: ldi.intra
define i64 @shared_stack(i64 %v) {
  %p = alloca i64, align 8
  call void @llvm.linx.scalar.store(ptr %p, i64 %v, i32 8, i32 1)
  %r = call i64 @llvm.linx.scalar.load(ptr %p, i32 8, i32 1)
  ret i64 %r
}
