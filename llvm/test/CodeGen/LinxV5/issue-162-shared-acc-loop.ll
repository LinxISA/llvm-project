; RUN: llc -mtriple=linx64v5 -O2 -enable-all-vector-as-tilereg=true -o - %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -O2 -enable-all-vector-as-tilereg=true -linxv5-enable-generic-element=true -filetype=obj -o /dev/null %s
;
; The initial Shared matmul and the loop-carried ACC publish the same M32
; FP32 descriptor. Compile-time M/N/K travel through GPR constraints. PHI
; resolution must converge even when C's reaching definition is the ACC itself.
; CHECK-LABEL: shared_acc_loop:
; CHECK: BSTART.CUBE TMATMUL,
; CHECK: BSTART.CUBE TMATMUL.ACC,
;
target triple = "linx64v5-unknown-linux-musl"

define void @shared_acc_loop(ptr %pa, ptr %pb, ptr %out, i64 %count) {
entry:
  %a = call i64 asm sideeffect "BSTART.TLSU TLOAD, BF16\0AB.DIM zero, 64, ->lb0\0AB.DIM zero, 128, ->lb1\0AB.DIM zero, 64, ->lb2\0AB.IOS mask=1111, ->${0:S}<${2:Z}>\0AB.IOR [$1], []\0A", "=&@2Sr,r,i,~{memory}"(ptr %pa, i32 8)
  %b = call i64 asm sideeffect "BSTART.TLSU TLOAD, BF16\0AB.DIM zero, 64, ->lb0\0AB.DIM zero, 64, ->lb1\0AB.DIM zero, 64, ->lb2\0AB.IOS mask=1111, ->${0:S}<${2:Z}>\0AB.IOR [$1], []\0A", "=&@2Sr,r,i,~{memory}"(ptr %pb, i32 7)
  %init = call <2048 x float> asm sideeffect "BSTART.CUBE TMATMUL, BF16\0AB.DATR BF16, byte0, Zero, RNONE, NOSAT\0AB.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0\0AB.DIM $3, 0, ->lb0\0AB.DIM $4, 0, ->lb1\0AB.DIM $5, 0, ->lb2\0AB.IOS ${1:S}, mask=1111\0AB.IOS ${2:S}, mask=1111\0AB.IOT mask=1111, last, ->$0<${6:Z}>\0A", "=&@2Tr,@2Sr,@2Sr,r,r,r,i,~{memory}"(i64 %a, i64 %b, i64 128, i64 64, i64 64, i32 7)
  br label %loop
loop:
  %acc = phi <2048 x float> [ %init, %entry ], [ %next, %loop ]
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %next = call <2048 x float> asm sideeffect "BSTART.CUBE TMATMUL.ACC, BF16\0AB.DATR BF16, byte0, Zero, RNONE, NOSAT\0AB.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0\0AB.DIM $4, 0, ->lb0\0AB.DIM $5, 0, ->lb1\0AB.DIM $6, 0, ->lb2\0AB.IOS ${2:S}, mask=1111\0AB.IOS ${3:S}, mask=1111\0AB.IOT $1, mask=1111, last, ->$0<${7:Z}>\0A", "=&@2Tr,@2Tr,@2Sr,@2Sr,r,r,r,i,~{memory}"(<2048 x float> %acc, i64 %a, i64 %b, i64 128, i64 64, i64 64, i32 7)
  %inc = add i64 %i, 1
  %more = icmp ult i64 %inc, %count
  br i1 %more, label %loop, label %exit
exit:
  call void asm sideeffect "BSTART.TLSU TSTORE, FP32\0AB.DATR M322ND, Zero\0AB.DIM zero, 64, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 64, ->lb2\0AB.IOT $0, mask=1111, last\0AB.IOR [$1], []\0A", "@2Tr,r,~{memory}"(<2048 x float> %next, ptr %out)
  ret void
}
