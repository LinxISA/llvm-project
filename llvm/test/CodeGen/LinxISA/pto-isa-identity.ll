; RUN: llc -mtriple=linx64 -filetype=obj %s -o %t
; RUN: llvm-readelf -S -x .note.pto.isa %t | FileCheck %s

define void @identity_smoke() {
  ret void
}

; CHECK: .note.pto.isa     NOTE
; CHECK-SAME: 0000b8
; CHECK-SAME: A
; CHECK-SAME: 4
; CHECK: 04000000 a5000000 01000000 50544f00
; CHECK: 7b22656e 636f6469 6e675f61 6269223a
; CHECK: 372e3122 7d000000
