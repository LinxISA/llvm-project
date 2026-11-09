; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true %s -o %t.s
; RUN: llvm-mc -triple=linx64v5 -filetype=obj %t.s -o %t.replay.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.replay.o | FileCheck %s

; RUN: llvm-objcopy --dump-section=.text=%t.direct.text %t.o
; RUN: llvm-objcopy --dump-section=.text=%t.replay.text %t.replay.o
; RUN: cmp %t.direct.text %t.replay.text

; RUN: %python %S/../../../utils/linxv5/verify_indexed_tlsu_range_stream.py llvm-mc llvm-objdump %t.o

; Use physical tile transports to isolate range lowering from legacy NORM macros.
define void @mgather_masked(ptr %out, ptr %base, ptr %ip, ptr %vp, ptr %mp, i64 %bits, i64 %min, i64 %max) {
  %ix = call <32 x i64> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %ip, i64 2)
  %v = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %vp, i64 1)
  %mask = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %mp, i64 1)
  %r = call <32 x i32> @llvm.linx.experimental.ew.mgather.masked.range.v32i32.v32i64.v32i32(i64 32, i64 1, i64 25, i64 0, ptr %base, <32 x i64> %ix, <32 x i32> %mask, i64 0, i64 0)
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []", "r,@2Tr,~{memory}"(ptr %out, <32 x i32> %r)
  ret void
}
; CHECK-LABEL: <mgather_masked>:
; CHECK: BSTART.MGATHER
; CHECK: B.IOR {{.*}}[{{[^]]+,[^]]+}}], []
; CHECK-NOT: ExecMaskPresent

define void @mgather_gpr_masked(ptr %out, ptr %base, ptr %ip, ptr %vp, ptr %mp, i64 %bits, i64 %min, i64 %max) {
  %ix = call <32 x i64> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %ip, i64 2)
  %v = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %vp, i64 1)
  %mask = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %mp, i64 1)
  %r = call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.range.v32i32.v32i64(i64 32, i64 1, i64 25, i64 0, i64 29, i64 24, ptr %base, <32 x i64> %ix, i64 %bits, i64 0, i64 0, i64 1, i64 %min, i64 %max)
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []", "r,@2Tr,~{memory}"(ptr %out, <32 x i32> %r)
  ret void
}
; CHECK-LABEL: <mgather_gpr_masked>:
; CHECK: BSTART.MGATHER
; CHECK: B.IOR {{.*}}[{{[^]]+,[^]]+}}], []
; CHECK-NEXT: {{.*}}B.IOR {{.*}}ExecMaskPresent

define void @mscatter_masked(ptr %out, ptr %base, ptr %ip, ptr %vp, ptr %mp, i64 %bits, i64 %min, i64 %max) {
  %ix = call <32 x i64> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %ip, i64 2)
  %v = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %vp, i64 1)
  %mask = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %mp, i64 1)
  call void @llvm.linx.experimental.ew.mscatter.masked.range.v32i32.v32i64.v32i32(i64 32, i64 1, i64 25, ptr %base, <32 x i32> %v, <32 x i64> %ix, <32 x i32> %mask, i64 %min, i64 %max)
  ret void
}
; CHECK-LABEL: <mscatter_masked>:
; CHECK: BSTART.MSCATTER
; CHECK: B.IOR {{.*}}[{{[^]]+,[^]]+}}], []
; CHECK-NOT: ExecMaskPresent

define void @mscatter_gpr_masked(ptr %out, ptr %base, ptr %ip, ptr %vp, ptr %mp, i64 %bits, i64 %min, i64 %max) {
  %ix = call <32 x i64> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %ip, i64 2)
  %v = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %vp, i64 1)
  %mask = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %mp, i64 1)
  call void @llvm.linx.experimental.ew.mscatter.gpr.masked.range.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, i64 24, ptr %base, <32 x i64> %ix, <32 x i32> %v, i64 %bits, i64 0, i64 0, i64 0, i64 %min, i64 %max)
  ret void
}
; CHECK-LABEL: <mscatter_gpr_masked>:
; CHECK: BSTART.MSCATTER
; CHECK: B.IOR {{.*}}[{{[^]]+,[^]]+}}], []
; CHECK-NEXT: {{.*}}B.IOR {{.*}}ExecMaskPresent

define void @mgather_add(ptr %out, ptr %base, ptr %ip, ptr %vp, ptr %mp, i64 %bits, i64 %min, i64 %max) {
  %ix = call <32 x i64> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %ip, i64 2)
  %v = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %vp, i64 1)
  %mask = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %mp, i64 1)
  %r = call <32 x i32> @llvm.linx.experimental.ew.mgather.add.range.v32i32.v32i64.v32i32(i64 32, i64 1, i64 25, i64 0, ptr %base, <32 x i64> %ix, <32 x i32> %v, i64 %min, i64 %max)
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []", "r,@2Tr,~{memory}"(ptr %out, <32 x i32> %r)
  ret void
}
; CHECK-LABEL: <mgather_add>:
; CHECK: BSTART.MGATHER
; CHECK: B.IOR {{.*}}[{{[^]]+,[^]]+}}], []
; CHECK-NOT: ExecMaskPresent

define void @mgather_add_masked(ptr %out, ptr %base, ptr %ip, ptr %vp, ptr %mp, i64 %bits, i64 %min, i64 %max) {
  %ix = call <32 x i64> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %ip, i64 2)
  %v = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %vp, i64 1)
  %mask = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %mp, i64 1)
  %r = call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked.range.v32i32.v32i64.v32i32(i64 32, i64 1, i64 25, i64 0, i64 29, ptr %base, <32 x i64> %ix, <32 x i32> %v, i64 %bits, i64 0, i64 0, i64 0, i64 %min, i64 %max)
  call void asm sideeffect "BSTART.TLSU TSTORE, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT $1, mask=1111, last\0AB.IOR [$0], []", "r,@2Tr,~{memory}"(ptr %out, <32 x i32> %r)
  ret void
}
; CHECK-LABEL: <mgather_add_masked>:
; CHECK: BSTART.MGATHER
; CHECK: B.IOR {{.*}}[{{[^]]+,[^]]+}}], []
; CHECK-NEXT: {{.*}}B.IOR {{.*}}ExecMaskPresent

define void @mscatter_add(ptr %out, ptr %base, ptr %ip, ptr %vp, ptr %mp, i64 %bits, i64 %min, i64 %max) {
  %ix = call <32 x i64> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %ip, i64 2)
  %v = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %vp, i64 1)
  %mask = call <32 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DIM zero, 32, ->lb0\0AB.DIM zero, 1, ->lb1\0AB.DIM zero, 32, ->lb2\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []", "=@2Tr,r,i,~{memory}"(ptr %mp, i64 1)
  call void @llvm.linx.experimental.ew.mscatter.add.range.v32i32.v32i64(i64 32, i64 1, i64 25, ptr %base, <32 x i32> %v, <32 x i64> %ix, i64 %min, i64 %max)
  ret void
}
; CHECK-LABEL: <mscatter_add>:
; CHECK: BSTART.MSCATTER
; CHECK: B.IOR {{.*}}[{{[^]]+,[^]]+}}], []
; CHECK-NOT: ExecMaskPresent

declare <32 x i32> @llvm.linx.experimental.ew.mgather.masked.range.v32i32.v32i64.v32i32(i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>, i64, i64)
declare <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.range.v32i32.v32i64(i64, i64, i64, i64, i64, i64, ptr, <32 x i64>, i64, i64, i64, i64, i64, i64)
declare void @llvm.linx.experimental.ew.mscatter.masked.range.v32i32.v32i64.v32i32(i64, i64, i64, ptr, <32 x i32>, <32 x i64>, <32 x i32>, i64, i64)
declare void @llvm.linx.experimental.ew.mscatter.gpr.masked.range.v32i64.v32i32(i64, i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>, i64, i64, i64, i64, i64, i64)
declare <32 x i32> @llvm.linx.experimental.ew.mgather.add.range.v32i32.v32i64.v32i32(i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>, i64, i64)
declare <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked.range.v32i32.v32i64.v32i32(i64, i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>, i64, i64, i64, i64, i64, i64)
declare void @llvm.linx.experimental.ew.mscatter.add.range.v32i32.v32i64(i64, i64, i64, ptr, <32 x i32>, <32 x i64>, i64, i64)

define void @predicate_range(ptr %base, ptr %vp, ptr %ip, ptr %mp) {
  %values = call <128 x i32> asm sideeffect "BSTART.TLSU TLOAD, U32\0AB.DATR ND2M32, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 4, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []", "=@2Tr,r,r,i,~{memory}"(ptr %vp, i64 16, i64 3)
  %indices = call <256 x i32> asm sideeffect "BSTART.TLSU TLOAD, U64\0AB.DATR ND2M32, Zero\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 4, ->lb2\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []", "=@2Tr,r,r,i,~{memory}"(ptr %ip, i64 32, i64 4)
  %mask = call <32 x i32> asm sideeffect "BSTART.TEPL TCMPS, U32\0AB.DATR CUBE_M32, DTYPE_NONE, Zero, GE, RNONE\0AB.DIM zero, 4, ->lb0\0AB.DIM zero, 32, ->lb1\0AB.DIM zero, 4, ->lb2\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>\0AB.IOR [zero], []", "=@2Tr,@2Tr,i"(<128 x i32> %values, i64 1)
  call void @llvm.linx.experimental.ew.mscatter.add.predicate.range.v128i32.v256i32.v32i32(i64 32, i64 4, i64 4, i64 25, i64 29, ptr %base, <128 x i32> %values, <256 x i32> %indices, <32 x i32> %mask, i64 0, i64 -8, i64 1099511627776)
  call void @llvm.linx.experimental.ew.mscatter.add.predicate.range.v128i32.v256i32.v32i32(i64 32, i64 4, i64 4, i64 25, i64 29, ptr %base, <128 x i32> %values, <256 x i32> %indices, <32 x i32> %mask, i64 1, i64 -8, i64 1099511627776)
  ret void
}


; CHECK-LABEL: <predicate_range>:
; CHECK: BSTART.MSCATTER.ADD
; CHECK: B.IOR {{.*}}[{{[^]]+,[^]]+}}], []
; CHECK-NOT: ExecMaskPresent
declare void @llvm.linx.experimental.ew.mscatter.add.predicate.range.v128i32.v256i32.v32i32(i64, i64, i64, i64, i64, ptr, <128 x i32>, <256 x i32>, <32 x i32>, i64, i64, i64)
