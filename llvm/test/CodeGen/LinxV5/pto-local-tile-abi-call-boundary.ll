; RUN: split-file %s %t
; RUN: llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false -enable-misched=false \
; RUN:   %t/positive.ll -o - | FileCheck %s --check-prefix=POSITIVE
; RUN: not --crash llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false -enable-misched=false \
; RUN:   %t/wrong-descriptor.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=DESC
; RUN: not --crash llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false -enable-misched=false \
; RUN:   %t/extra-live.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=LIVE
; RUN: not --crash llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false -enable-misched=false \
; RUN:   %t/unmarked-inreg.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=MARKER
; RUN: not --crash llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false -enable-misched=false \
; RUN:   %t/missing-return-marker.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=CONTRACT
; RUN: not --crash llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-tile-clock-hand=false -enable-misched=false \
; RUN:   %t/malformed-carrier.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=MALFORMED
; RUN: not --crash llc -mtriple=linx64v5 \
; RUN:   -linxv5-enable-tile-clock-hand=false -enable-misched=false \
; RUN:   %t/split-carrier.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=SPLIT

; POSITIVE-LABEL: caller:
; POSITIVE-NOT: Folded Spill
; POSITIVE: BSTART.TLSU TLOAD, U32
; POSITIVE: BSTART.TLSU TMOV, U32
; POSITIVE: C.B.DIMI 128, ->lb0
; POSITIVE: C.B.DIMI 1, ->lb1
; POSITIVE: C.B.DIMI 128, ->lb2
; POSITIVE: B.IOT t#1.reuse, mask=1111, last, ->t<512B>
; POSITIVE: BSTART.STD CALL, helper128
; POSITIVE: BSTART.TLSU TSTORE, U32
; POSITIVE: BSTART.TLSU TLOAD, U32
; POSITIVE: BSTART.TLSU TMOV, U32
; POSITIVE: C.B.DIMI 32, ->lb0
; POSITIVE: C.B.DIMI 1, ->lb1
; POSITIVE: C.B.DIMI 128, ->lb2
; POSITIVE: B.IOT t#1.reuse, mask=1111, last, ->t<512B>
; POSITIVE: BSTART.STD CALL, helper32
; POSITIVE: BSTART.TLSU TSTORE, U32
; POSITIVE: BSTART.TLSU TLOAD, U32
; POSITIVE: BSTART.TLSU TMOV, U32
; POSITIVE: B.DIM zero, 2048, ->lb0
; POSITIVE: C.B.DIMI 1, ->lb1
; POSITIVE: B.DIM zero, 2048, ->lb2
; POSITIVE: B.IOT t#1.reuse, mask=1111, last, ->t<8KB>
; POSITIVE: BSTART.STD CALL, helper2048
; POSITIVE: BSTART.TLSU TSTORE, U32
; POSITIVE-LABEL: unused_result_caller:
; POSITIVE: BSTART.TLSU TLOAD, U32
; POSITIVE: BSTART.TLSU TMOV, U32
; POSITIVE: BSTART.STD CALL, helper128
; POSITIVE: BSTART.TLSU TLOAD, U32
; POSITIVE: BSTART.TLSU TSTORE, U32
; POSITIVE-LABEL: helper128:
; POSITIVE: BSTART.TLSU TMOV, U32
; POSITIVE: B.IOT t#1.reuse, mask=1111, last, ->t<512B>
; POSITIVE: BSTART.STD RET
; POSITIVE: c.setc.tgt
; POSITIVE-LABEL: helper32:
; POSITIVE: BSTART.TLSU TMOV, U32
; POSITIVE: C.B.DIMI 32, ->lb0
; POSITIVE: C.B.DIMI 128, ->lb2
; POSITIVE: BSTART.STD RET
; POSITIVE: c.setc.tgt
; POSITIVE-LABEL: helper2048:
; POSITIVE: BSTART.TLSU TMOV, U32
; POSITIVE: B.IOT t#1.reuse, mask=1111, last, ->t<8KB>
; POSITIVE: BSTART.STD RET
; POSITIVE: c.setc.tgt
; DESC: linx-local-tile-abi export source descriptor does not match
; LIVE: linx-local-tile-abi cannot spill a Local Tile payload
; MARKER: inreg Local Tile return requires linx-local-tile-abi
; CONTRACT: linx-local-tile-abi requires an explicit inreg return marker
; MALFORMED: linx-local-tile-abi type does not match its U32 NORM profile
; SPLIT: linx-local-tile-abi value was split or legalized

;--- positive.ll
@in128 = internal global [128 x i32] zeroinitializer, align 128
@out128 = internal global [128 x i32] zeroinitializer, align 128
@in2048 = internal global [2048 x i32] zeroinitializer, align 128
@out2048 = internal global [2048 x i32] zeroinitializer, align 128

define void @caller() {
entry:
  %a = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.DIM zero, ${6:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,i,i,i,~{memory}"(ptr @in128, i32 25, i32 3, i32 128, i32 1, i32 128)
  %ar = call inreg <128 x i32> @helper128(<128 x i32> %a)
  call void asm sideeffect "BSTART.TLSU TSTORE, ${2:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []\0A", "r,@2Tr,i,i,i,i,~{memory}"(ptr @out128, <128 x i32> %ar, i32 25, i32 128, i32 1, i32 128)
  %b = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.DIM zero, ${6:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,i,i,i,~{memory}"(ptr @in128, i32 25, i32 3, i32 32, i32 1, i32 128)
  %br = call inreg <128 x i32> @helper32(<128 x i32> %b)
  call void asm sideeffect "BSTART.TLSU TSTORE, ${2:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []\0A", "r,@2Tr,i,i,i,i,~{memory}"(ptr @out128, <128 x i32> %br, i32 25, i32 32, i32 1, i32 128)
  %c = call <2048 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.DIM zero, ${6:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,i,i,i,~{memory}"(ptr @in2048, i32 25, i32 7, i32 2048, i32 1, i32 2048)
  %cr = call inreg <2048 x i32> @helper2048(<2048 x i32> %c)
  call void asm sideeffect "BSTART.TLSU TSTORE, ${2:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []\0A", "r,@2Tr,i,i,i,i,~{memory}"(ptr @out2048, <2048 x i32> %cr, i32 25, i32 2048, i32 1, i32 2048)
  ret void
}

; A caller may need only the helper's scalar/GM side effects.  The Tile return
; is still exported by the callee, but an unused SSA result must not ask the
; instruction emitter to create an incompatible generic vector vreg.
define void @unused_result_caller() {
entry:
  %a = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.DIM zero, ${6:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,i,i,i,~{memory}"(ptr @in128, i32 25, i32 3, i32 128, i32 1, i32 128)
  %ignored = call inreg <128 x i32> @helper128(<128 x i32> %a)
  %b = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.DIM zero, ${6:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,i,i,i,~{memory}"(ptr @in128, i32 25, i32 3, i32 128, i32 1, i32 128)
  call void asm sideeffect "BSTART.TLSU TSTORE, ${2:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []\0A", "r,@2Tr,i,i,i,i,~{memory}"(ptr @out128, <128 x i32> %b, i32 25, i32 128, i32 1, i32 128)
  ret void
}

define internal inreg <128 x i32> @helper128(<128 x i32> %tile) #0 {
  %p = call <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32(i64 1, i64 128, i64 25, i64 0, <128 x i32> %tile, <128 x i32> %tile, i64 0, i64 128)
  %r = call <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v32i32.v128i32.v128i32(i64 1, i64 128, i64 25, i64 0, <32 x i32> %p, <128 x i32> %tile, <128 x i32> %tile, i64 128)
  ret <128 x i32> %r
}
define internal inreg <128 x i32> @helper32(<128 x i32> %tile) #1 {
  %p = call <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32(i64 1, i64 32, i64 25, i64 0, <128 x i32> %tile, <128 x i32> %tile, i64 0, i64 128)
  %r = call <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v32i32.v128i32.v128i32(i64 1, i64 32, i64 25, i64 0, <32 x i32> %p, <128 x i32> %tile, <128 x i32> %tile, i64 128)
  ret <128 x i32> %r
}
define internal inreg <2048 x i32> @helper2048(<2048 x i32> %tile) #2 {
  %p = call <512 x i32> @llvm.linx.experimental.ew.tcmp.row.v512i32.v2048i32.v2048i32(i64 1, i64 2048, i64 25, i64 0, <2048 x i32> %tile, <2048 x i32> %tile, i64 0, i64 2048)
  %r = call <2048 x i32> @llvm.linx.experimental.ew.tsel.row.v2048i32.v512i32.v2048i32.v2048i32(i64 1, i64 2048, i64 25, i64 0, <512 x i32> %p, <2048 x i32> %tile, <2048 x i32> %tile, i64 2048)
  ret <2048 x i32> %r
}
declare <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32(i64, i64, i64, i64, <128 x i32>, <128 x i32>, i64, i64)
declare <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v32i32.v128i32.v128i32(i64, i64, i64, i64, <32 x i32>, <128 x i32>, <128 x i32>, i64)
declare <512 x i32> @llvm.linx.experimental.ew.tcmp.row.v512i32.v2048i32.v2048i32(i64, i64, i64, i64, <2048 x i32>, <2048 x i32>, i64, i64)
declare <2048 x i32> @llvm.linx.experimental.ew.tsel.row.v2048i32.v512i32.v2048i32.v2048i32(i64, i64, i64, i64, <512 x i32>, <2048 x i32>, <2048 x i32>, i64)
attributes #0 = { "linx-local-tile-abi"="0:128:128" }
attributes #1 = { "linx-local-tile-abi"="0:128:32" }
attributes #2 = { "linx-local-tile-abi"="0:2048:2048" }

;--- unmarked-inreg.ll
@marker_out = internal global [128 x i32] zeroinitializer, align 128
declare inreg <128 x i32> @unmarked_return()
define void @unmarked_caller() {
  %result = call inreg <128 x i32> @unmarked_return()
  store <128 x i32> %result, ptr @marker_out, align 128
  ret void
}
;--- missing-return-marker.ll
define internal <128 x i32> @missing_return_marker(<128 x i32> %tile) #0 {
  ret <128 x i32> %tile
}
attributes #0 = { "linx-local-tile-abi"="0:128:128" }
;--- malformed-carrier.ll
define internal inreg <64 x i32> @malformed_carrier(<64 x i32> %tile) #0 {
  ret <64 x i32> %tile
}
attributes #0 = { "linx-local-tile-abi"="0:128:128" }
;--- split-carrier.ll
define internal inreg <128 x i32> @split_carrier(<128 x i32> %tile) #0 {
  ret <128 x i32> %tile
}
attributes #0 = { "linx-local-tile-abi"="0:128:128" }
;--- wrong-descriptor.ll
@bad_in = internal global [128 x i32] zeroinitializer, align 128
@bad_out = internal global [128 x i32] zeroinitializer, align 128
define void @bad_descriptor() {
  %a = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.DIM zero, ${6:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,i,i,i,~{memory}"(ptr @bad_in, i32 25, i32 3, i32 128, i32 1, i32 128)
  %r = call inreg <128 x i32> @bad_helper(<128 x i32> %a)
  call void asm sideeffect "BSTART.TLSU TSTORE, ${2:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []\0A", "r,@2Tr,i,i,i,i,~{memory}"(ptr @bad_out, <128 x i32> %r, i32 25, i32 32, i32 1, i32 128)
  ret void
}
define internal inreg <128 x i32> @bad_helper(<128 x i32> %tile) #0 {
  %p = call <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32(i64 1, i64 32, i64 25, i64 0, <128 x i32> %tile, <128 x i32> %tile, i64 0, i64 128)
  %r = call <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v32i32.v128i32.v128i32(i64 1, i64 32, i64 25, i64 0, <32 x i32> %p, <128 x i32> %tile, <128 x i32> %tile, i64 128)
  ret <128 x i32> %r
}
declare <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32(i64, i64, i64, i64, <128 x i32>, <128 x i32>, i64, i64)
declare <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v32i32.v128i32.v128i32(i64, i64, i64, i64, <32 x i32>, <128 x i32>, <128 x i32>, i64)
attributes #0 = { "linx-local-tile-abi"="0:128:32" }

;--- extra-live.ll
@live_in = internal global [128 x i32] zeroinitializer, align 128
@live_out = internal global [128 x i32] zeroinitializer, align 128
define void @extra_live() {
  %arg = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.DIM zero, ${6:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,i,i,i,~{memory}"(ptr @live_in, i32 25, i32 3, i32 128, i32 1, i32 128)
  %other = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, ${2:D}\0AB.DIM zero, ${4:c}, ->lb0\0AB.DIM zero, ${5:c}, ->lb1\0AB.DIM zero, ${6:c}, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i,i,i,i,i,~{memory}"(ptr @live_in, i32 25, i32 3, i32 128, i32 1, i32 128)
  %r = call inreg <128 x i32> @live_helper(<128 x i32> %arg)
  call void asm sideeffect "BSTART.TLSU TSTORE, ${2:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []\0A", "r,@2Tr,i,i,i,i,~{memory}"(ptr @live_out, <128 x i32> %other, i32 25, i32 128, i32 1, i32 128)
  call void asm sideeffect "BSTART.TLSU TSTORE, ${2:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []\0A", "r,@2Tr,i,i,i,i,~{memory}"(ptr @live_out, <128 x i32> %r, i32 25, i32 128, i32 1, i32 128)
  ret void
}
define internal inreg <128 x i32> @live_helper(<128 x i32> %tile) #0 {
  %p = call <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32(i64 1, i64 128, i64 25, i64 0, <128 x i32> %tile, <128 x i32> %tile, i64 0, i64 128)
  %r = call <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v32i32.v128i32.v128i32(i64 1, i64 128, i64 25, i64 0, <32 x i32> %p, <128 x i32> %tile, <128 x i32> %tile, i64 128)
  ret <128 x i32> %r
}
declare <32 x i32> @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32(i64, i64, i64, i64, <128 x i32>, <128 x i32>, i64, i64)
declare <128 x i32> @llvm.linx.experimental.ew.tsel.row.v128i32.v32i32.v128i32.v128i32(i64, i64, i64, i64, <32 x i32>, <128 x i32>, <128 x i32>, i64)
attributes #0 = { "linx-local-tile-abi"="0:128:128" }
