; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=linx64 -O2 -filetype=obj -o - < %s | \
; RUN:   llvm-objdump -d - | FileCheck %s --check-prefix=OBJ

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.vpar.tadd(%linx.tile, %linx.tile, i32)
declare %linx.tile @llvm.linx.vpar.tsub(%linx.tile, %linx.tile, i32)

define void @vpar_tadd(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %sum = call %linx.tile @llvm.linx.vpar.tadd(%linx.tile %ta, %linx.tile %tb, i32 8)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %sum, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: vpar_tadd:
; CHECK:      BSTART.VPAR{{[[:space:]]+}}VS8
; CHECK-NEXT: B.TEXT{{[[:space:]]+}}.__linx_vtile_add_body.
; TLOAD A followed by TLOAD B makes B the newest T#1 and A the preceding T#2
; (docs/zh/isa/register/common/tilereg.md). Keep the source order explicit so
; non-commutative TSUB cannot pass through a commutative TADD false green.
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#2, t#1, last, ->t<4KB>
; CHECK-NEXT: C.B.DIMI
; CHECK:      .__linx_vtile_add_body.{{[0-9]+}}:
; CHECK:      v.add{{[[:space:]]+}}vt#1, vu#1.sw,{{[[:space:]]+}}->vt#2
; CHECK-NEXT: v.sw.brg.local{{[[:space:]]+}}vt#1, [to,
; OBJ-LABEL:  <vpar_tadd>:
; OBJ:        BSTART.VPAR{{[[:space:]]+}}VS8
; OBJ-NEXT:   B.TEXT
; OBJ-NEXT:   B.IOT{{[[:space:]]+}}t#2, t#1, last, ->t<4KB>
; OBJ-NEXT:   C.B.DIMI

define void @vpar_tsub(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %diff = call %linx.tile @llvm.linx.vpar.tsub(%linx.tile %ta, %linx.tile %tb, i32 8)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %diff, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: vpar_tsub:
; CHECK:      BSTART.VPAR{{[[:space:]]+}}VS8
; CHECK-NEXT: B.TEXT{{[[:space:]]+}}.__linx_vtile_sub_body.
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#2, t#1, last, ->t<4KB>
; CHECK-NEXT: C.B.DIMI

; The generated bodies retain queue-head destinations and consume VT#1 at
; their local stores. Header canonicalization must not rewrite body queues.
; CHECK:      .__linx_vtile_sub_body.{{[0-9]+}}:
; CHECK:      v.sub{{[[:space:]]+}}vt#1, vu#1.sw,{{[[:space:]]+}}->vt#2
; CHECK-NEXT: v.sw.brg.local{{[[:space:]]+}}vt#1, [to,
; OBJ-LABEL:  <vpar_tsub>:
; OBJ:        BSTART.VPAR{{[[:space:]]+}}VS8
; OBJ-NEXT:   B.TEXT
; OBJ-NEXT:   B.IOT{{[[:space:]]+}}t#2, t#1, last, ->t<4KB>
; OBJ-NEXT:   C.B.DIMI
