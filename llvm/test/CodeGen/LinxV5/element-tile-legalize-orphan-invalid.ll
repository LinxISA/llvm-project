; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %s 2>&1 | FileCheck %s
; CHECK: Tile legalization: orphan predicated region ownership
; Missing region ownership must fail before instruction selection.
define <32 x i1> @orphan(<32 x i1> %a, <32 x i1> %b) {
  %r = or <32 x i1> %a, %b, !linx.pto.element.vp.owner !0
  ret <32 x i1> %r
}
!0 = distinct !{}
