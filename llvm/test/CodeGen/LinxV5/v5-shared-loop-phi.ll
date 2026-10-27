; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -stop-after=linxv5-shared-handle-legalize %s -o - | FileCheck %s --check-prefix=MIR
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

target triple = "linx64v5"

; MIR-LABEL: name: shared_loop_phi
; MIR: :shared_abs = PHI
; CHECK-LABEL: <shared_loop_phi>:
; CHECK-NOT: movr{{.*}}S{{[0-9]+}}
; CHECK: B.IOS mask=1111, ->S0<512B>
; CHECK-NOT: movr
; CHECK: B.IOS S0, mask=1111
; CHECK-NOT: movr
; CHECK: B.IOS mask=1111, ->S0<512B>
; CHECK-NOT: movr
define void @shared_loop_phi(i64 %count) {
entry:
  %initial = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<512B>", "=@2Sr"()
  br label %loop
loop:
  %handle = phi i64 [ %initial, %entry ], [ %next_handle, %loop ]
  %index = phi i64 [ 0, %entry ], [ %next, %loop ]
  call void asm sideeffect "B.IOS ${0:S}, mask=1111", "@2Sr"(i64 %handle)
  %next_handle = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<512B>", "=@2Sr"()
  %next = add i64 %index, 1
  %more = icmp ult i64 %next, %count
  br i1 %more, label %loop, label %exit
exit:
  ret void
}
