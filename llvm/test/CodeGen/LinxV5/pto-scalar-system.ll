; RUN: llc -mtriple=linx64v5 -mcpu=janus -linxv5-enable-clock-hand-opt=false < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -linxv5-enable-clock-hand-opt=false -filetype=obj < %s -o %t.o
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"

declare void @llvm.linx.scalar.dma(ptr, ptr)
declare void @llvm.linx.scalar.bwt(i64)
declare void @llvm.linx.scalar.sync.core() convergent noduplicate
declare void @llvm.prefetch.p0(ptr, i32 immarg, i32 immarg, i32 immarg)

; CHECK-LABEL: dma64:
; CHECK: dma [a1], a0
define void @dma64(ptr %dst, ptr %src) {
  call void @llvm.linx.scalar.dma(ptr %dst, ptr %src)
  ret void
}

; CHECK-LABEL: wait_timeout:
; CHECK: bwt a0
define void @wait_timeout(i64 %timeout) {
  call void @llvm.linx.scalar.bwt(i64 %timeout)
  ret void
}

; CHECK-LABEL: collective:
; CHECK: BSTART.SYS FALL
; CHECK-NEXT: sync.core
; CHECK-NEXT: BSTOP
define void @collective(ptr %p, i64 %v) {
  store volatile i64 %v, ptr %p, align 8
  call void @llvm.linx.scalar.sync.core()
  store volatile i64 1, ptr %p, align 8
  ret void
}

; CHECK-LABEL: repeated_collective:
; CHECK: BSTART.SYS FALL
; CHECK-NEXT: sync.core
; CHECK-NEXT: BSTOP
; CHECK: BSTART.SYS FALL
; CHECK-NEXT: sync.core
; CHECK-NEXT: BSTOP
define void @repeated_collective() {
  call void @llvm.linx.scalar.sync.core()
  call void @llvm.linx.scalar.sync.core()
  ret void
}

; CHECK-LABEL: hint:
; CHECK: prfi.u [a0, -17]
define void @hint(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 -17
  call void @llvm.prefetch.p0(ptr %q, i32 0, i32 3, i32 1)
  ret void
}

; CHECK-LABEL: hint_write:
; CHECK: prfi.u [a0, 2047]
define void @hint_write(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 2047
  call void @llvm.prefetch.p0(ptr %q, i32 1, i32 0, i32 1)
  ret void
}

; CHECK-LABEL: fences:
; CHECK: fence.d
; CHECK: fence.d
; CHECK: fence.d
; CHECK: fence.d
define void @fences() {
  fence acquire
  fence release
  fence acq_rel
  fence seq_cst
  ret void
}

; CHECK-LABEL: mixed_fp_atomic:
; CHECK: C.BSTART.AUX
; CHECK: casd.aqrl
; CHECK: C.BSTART.FP
; CHECK: fadd
define double @mixed_fp_atomic(ptr %p, i64 %old, i64 %new, double %x, double %y) {
  %r = cmpxchg ptr %p, i64 %old, i64 %new acq_rel acquire
  %v = fadd double %x, %y
  ret double %v
}

; CHECK-LABEL: hint_indexed:
; CHECK: prf [a0, a1<<3]
define void @hint_indexed(ptr %p, i64 %index) {
  %q = getelementptr i64, ptr %p, i64 %index
  call void @llvm.prefetch.p0(ptr %q, i32 0, i32 2, i32 1)
  ret void
}

; SYNC.CORE must be exactly four bytes after a four-byte SYS header and
; immediately followed by the exact 32-bit BSTOP word. Compressed is illegal.
; OBJ-LABEL: <collective>:
; OBJ: 00001081 BSTART.AUX FALL
; OBJ-NEXT: {{.*}}0330a02b sync.core
; OBJ-NEXT: {{.*}}00000001 BSTOP
