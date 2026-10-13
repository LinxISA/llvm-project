; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/dense.ll 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/i16.ll 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/saturating.ll 2>&1 | FileCheck %s
;
; CHECK: GPR-masked TCVT requires exact logical32 S64/U64 to S32/U32 CUBE_M32 narrowing, RNONE, Sat=0, one low mask word and zero inactive lanes

;--- dense.ll
define void @dense_i64_is_not_an_element_leaf(ptr %source, ptr %output) {
  %input = load <64 x i64>, ptr %source
  %result = call <64 x i32>
      @llvm.linx.experimental.ew.tcvt.gpr.masked.v64i32.v64i64(
          i64 32, i64 2, i64 24, i64 25, i64 29, i64 0, i64 0,
          <64 x i64> %input, i64 -1, i64 0, i64 0, i64 1)
  store <64 x i32> %result, ptr %output
  ret void
}
declare <64 x i32> @llvm.linx.experimental.ew.tcvt.gpr.masked.v64i32.v64i64(
    i64, i64, i64, i64, i64, i64, i64, <64 x i64>, i64, i64, i64, i64)

;--- i16.ll
define void @i16_destination_is_not_admitted(ptr %source, ptr %output) {
  %input = load <32 x i64>, ptr %source
  %result = call <32 x i32>
      @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(
          i64 32, i64 1, i64 24, i64 26, i64 29, i64 0, i64 0,
          <32 x i64> %input, i64 4294967295, i64 0, i64 0, i64 1)
  store <32 x i32> %result, ptr %output
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(
    i64, i64, i64, i64, i64, i64, i64, <32 x i64>, i64, i64, i64, i64)

;--- saturating.ll
define void @saturating_narrow_is_not_this_stage(ptr %source, ptr %output) {
  %input = load <32 x i64>, ptr %source
  %result = call <32 x i32>
      @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(
          i64 32, i64 1, i64 16, i64 17, i64 29, i64 0, i64 1,
          <32 x i64> %input, i64 4294967295, i64 0, i64 0, i64 1)
  store <32 x i32> %result, ptr %output
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(
    i64, i64, i64, i64, i64, i64, i64, <32 x i64>, i64, i64, i64, i64)
