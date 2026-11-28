; RUN: llc -mtriple=linx64v5 -mcpu=janus -verify-machineinstrs -stop-before=linxv5-treg-to-offset < %s -o /dev/null
; RUN: llc -mtriple=linx64v5 -mcpu=janus < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -O0 < %s | FileCheck %s

declare void @llvm.memset.p0.i64(ptr, i8, i64, i1 immarg)
declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1 immarg)

; CHECK-LABEL: shared_scratch:
; CHECK: swi.intra
; CHECK: lwui.intra
define i32 @shared_scratch(ptr %p, i32 %x) #0 {
  store volatile i32 %x, ptr %p, align 4
  %v = load volatile i32, ptr %p, align 4
  ret i32 %v
}
; CHECK-LABEL: shared_fill:
; CHECK: sbi.intra
; CHECK-NOT: bstart{{.*}}memset
define void @shared_fill(ptr %p, i64 %n) #0 {
  call void @llvm.memset.p0.i64(ptr align 1 %p, i8 -1, i64 %n, i1 false)
  ret void
}
; CHECK-LABEL: shared_copy:
; CHECK: lbui.intra
; CHECK: sbi.intra
define void @shared_copy(ptr %d, ptr %s, i64 %n) #0 {
  call void @llvm.memcpy.p0.p0.i64(ptr align 1 %d, ptr align 1 %s, i64 %n, i1 false)
  ret void
}
; CHECK-LABEL: shared_atomic:
; CHECK: ld.add.aqrl.inter
define i64 @shared_atomic(ptr %p, i64 %v) #1 {
  %old = atomicrmw add ptr %p, i64 %v acq_rel
  ret i64 %old
}
; CHECK-LABEL: shared_cas:
; CHECK: casd.aqrl.inter
define {i64, i1} @shared_cas(ptr %p, i64 %v, i64 %w) #1 {
  %old = cmpxchg ptr %p, i64 %v, i64 %w acq_rel acquire
  ret {i64, i1} %old
}
; CHECK-LABEL: inter_vector:
; CHECK: sdi.inter
; CHECK: sdi.inter
; CHECK: ldi.inter
; CHECK: ldi.inter
define <2 x i64> @inter_vector(ptr %p, <2 x i64> %v) #1 {
  store volatile <2 x i64> %v, ptr %p, align 8
  %r = load volatile <2 x i64>, ptr %p, align 8
  ret <2 x i64> %r
}
; CHECK-LABEL: ordered_load_store:
; CHECK: swapd.rl.inter
; CHECK: lr.d.aq.inter
define i64 @ordered_load_store(ptr %p, i64 %v) #1 {
  store atomic i64 %v, ptr %p release, align 8
  %r = load atomic i64, ptr %p acquire, align 8
  ret i64 %r
}
; Explicit selectors take precedence over the function default.
; CHECK-LABEL: explicit_private:
; CHECK: lwui.private
declare i64 @llvm.linx.scalar.load(ptr, i32 immarg, i32 immarg)
define i64 @explicit_private(ptr %p) #1 {
  %r = call i64 @llvm.linx.scalar.load(ptr %p, i32 4, i32 0)
  ret i64 %r
}
attributes #0 = { "linx-scalar-shareability"="intra" }
attributes #1 = { "linx-scalar-shareability"="inter" }

; Ordinary atomics retain their memory order and use the Private default.
; CHECK-LABEL: private_atomic:
; CHECK-NOT: .intra
; CHECK-NOT: .inter
; CHECK: ld.add.aqrl
; CHECK: c.setc.tgt
define i64 @private_atomic(ptr %p, i64 %v) {
  %old = atomicrmw add ptr %p, i64 %v acq_rel
  ret i64 %old
}
; CHECK-LABEL: private_volatile:
; CHECK-NOT: .intra
; CHECK-NOT: .inter
; CHECK: sw
; CHECK: lw
; CHECK: c.setc.tgt
define i32 @private_volatile(ptr %p, i32 %v) {
  store volatile i32 %v, ptr %p, align 4
  %r = load volatile i32, ptr %p, align 4
  ret i32 %r
}
