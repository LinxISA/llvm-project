; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -verify-machineinstrs -stop-after=finalize-isel %s -o - | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=obj %s -o %t.o
; Sub-Tile vectors must split into ordinary Private Scalar accesses without
; widening their memory range or changing request type to Tile.
; CHECK-LABEL: name: copy_i32
; CHECK-NOT: TLOAD
; CHECK-NOT: TSTORE
; CHECK-NOT: SCOPED
; CHECK: body:
; CHECK: {{LW|LD}}
; CHECK: {{SW|SD}}
; CHECK: PseudoRET
; CHECK-LABEL: name: copy_i64
; CHECK-NOT: TLOAD
; CHECK-NOT: TSTORE
; CHECK-NOT: SCOPED
; CHECK: body:
; CHECK: {{LD|LW}}
; CHECK: {{SD|SW}}
; CHECK: PseudoRET
; CHECK-LABEL: name: copy_float
; CHECK-NOT: TLOAD
; CHECK-NOT: TSTORE
; CHECK-NOT: SCOPED
; CHECK: body:
; CHECK: {{LW|LD}}
; CHECK: {{SW|SD}}
; CHECK: PseudoRET

define void @copy_i32(ptr %src, ptr %dst) {
  %v = load volatile <2 x i32>, ptr %src, align 4
  store volatile <2 x i32> %v, ptr %dst, align 4
  ret void
}
define void @copy_i64(ptr %src, ptr %dst) {
  %v = load volatile <4 x i64>, ptr %src, align 8
  store volatile <4 x i64> %v, ptr %dst, align 8
  ret void
}
define void @copy_float(ptr %src, ptr %dst) {
  %v = load volatile <2 x float>, ptr %src, align 4
  store volatile <2 x float> %v, ptr %dst, align 4
  ret void
}
