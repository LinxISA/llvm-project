; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

target triple = "linx64v5"

@value = global i64 0, align 8

; A sequential scalar body remains inside the preceding Tile block. The next
; Tile macro is the commit boundary, so no empty or fallthrough-only STD block
; is needed between the macro, scalar body, and following macro.
;
; CHECK-LABEL: <tile_scalar_fallthrough>:
; CHECK-NEXT: TEXPANDS{{ +}}<Row=32, Col=1, FP32>, a0, ->T<128B>
; CHECK-NEXT: addtpc
; CHECK-NEXT: ld
; CHECK-NEXT: TEXPANDS{{ +}}<Row=32, Col=1, FP32>, a0, ->T<128B>
; CHECK-NEXT: C.BSTART.STD{{[[:space:]]+}}RET
define i64 @tile_scalar_fallthrough() {
  call void asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->T<128B>\0AB.IOR [a0], []\0A", ""()
  %loaded = load volatile i64, ptr @value, align 8
  call void asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->T<128B>\0AB.IOR [a0], []\0A", ""()
  ret i64 %loaded
}

; A scalar branch target still begins with BSTART even when it is the layout
; successor. Conditional and direct transfer headers are never removed.
;
; CHECK-LABEL: <scalar_branch_target>:
; CHECK-NEXT: L.BSTART.STD{{[[:space:]]+}}COND
; CHECK: <.LBB{{[0-9]+}}_2>:
; CHECK-NEXT: C.BSTART.STD
define i64 @scalar_branch_target(i1 %condition) {
entry:
  br i1 %condition, label %taken, label %fallthrough

taken:
  %taken_value = load volatile i64, ptr @value, align 8
  br label %join

fallthrough:
  %fallthrough_value = load volatile i64, ptr @value, align 8
  br label %join

join:
  %result = phi i64 [ %taken_value, %taken ],
                    [ %fallthrough_value, %fallthrough ]
  ret i64 %result
}
