; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tlea-float.ll 2>&1 | FileCheck %s --check-prefix=TLEA-FLOAT
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tlea-shape.ll 2>&1 | FileCheck %s --check-prefix=TLEA-SHAPE
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tci-float.ll 2>&1 | FileCheck %s --check-prefix=TCI-FLOAT
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tci-shape.ll 2>&1 | FileCheck %s --check-prefix=TCI-SHAPE
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tcmps-float.ll 2>&1 | FileCheck %s --check-prefix=TCMPS-FLOAT
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/tcmps-layout.ll 2>&1 | FileCheck %s --check-prefix=TCMPS-LAYOUT
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/atomic-shape.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC-SHAPE
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/atomic-dtype-layout.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC-DTYPE
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/atomic-wide-mask.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC-MASK
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/atomic-mask-high.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC-MASK

; TLEA-FLOAT: TLEA requires equal-lane i32/i64 source and i64 destination Tiles
; TLEA-SHAPE: TLEA requires equal-lane i32/i64 source and i64 destination Tiles
; TCI-FLOAT: TCI requires an exact-shape integer S32/U32/S64/U64 Tile
; TCI-SHAPE: TCI requires an exact-shape integer S32/U32/S64/U64 Tile
; TCMPS-FLOAT: TCMPS GPR requires a 32-row exact-shape integer CUBE_M32 Tile
; TCMPS-LAYOUT: TCMPS GPR requires a 32-row exact-shape integer CUBE_M32 Tile
; ATOMIC-SHAPE: masked MGATHER_ADD requires exact-shape U32 values, U64 offsets
; ATOMIC-DTYPE: masked MGATHER_ADD requires exact-shape U32 values, U64 offsets
; ATOMIC-MASK: masked MGATHER_ADD requires exact-shape U32 values, U64 offsets, CUBE_M32 and one low GPR mask word

;--- tlea-float.ll
define void @bad(ptr %out, ptr %in) {
  %indices = load <32 x float>, ptr %in
  %r = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32f32(
      i64 32, i64 1, i64 25, i64 29, <32 x float> %indices, i64 32)
  store <32 x i64> %r, ptr %out
  ret void
}
declare <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32f32(
    i64, i64, i64, i64, <32 x float>, i64)

;--- tlea-shape.ll
define void @bad(ptr %out, ptr %in) {
  %indices = load <32 x i32>, ptr %in
  %r = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
      i64 32, i64 2, i64 25, i64 29, <32 x i32> %indices, i64 32)
  store <32 x i64> %r, ptr %out
  ret void
}
declare <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(
    i64, i64, i64, i64, <32 x i32>, i64)

;--- tci-float.ll
define void @bad(ptr %out) {
  %r = call <32 x float> @llvm.linx.experimental.ew.tci.v32f32(
      i64 32, i64 1, i64 25, i64 29, i64 0, i64 4294967296)
  store <32 x float> %r, ptr %out
  ret void
}
declare <32 x float> @llvm.linx.experimental.ew.tci.v32f32(
    i64, i64, i64, i64, i64, i64)

;--- tci-shape.ll
define void @bad(ptr %out) {
  %r = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
      i64 32, i64 2, i64 25, i64 29, i64 0, i64 4294967296)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
    i64, i64, i64, i64, i64, i64)

;--- tcmps-float.ll
define i64 @bad(ptr %in) {
  %values = load <32 x float>, ptr %in
  %r = call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32f32(
      i64 32, i64 1, i64 25, i64 29, <32 x float> %values, i64 0, i64 0)
  ret i64 %r
}
declare i64 @llvm.linx.experimental.ew.tcmps.gpr.v32f32(
    i64, i64, i64, i64, <32 x float>, i64, i64)

;--- tcmps-layout.ll
define i64 @bad(ptr %in) {
  %values = load <32 x i32>, ptr %in
  %r = call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32(
      i64 32, i64 1, i64 25, i64 0, <32 x i32> %values, i64 0, i64 0)
  ret i64 %r
}
declare i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32(
    i64, i64, i64, i64, <32 x i32>, i64, i64)

;--- atomic-shape.ll
define void @bad(ptr %out, ptr %base, ptr %offset.ptr, ptr %value.ptr) {
  %offsets = load <16 x i64>, ptr %offset.ptr
  %values = load <32 x i32>, ptr %value.ptr
  %r = call <32 x i32>
      @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v16i64.v32i32(
          i64 32, i64 1, i64 25, i64 3, i64 29, ptr %base,
          <16 x i64> %offsets, <32 x i32> %values,
          i64 -1, i64 0, i64 0, i64 1)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32>
    @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v16i64.v32i32(
        i64, i64, i64, i64, i64, ptr, <16 x i64>, <32 x i32>,
        i64, i64, i64, i64)

;--- atomic-wide-mask.ll
define void @bad(ptr %out, ptr %base, ptr %offset.ptr, ptr %value.ptr) {
  %offsets = load <32 x i64>, ptr %offset.ptr
  %values = load <32 x i32>, ptr %value.ptr
  %r = call <32 x i32>
      @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(
          i64 32, i64 3, i64 25, i64 3, i64 29, ptr %base,
          <32 x i64> %offsets, <32 x i32> %values,
          i64 -1, i64 0, i64 0, i64 1)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32>
    @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(
        i64, i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>,
        i64, i64, i64, i64)

;--- atomic-mask-high.ll
define void @bad(ptr %out, ptr %base, ptr %offset.ptr, ptr %value.ptr) {
  %offsets = load <32 x i64>, ptr %offset.ptr
  %values = load <32 x i32>, ptr %value.ptr
  %r = call <32 x i32>
      @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(
          i64 32, i64 1, i64 25, i64 3, i64 29, ptr %base,
          <32 x i64> %offsets, <32 x i32> %values,
          i64 -1, i64 1, i64 0, i64 1)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32>
    @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(
        i64, i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>,
        i64, i64, i64, i64)

;--- atomic-dtype-layout.ll
define void @bad(ptr %out, ptr %base, ptr %offset.ptr, ptr %value.ptr) {
  %offsets = load <32 x i64>, ptr %offset.ptr
  %values = load <32 x i32>, ptr %value.ptr
  %r = call <32 x i32>
      @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(
          i64 32, i64 1, i64 17, i64 3, i64 0, ptr %base,
          <32 x i64> %offsets, <32 x i32> %values,
          i64 -1, i64 0, i64 0, i64 1)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32>
    @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(
        i64, i64, i64, i64, i64, ptr, <32 x i64>, <32 x i32>,
        i64, i64, i64, i64)
