; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t.direct.o
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=asm %s -o %t.s
; RUN: FileCheck %s --check-prefix=ASM < %t.s
; RUN: llvm-mc -triple=linx64v5 -mcpu=janus -filetype=obj %t.s -o %t.roundtrip.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.direct.o | FileCheck %s --check-prefix=OBJ
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.roundtrip.o | FileCheck %s --check-prefix=OBJ
; RUN: llvm-objcopy --dump-section=.text=%t.direct.text %t.direct.o
; RUN: llvm-objcopy --dump-section=.text=%t.roundtrip.text %t.roundtrip.o
; RUN: cmp %t.direct.text %t.roundtrip.text
; Inputs use the native Tile register/size transport contract of the public
; API. This avoids the unrelated legacy TLOAD.NORM pseudo text spelling.
; Every predicate uses the same native command expansion for -S and object
; emission. TCMPS scalar +0 occupies source0 while its mask stays source1.
target triple = "linx64v5"
declare i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64, i64, i64, i64, <32 x float>, <32 x float>, i64, i64)
declare i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64, i64, i64, i64, <32 x float>, i64, i64, i64)

define i64 @tile_eq(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %bp, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 0, i64 %active)
  ret i64 %result
}
; ASM-LABEL: tile_eq:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMP, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <tile_eq>:
; OBJ: BSTART.TEPL TCMP, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @scalar_zero_eq(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 0, i64 0, i64 %active)
  ret i64 %result
}
; ASM-LABEL: scalar_zero_eq:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMPS, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <scalar_zero_eq>:
; OBJ: BSTART.TEPL TCMPS, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @tile_ne(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %bp, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 1, i64 %active)
  ret i64 %result
}
; ASM-LABEL: tile_ne:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMP, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, NE, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <tile_ne>:
; OBJ: BSTART.TEPL TCMP, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, NE, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @scalar_zero_ne(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 0, i64 1, i64 %active)
  ret i64 %result
}
; ASM-LABEL: scalar_zero_ne:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMPS, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, NE, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <scalar_zero_ne>:
; OBJ: BSTART.TEPL TCMPS, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, NE, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @tile_lt(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %bp, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 2, i64 %active)
  ret i64 %result
}
; ASM-LABEL: tile_lt:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMP, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, LT, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <tile_lt>:
; OBJ: BSTART.TEPL TCMP, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, LT, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @scalar_zero_lt(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 0, i64 2, i64 %active)
  ret i64 %result
}
; ASM-LABEL: scalar_zero_lt:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMPS, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, LT, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <scalar_zero_lt>:
; OBJ: BSTART.TEPL TCMPS, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, LT, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @tile_gt(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %bp, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 3, i64 %active)
  ret i64 %result
}
; ASM-LABEL: tile_gt:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMP, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, GT, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <tile_gt>:
; OBJ: BSTART.TEPL TCMP, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, GT, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @scalar_zero_gt(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 0, i64 3, i64 %active)
  ret i64 %result
}
; ASM-LABEL: scalar_zero_gt:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMPS, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, GT, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <scalar_zero_gt>:
; OBJ: BSTART.TEPL TCMPS, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, GT, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @tile_le(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %bp, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 4, i64 %active)
  ret i64 %result
}
; ASM-LABEL: tile_le:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMP, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, LE, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <tile_le>:
; OBJ: BSTART.TEPL TCMP, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, LE, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @scalar_zero_le(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 0, i64 4, i64 %active)
  ret i64 %result
}
; ASM-LABEL: scalar_zero_le:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMPS, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, LE, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <scalar_zero_le>:
; OBJ: BSTART.TEPL TCMPS, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, LE, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @tile_ge(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %b = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %bp, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 5, i64 %active)
  ret i64 %result
}
; ASM-LABEL: tile_ge:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMP, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, GE, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <tile_ge>:
; OBJ: BSTART.TEPL TCMP, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, GE, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

define i64 @scalar_zero_ge(ptr %ap, ptr %bp, i64 %active) {
entry:
  %a = call <32 x float> asm sideeffect "BSTART.TLSU TLOAD, FP32\0AB.DATR ND2M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${3:Z}>\0AB.IOR [$1,$2], []\0A", "=@2Tr,r,r,i,~{memory}"(ptr %ap, i64 4, i64 1)
  %result = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 0, i64 5, i64 %active)
  ret i64 %result
}
; ASM-LABEL: scalar_zero_ge:
; ASM-NOT: .GPR.MASKED
; ASM: BSTART.TEPL TCMPS, FP32
; ASM-NEXT: B.DATR NORM, DTYPE_NONE, Zero, byte0, GE, RNONE, nosat, 0, 1
; ASM: B.IOT
; ASM-NEXT: B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.STD RET
; OBJ-LABEL: <scalar_zero_ge>:
; OBJ: BSTART.TEPL TCMPS, FP32
; OBJ-NEXT: {{.*}}B.DATR NORM, DTYPE_NONE, Zero, byte0, GE, RNONE, nosat, 0, 1
; OBJ: B.IOT
; OBJ-NEXT: {{.*}}B.IOR [zero,{{[a-z0-9]+}},zero],[{{[a-z0-9]+}}], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.STD RET

