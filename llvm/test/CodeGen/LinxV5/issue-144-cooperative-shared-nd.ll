; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s

target triple = "linx64v5"

; CHECK-LABEL: <cooperative_shared_nd>:
; CHECK: B.DATR M322ND.normal, Zero
; CHECK-NEXT: B.IOS mask=1111, ->S0<16KB>
; CHECK-NEXT: B.ASSEMBLE 1, 0, {{[^,]+}}, 0, 6
; CHECK: B.DATR M322ND.normal, Zero
; CHECK-NEXT: B.IOS S0, mask=1111
; CHECK-NEXT: B.ASSEMBLE 0, 1, {{[^,]+}}, 0, 6
; CHECK: B.DATR M322ND.normal, Zero
; CHECK-NEXT: B.IOS S0, mask=1111
; CHECK-NEXT: B.ASSEMBLE 0, 0, {{[^,]+}}, 0, 6
define void @cooperative_shared_nd(i64 %thread) {
entry:
  %local = call <1024 x i32> asm sideeffect "BSTART.TEPL TEXPANDS, e4m3\0AB.DATR CUBE_M32, Null\0AB.DIM zero, 128, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${1:Z}>", "=@2Tr,i"(i64 6)
  %offset = shl i64 %thread, 5
  switch i64 %thread, label %middle [
    i64 0, label %init
    i64 3, label %last
  ]
init:
  %parent = call i64 asm sideeffect "BSTART.TMOV DTYPE_NONE\0AB.DATR M322ND, DTYPE_NONE, Zero\0AB.IOS mask=1111, ->${0:S}<16KB>\0AB.ASSEMBLE ${3:c}, ${4:c}, $2, 0, ${5:c}\0AB.IOT $1, mask=1111, last", "=@2Sr,@2Tr,r,i,i,i,~{memory}"(<1024 x i32> %local, i64 %offset, i64 1, i64 0, i64 6)
  br label %exit
last:
  call void asm sideeffect "BSTART.TMOV DTYPE_NONE\0AB.DATR M322ND, DTYPE_NONE, Zero\0AB.IOS ${0:S}, mask=1111\0AB.ASSEMBLE ${3:c}, ${4:c}, $2, 0, ${5:c}\0AB.IOT $1, mask=1111, last", "@2Sr,@2Tr,r,i,i,i,~{memory}"(i64 undef, <1024 x i32> %local, i64 %offset, i64 0, i64 1, i64 6)
  br label %exit
middle:
  call void asm sideeffect "BSTART.TMOV DTYPE_NONE\0AB.DATR M322ND, DTYPE_NONE, Zero\0AB.IOS ${0:S}, mask=1111\0AB.ASSEMBLE ${3:c}, ${4:c}, $2, 0, ${5:c}\0AB.IOT $1, mask=1111, last", "@2Sr,@2Tr,r,i,i,i,~{memory}"(i64 undef, <1024 x i32> %local, i64 %offset, i64 0, i64 0, i64 6)
  br label %exit
exit:
  ret void
}
