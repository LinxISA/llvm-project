; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %s 2>&1 | FileCheck %s --check-prefix=OWNER
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-verify' -disable-output %s 2>&1 | FileCheck %s --check-prefix=RESIDUAL
;
; The internal effect is only valid as output from the generic predicator. A
; hand-written or stranded call must never reach target legalization or codegen.
;
; OWNER: PTO element region: Tile legalization: atomic effect lacks region ownership
; RESIDUAL: PTO element region: required region lowering did not complete

target triple = "linx64v5"

declare <32 x i32>
    @llvm.linx.experimental.element.atomic.add.v32i32.v32p0(
        <32 x ptr>, <32 x i32>, <32 x i1>, i32)

define void @ownerless_internal_atomic(ptr %base) {
entry:
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison,
                         <32 x i32> zeroinitializer
  %addresses = getelementptr i32, <32 x ptr> %bases,
               <32 x i32> zeroinitializer
  %old = call <32 x i32>
      @llvm.linx.experimental.element.atomic.add.v32i32.v32p0(
          <32 x ptr> align 4 %addresses, <32 x i32> zeroinitializer,
          <32 x i1> <i1 true, i1 true, i1 true, i1 true,
                      i1 true, i1 true, i1 true, i1 true,
                      i1 true, i1 true, i1 true, i1 true,
                      i1 true, i1 true, i1 true, i1 true,
                      i1 true, i1 true, i1 true, i1 true,
                      i1 true, i1 true, i1 true, i1 true,
                      i1 true, i1 true, i1 true, i1 true,
                      i1 true, i1 true, i1 true, i1 true>, i32 32)
  ret void
}

