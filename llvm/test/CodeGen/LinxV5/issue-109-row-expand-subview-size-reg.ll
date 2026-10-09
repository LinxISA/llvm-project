; RUN: not llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true \
; RUN:     -linxv5-enable-clock-hand-opt=false %s -o /dev/null 2>&1 | FileCheck %s

target triple = "linx64v5"

; Issue #109: a TileOP region row-expand (TROWEXPAND* family with a
; region::SubTileView source) lowers to an inline-asm TEPL block whose
; destination size selector "->$0<${7:Z}>" binds the subview range-base GPR
; (operand $7, an "r" constraint) instead of an immediate TSize code.  The
; LinxV5 "TReg to Offset" pass parses the asm to size the tile def; it used to
; read that operand with MachineOperand::getImm() and abort codegen with
; "Wrong MachineOperand accessor" (an assertion build) / UNREACHABLE, at both
; -O0 and -O2.  The pass now records an unknown size for a non-immediate size
; operand, so it no longer crashes: codegen reaches the AsmPrinter, which emits
; its "0B" :Z sentinel, and the malformed selector is then reported as an
; ordinary instruction-match error rather than an internal compiler crash.

; The getImm() assertion / ICE must not fire ...
; CHECK-NOT: Wrong MachineOperand accessor
; CHECK-NOT: PLEASE submit a bug report
; ... instead the degraded selector reaches the "0B" sentinel and fails loudly.
; CHECK: error: Match Instruction Error!
; CHECK: ->{{[a-z_0-9]+}}<0B>

define void @row_expand_subview_size_reg(i64 %rangebase) {
  %d = tail call <512 x i32> asm sideeffect "BSTART.TEPL ${9:c}, ${1:D}\0AB.DIM zero, ${3:c}, ->lb0\0AB.DIM zero, ${4:c}, ->lb1\0AB.DIM zero, ${5:c}, ->lb2\0AB.IOT $2, $6, mask=1111, last, ->$0<${7:Z}>\0AB.SUBVIEW 0, $7, 0, ${8:c}\0A", "=@2Tr,i,@2Tr,i,i,i,@2Tr,r,i,i,i,~{memory}"(i32 5, <2048 x i32> undef, i32 32, i32 32, i32 32, <32 x i32> undef, i64 %rangebase, i32 5, i32 5, i32 75)
  ret void
}
