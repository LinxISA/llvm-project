; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=asm %s -o - | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"

; A Shared handle defined by an inline-asm Sr output in one block and
; consumed by an Sr input in a different (loop) block lowers to a
; shared_abs COPY chain through the i64 SSA value. The RegisterCoalescer
; used to join the MixedGPR <-> Shared_ABS copy through the empty
; SIMT_OSVKR register class (an empty class is a subset of every class)
; and abort in MachineRegisterInfo::setRegClass (issue #99). The handle
; must stay live across the loop backedge and reuse the same S register.
;
; CHECK-LABEL: shared_crossblock:
; CHECK: B.IOS mask=1111, ->S0<128B>
; CHECK: B.IOS S0.reuse, mask=1111
; CHECK: B.IOS S0, mask=1111
; CHECK-NOT: B.IOS S1
;
; OBJ-LABEL: <shared_crossblock>:
; OBJ: B.IOS mask=1111, ->S0<128B>
; OBJ: B.IOS S0.reuse, mask=1111
; OBJ: B.IOS S0, mask=1111
; OBJ-NOT: B.IOS S1
define void @shared_crossblock(ptr %p, i64 %n) {
entry:
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  call void asm sideeffect "B.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  %next = add i64 %i, 1
  %cond = icmp slt i64 %next, %n
  br i1 %cond, label %loop, label %exit

exit:
  call void asm sideeffect "B.IOS ${0:K}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}
