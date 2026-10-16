; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t.direct.o
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=asm %s -o %t.s
; RUN: FileCheck %s --check-prefix=ASM < %t.s
; RUN: llvm-mc -triple=linx64v5 -mcpu=janus -filetype=obj %t.s -o %t.roundtrip.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.direct.o | FileCheck %s --check-prefix=OBJ
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.roundtrip.o | FileCheck %s --check-prefix=OBJ
; RUN: llvm-objcopy --dump-section=.text=%t.direct.text %t.direct.o
; RUN: llvm-objcopy --dump-section=.text=%t.roundtrip.text %t.roundtrip.o
; RUN: cmp %t.direct.text %t.roundtrip.text
; Raw low32 encodings exercise negative zero, infinities, qNaN and sNaN
; without numeric identity operations. Runtime bit/flag goldens belong to the
; matching model tests; this gate proves the compiler chooses masked TNEG.
target triple = "linx64v5"
declare <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64, i64, i64, i64, <32 x float>, i64)
define void @negative_zero_full() {
  %input = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2147483648, i64 1)
  %neg = call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %input, i64 4294967295)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP32\0AB.DATR NORM, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<32 x float> %neg)
  ret void
}
; ASM-LABEL: negative_zero_full:
; ASM: BSTART.TEPL TNEG, FP32
; ASM-NEXT: B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.TLSU TSTORE, FP32
; OBJ-LABEL: <negative_zero_full>:
; OBJ: BSTART.TEPL TNEG, FP32
; OBJ-NEXT: {{.*}}B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.TLSU TSTORE, FP32

define void @positive_zero_empty() {
  %input = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 0, i64 1)
  %neg = call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %input, i64 0)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP32\0AB.DATR NORM, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<32 x float> %neg)
  ret void
}
; ASM-LABEL: positive_zero_empty:
; ASM: BSTART.TEPL TNEG, FP32
; ASM-NEXT: B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.TLSU TSTORE, FP32
; OBJ-LABEL: <positive_zero_empty>:
; OBJ: BSTART.TEPL TNEG, FP32
; OBJ-NEXT: {{.*}}B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.TLSU TSTORE, FP32

define void @qnan_sparse() {
  %input = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2143363909, i64 1)
  %neg = call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %input, i64 1431655765)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP32\0AB.DATR NORM, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<32 x float> %neg)
  ret void
}
; ASM-LABEL: qnan_sparse:
; ASM: BSTART.TEPL TNEG, FP32
; ASM-NEXT: B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.TLSU TSTORE, FP32
; OBJ-LABEL: <qnan_sparse>:
; OBJ: BSTART.TEPL TNEG, FP32
; OBJ-NEXT: {{.*}}B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.TLSU TSTORE, FP32

define void @snan_sparse() {
  %input = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2139169605, i64 1)
  %neg = call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %input, i64 2863311530)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP32\0AB.DATR NORM, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<32 x float> %neg)
  ret void
}
; ASM-LABEL: snan_sparse:
; ASM: BSTART.TEPL TNEG, FP32
; ASM-NEXT: B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.TLSU TSTORE, FP32
; OBJ-LABEL: <snan_sparse>:
; OBJ: BSTART.TEPL TNEG, FP32
; OBJ-NEXT: {{.*}}B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.TLSU TSTORE, FP32

define void @positive_infinity_full() {
  %input = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 2139095040, i64 1)
  %neg = call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %input, i64 4294967295)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP32\0AB.DATR NORM, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<32 x float> %neg)
  ret void
}
; ASM-LABEL: positive_infinity_full:
; ASM: BSTART.TEPL TNEG, FP32
; ASM-NEXT: B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.TLSU TSTORE, FP32
; OBJ-LABEL: <positive_infinity_full>:
; OBJ: BSTART.TEPL TNEG, FP32
; OBJ-NEXT: {{.*}}B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.TLSU TSTORE, FP32

define void @negative_infinity_sparse() {
  %input = call <32 x float> asm sideeffect "BSTART.TEPL TEXPANDS, FP32\0AB.DATR CUBE_M32, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT mask=1111, last, ->$0<${2:Z}>\0AB.IOR [$1], []\0A", "=@2Tr,r,i"(i64 4286578688, i64 1)
  %neg = call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %input, i64 1431655765)
  call void asm sideeffect "BSTART.TLSU TSTORE, FP32\0AB.DATR NORM, DTYPE_NONE, Null\0AB.DIM zero, 32, ->lb1\0AB.IOT $0, mask=1111, last\0AB.IOR [zero,zero,zero], []\0A", "@2Tr,~{memory}"(<32 x float> %neg)
  ret void
}
; ASM-LABEL: negative_infinity_sparse:
; ASM: BSTART.TEPL TNEG, FP32
; ASM-NEXT: B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; ASM: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; ASM-NEXT: B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; ASM-NOT: B.IOR
; ASM: BSTART.TLSU TSTORE, FP32
; OBJ-LABEL: <negative_infinity_sparse>:
; OBJ: BSTART.TEPL TNEG, FP32
; OBJ-NEXT: {{.*}}B.DATR CUBE_M32, DTYPE_NONE, Null, byte0, Eq, RNONE, nosat, 0, 1
; OBJ: B.IOT {{.*}}, mask=1111, last, {{.*}}->{{[tmnu]}}<128B>
; OBJ-NEXT: {{.*}}B.IOR [{{[a-z0-9]+}},zero,zero], ExecMaskPresent
; OBJ-NOT: B.IOR
; OBJ: BSTART.TLSU TSTORE, FP32
