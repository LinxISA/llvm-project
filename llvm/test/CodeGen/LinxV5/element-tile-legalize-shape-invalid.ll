; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %s 2>&1 | FileCheck %s
; CHECK: Tile legalization: requires a unique predicated 32-element domain
; Logical predication still supports larger domains; this physical profile does
; not silently truncate or invent an arbitrary-width GPR carrier.
declare void @llvm.linx.experimental.element.region(metadata)
define void @wide_domain() {
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  ret void
}
!0 = distinct !{}
!1 = !{i32 129}
