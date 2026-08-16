; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=linx32 -O2 < %s | FileCheck %s

; Public Tile API functions pass tile storage by reference.  The backend must
; move the 4 KiB payload through canonical TLSU blocks around tile inline asm,
; rather than attempting to select scalar loads or stores for linxtile values.

define void @tile_by_reference(ptr %dst, ptr %lhs, ptr %rhs) {
entry:
  %src0 = load <1024 x i32>, ptr %lhs, align 4096
  %src1 = load <1024 x i32>, ptr %rhs, align 4096
  %out = call <1024 x i32> asm sideeffect "BSTART.VEC TADD, U32\0AB.DATR Null\0AB.IOT $1, $2, mask=1111, last, ->${0:q}<4KB>", "=&r,r,r"(<1024 x i32> %src0, <1024 x i32> %src1)
  store <1024 x i32> %out, ptr %dst, align 4096
  ret void
}

; CHECK-LABEL: tile_by_reference:
; CHECK: BSTART.TLOAD
; CHECK: B.IOT {{.*}}->t<4KB>
; CHECK: BSTART.TLOAD
; CHECK: B.IOT {{.*}}->t<4KB>
; CHECK: BSTART.VEC TADD, U32
; CHECK: B.IOT t#{{[1-8]}}, t#{{[1-8]}}, mask=1111, last, ->t<4KB>
; CHECK: BSTART.TSTORE
; CHECK: B.IOT t#{{[1-8]}}, mask=1111, last
