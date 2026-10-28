; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true %s -o %t.s
; RUN: llvm-mc -triple=linx64v5 -mcpu=janus -filetype=obj %t.s -o %t.roundtrip.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.roundtrip.o | FileCheck %s

define void @predicate_atomic(ptr %base, ptr %vp, ptr %ip, ptr %mp) {
  %values = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DATR ND2M32, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 4, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []", "=@2Tr,r,r,i,~{memory}"(ptr %vp, i64 16, i64 3)
  %indices = call <256 x i32> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DATR ND2M32, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 4, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []", "=@2Tr,r,r,i,~{memory}"(ptr %ip, i64 32, i64 4)
  %mask = call <32 x i32> asm sideeffect "BSTART.TEPL TCMPS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Zero, GE, RNONE\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 4, ->lb2\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.IOR [zero], []", "=@2Tr,@2Tr,i"(<128 x i32> %values, i64 1)
  call void @llvm.linx.experimental.ew.mscatter.add.predicate.v128i32.v256i32.v32i32(i64 32, i64 4, i64 4, i64 25, i64 29, ptr %base, <128 x i32> %values, <256 x i32> %indices, <32 x i32> %mask, i64 0)
  call void @llvm.linx.experimental.ew.mscatter.add.predicate.v128i32.v256i32.v32i32(i64 32, i64 4, i64 4, i64 25, i64 29, ptr %base, <128 x i32> %values, <256 x i32> %indices, <32 x i32> %mask, i64 1)
  ret void
}

; CHECK-LABEL: <predicate_atomic>:
; CHECK: BSTART.TLSU MSCATTER.ADD, U32
; CHECK: B.DATR CUBE_M32.normal, Zero
; CHECK: B.IOT {{.*}}, {{.*}}, mask=1111
; CHECK-NEXT: {{.*}}B.IOT {{.*}}, mask=1111, last
; CHECK-NEXT: {{.*}}B.IOR {{.*}}, ExecMaskPresent
; CHECK: BSTART.TLSU MSCATTER.ADD, U32
; CHECK: B.DATR CUBE_M32, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 1, 0
; CHECK: B.IOR {{.*}}, ExecMaskPresent

declare void @llvm.linx.experimental.ew.mscatter.add.predicate.v128i32.v256i32.v32i32(i64, i64, i64, i64, i64, ptr, <128 x i32>, <256 x i32>, <32 x i32>, i64)
