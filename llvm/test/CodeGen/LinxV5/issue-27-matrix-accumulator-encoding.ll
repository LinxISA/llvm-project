; RUN: llc -mtriple=linx64v5 -O2 -enable-all-vector-as-tilereg=true -linxv5-enable-tile-clock-hand=false -filetype=obj %s -o %t.off.o
; RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.off.o | FileCheck %s
; RUN: llc -mtriple=linx64v5 -O2 -enable-all-vector-as-tilereg=true -linxv5-enable-tile-clock-hand=true -filetype=obj %s -o %t.on.o
; RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.on.o | FileCheck %s
; CHECK-LABEL: <accumulator_encoding>:
; CHECK: BSTART.TMOV
; CHECK: ->u<4KB>
; CHECK: BSTART.TMATMUL.ACC
; CHECK: 050b5e13
; CHECK-SAME: B.IOT
; CHECK-SAME: u#1
; CHECK-SAME: ->t<4KB>

define void @accumulator_encoding() {
  %shared_a = call i64 asm sideeffect "", "=@2Sr"()
  %shared_b = call i64 asm sideeffect "", "=@2Sr"()
  %initial = call <1024 x i32> asm sideeffect "BSTART.CUBE TMATMUL, FP32\0AB.DATR FP32, byte0, Zero, RNONE, NOSAT\0AB.FPATR ${3:c}, 0, 0, 0, 0, 0, 0, 0, 0, 0\0AB.DIM zero, 64, ->lb0\0AB.DIM zero, 64, ->lb1\0AB.DIM zero, 64, ->lb2\0AB.IOS ${1:S}, mask=1111\0AB.IOS ${2:S}, mask=1111\0AB.IOT mask=1111, last, ->$0<${4:Z}>\0A", "=&@2Tr,@2Sr,@2Sr,i,i"(i64 %shared_a, i64 %shared_b, i32 0, i32 6)
  %result = call <1024 x i32> asm sideeffect "BSTART.CUBE TMATMUL.ACC, FP32\0AB.DATR FP32, byte0, Zero, RNONE, NOSAT\0AB.FPATR ${4:c}, 0, 0, 0, 0, 0, 0, 0, 0, 0\0AB.DIM zero, 64, ->lb0\0AB.DIM zero, 64, ->lb1\0AB.DIM zero, 64, ->lb2\0AB.IOS ${2:S}, mask=1111\0AB.IOS ${3:S}, mask=1111\0AB.IOT $1, mask=1111, last, ->$0<${5:Z}>\0A", "=&@2Tr,@2Tr,@2Sr,@2Sr,i,i"(<1024 x i32> %initial, i64 %shared_a, i64 %shared_b, i32 0, i32 6)
  call void asm sideeffect "", "@2Tr"(<1024 x i32> %result)
  ret void
}
