; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | not grep -E 'call .*element\.(region|view)|extractelement|insertelement'
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -disable-output %s
;
; Model the public TPARTELEMENT boundary after normal C++ optimization.  The
; carrier comes from a real Tile-register inline-asm result, element.view gives
; the compiler its dtype/layout identity, and the completed carrier is consumed
; by another Tile-register operation after the ordinary CFG loop.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @signed_nested_cfg() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %negative = icmp slt i32 %value, 0
  br i1 %negative, label %negative.arm, label %nonnegative

negative.arm:
  %quotient = sdiv i32 %value, 3
  %negative.insert = insertelement <32 x i32> %carrier, i32 %quotient,
                                    i32 %element
  br label %merge

nonnegative:
  %zero = icmp eq i32 %value, 0
  br i1 %zero, label %zero.arm, label %preserve.arm

zero.arm:
  %zero.insert = insertelement <32 x i32> %carrier, i32 7, i32 %element
  br label %merge

preserve.arm:
  br label %merge

merge:
  %updated = phi <32 x i32> [ %negative.insert, %negative.arm ],
                            [ %zero.insert, %zero.arm ],
                            [ %carrier, %preserve.arm ]
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %updated, i64 2, i64 2, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "BSTART.TEPL 32, $0", "@2Tr"(<32 x i32> %result)
  ret void
}

; CHECK-LABEL: define void @signed_nested_cfg()
; CHECK: [[SIGNED_INPUT:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: [[SIGNED_INITIAL:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32{{.*}}<32 x i32> [[SIGNED_INITIAL]]
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 0,
; CHECK: call void asm sideeffect "BSTART.TEPL 32, $0", "@2Tr"(<32 x i32>
; CHECK-NOT: llvm.linx.experimental.element.region
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement
; CHECK: ret void

; Clang may feed the header from the raw insertion while a matching typed view
; of that same final insertion is used only outside the loop.  The preheader
; seed establishes the accumulator identity; the external wrapper must retain
; it exactly.
define void @raw_backedge_typed_external() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %initial, i64 13, i64 13, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  call void @llvm.linx.experimental.element.region(metadata !21)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial.view, %entry ], [ %insert, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 14, i64 14, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %sum = add i32 %value, 9
  %insert = insertelement <32 x i32> %carrier, i32 %sum, i32 %element
  br label %latch

latch:
  %typed.final = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 13, i64 13, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !22

exit:
  %result = phi <32 x i32> [ %typed.final, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32> %result)
  ret void
}

; CHECK-LABEL: define void @raw_backedge_typed_external()
; CHECK: [[RAW_INPUT:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: [[RAW_INITIAL:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 0, <32 x i32> [[RAW_INPUT]],
; CHECK: call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32>
; CHECK-NOT: llvm.linx.experimental.element.region
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement
; CHECK: ret void

; A read-only typed import can outlive its first element region and feed a
; later region through the ordinary single-incoming LCSSA carrier.  Each region
; still owns a distinct output accumulator and publication.
define void @shared_readonly_import() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %first.initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %second.initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !15)
  br label %first.header

first.header:
  %first.element = phi i32 [ 0, %entry ], [ %first.next, %first.latch ]
  %first.carrier = phi <32 x i32> [ %first.initial, %entry ],
                                  [ %first.published, %first.latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 10, i64 10, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %first.value = extractelement <32 x i32> %input.view, i32 %first.element
  %first.sum = add i32 %first.value, 3
  %first.insert = insertelement <32 x i32> %first.carrier, i32 %first.sum,
                                 i32 %first.element
  br label %first.latch

first.latch:
  %first.published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %first.insert, i64 11, i64 11, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %first.next = add nuw nsw i32 %first.element, 1
  %first.more = icmp ult i32 %first.next, 32
  br i1 %first.more, label %first.header, label %between, !llvm.loop !16

between:
  %shared.input = phi <32 x i32> [ %input.view, %first.latch ]
  %first.result = phi <32 x i32> [ %first.published, %first.latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(
      <32 x i32> %first.result)
  call void @llvm.linx.experimental.element.region(metadata !18)
  br label %second.header

second.header:
  %second.element = phi i32 [ 0, %between ], [ %second.next, %second.latch ]
  %second.carrier = phi <32 x i32> [ %second.initial, %between ],
                                   [ %second.published, %second.latch ]
  %second.value = extractelement <32 x i32> %shared.input, i32 %second.element
  %second.mixed = xor i32 %second.value, 5
  %second.insert = insertelement <32 x i32> %second.carrier, i32 %second.mixed,
                                  i32 %second.element
  br label %second.latch

second.latch:
  %second.published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %second.insert, i64 12, i64 12, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %second.next = add nuw nsw i32 %second.element, 1
  %second.more = icmp ult i32 %second.next, 32
  br i1 %second.more, label %second.header, label %exit, !llvm.loop !19

exit:
  %second.result = phi <32 x i32> [ %second.published, %second.latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(
      <32 x i32> %second.result)
  ret void
}

; CHECK-LABEL: define void @shared_readonly_import()
; CHECK: [[SHARED_INPUT:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 0, <32 x i32> [[SHARED_INPUT]],
; CHECK: [[SHARED_LCSSA:%.*]] = phi <32 x i32> [ [[SHARED_INPUT]],
; CHECK: call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32>
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 7, <32 x i32> [[SHARED_LCSSA]],
; CHECK: call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32>
; CHECK-NOT: llvm.linx.experimental.element.region
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement
; CHECK: ret void

define void @unsigned_preserve_consecutive() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %first.header

first.header:
  %first.element = phi i32 [ 0, %entry ], [ %first.next, %first.latch ]
  %first.carrier = phi <32 x i32> [ %initial, %entry ],
                                  [ %first.published, %first.latch ]
  %first.input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 3, i64 3, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %first.value = extractelement <32 x i32> %first.input.view,
                                i32 %first.element
  %replace = icmp ult i32 %first.value, 16
  br i1 %replace, label %first.replace, label %first.preserve

first.replace:
  %incremented = add i32 %first.value, 11
  %first.insert = insertelement <32 x i32> %first.carrier, i32 %incremented,
                                 i32 %first.element
  br label %first.merge

first.preserve:
  br label %first.merge

first.merge:
  %first.updated = phi <32 x i32> [ %first.insert, %first.replace ],
                                  [ %first.carrier, %first.preserve ]
  br label %first.latch

first.latch:
  %first.published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %first.updated, i64 4, i64 4, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %first.next = add nuw nsw i32 %first.element, 1
  %first.more = icmp ult i32 %first.next, 32
  br i1 %first.more, label %first.header, label %between, !llvm.loop !4

between:
  %first.result = phi <32 x i32> [ %first.published, %first.latch ]
  call void @llvm.linx.experimental.element.region(metadata !6)
  br label %second.header

second.header:
  %second.element = phi i32 [ 0, %between ], [ %second.next, %second.latch ]
  %second.carrier = phi <32 x i32> [ %first.result, %between ],
                                   [ %second.published, %second.latch ]
  %second.input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %first.result, i64 4, i64 4, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %second.value = extractelement <32 x i32> %second.input.view,
                                 i32 %second.element
  %mixed = xor i32 %second.value, 5
  %second.insert = insertelement <32 x i32> %second.carrier, i32 %mixed,
                                  i32 %second.element
  br label %second.latch

second.latch:
  %second.published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %second.insert, i64 5, i64 5, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %second.next = add nuw nsw i32 %second.element, 1
  %second.more = icmp ult i32 %second.next, 32
  br i1 %second.more, label %second.header, label %exit, !llvm.loop !7

exit:
  %result = phi <32 x i32> [ %second.published, %second.latch ]
  call void asm sideeffect "BSTART.TEPL 32, $0", "@2Tr"(<32 x i32> %result)
  ret void
}

; CHECK-LABEL: define void @unsigned_preserve_consecutive()
; CHECK: [[UNSIGNED_INPUT:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: [[UNSIGNED_INITIAL:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32{{.*}}<32 x i32> [[UNSIGNED_INITIAL]]
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
; CHECK: call void asm sideeffect "BSTART.TEPL 32, $0", "@2Tr"(<32 x i32>
; CHECK-NOT: llvm.linx.experimental.element.region
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement
; CHECK: ret void

; Clang can place the typed publication in only the writing arm and feed the
; header accumulator from a carrier PHI that preserves the old value on the
; other arm.  The PHI itself is the latch incoming value; there is no redundant
; outer view call to recover the descriptor.
define void @conditional_publication_phi() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !9)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %next.carrier, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 6, i64 6, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %write = icmp ult i32 %value, 8
  br i1 %write, label %write.arm, label %preserve.arm

write.arm:
  %incremented = add i32 %value, 1
  %insert = insertelement <32 x i32> %carrier, i32 %incremented, i32 %element
  %view.updated = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 7, i64 7, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  br label %merge

preserve.arm:
  br label %merge

merge:
  %next.carrier = phi <32 x i32> [ %view.updated, %write.arm ],
                                  [ %carrier, %preserve.arm ]
  br label %latch

latch:
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !10

exit:
  %result = phi <32 x i32> [ %next.carrier, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32> %result)
  ret void
}

; CHECK-LABEL: define void @conditional_publication_phi()
; CHECK: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32
; CHECK: call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32>
; CHECK-NOT: llvm.linx.experimental.element.region
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement
; CHECK: ret void

; Narrow C++ scalar expressions retain their i8 modulo and signed-compare
; semantics even though the first physical Tile profile computes in i32.
define void @narrow_i8_control() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !12)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 8, i64 8, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %byte = trunc i32 %value to i8
  %remainder = urem i8 %byte, 7
  %negative = icmp slt i8 %byte, 0
  br i1 %negative, label %negative.arm, label %nonnegative.arm

negative.arm:
  %signed.word = sext i8 %byte to i16
  ; Keep the intermediate i16 observable after InstCombine.  For 0xff the
  ; signed word is already 0xffff, so setting bit 8 preserves the proof value.
  %adjusted.word = or i16 %signed.word, 256
  %signed = sext i16 %adjusted.word to i32
  %negative.insert = insertelement <32 x i32> %carrier, i32 %signed,
                                    i32 %element
  br label %merge

nonnegative.arm:
  %unsigned = zext i8 %remainder to i32
  %incremented = add i32 %unsigned, 1
  %nonnegative.insert = insertelement <32 x i32> %carrier, i32 %incremented,
                                       i32 %element
  br label %merge

merge:
  %updated = phi <32 x i32> [ %negative.insert, %negative.arm ],
                            [ %nonnegative.insert, %nonnegative.arm ]
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %updated, i64 9, i64 9, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !13

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32> %result)
  ret void
}

; CHECK-LABEL: define void @narrow_i8_control()
; i8 truncation and every i8 arithmetic result are reduced modulo 256.
; CHECK: [[BYTE:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 5,
; CHECK: [[SIGNED8_LEFT:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 8, <32 x i32> [[BYTE]],
; CHECK: [[SIGNED8:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 9, <32 x i32> [[SIGNED8_LEFT]],
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> [[SIGNED8]],
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 4,
; The i8-to-i16 result is normalized to 16 bits before the i16-to-i32
; sign-extension pair.  Thus input 0xff forms 0xffff before becoming -1.
; CHECK: [[WORD_LEFT:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 8, <32 x i32> [[BYTE]],
; CHECK: [[WORD_SIGNED:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 9, <32 x i32> [[WORD_LEFT]],
; CHECK: [[MASK16_SCALAR:%.*]] = freeze i32 65535
; CHECK: [[MASK16_WIDE:%.*]] = zext i32 [[MASK16_SCALAR]] to i64
; CHECK: [[MASK16_TILE:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32{{.*}}i64 [[MASK16_WIDE]],
; CHECK: [[WORD_NORMALIZED:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 5, <32 x i32> [[WORD_SIGNED]], <32 x i32> [[MASK16_TILE]],
; CHECK: [[WORD_ADJUSTED:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 6, <32 x i32> [[WORD_NORMALIZED]],
; CHECK: [[WORD_RENORMALIZED:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 5, <32 x i32> [[WORD_ADJUSTED]], <32 x i32> [[MASK16_TILE]],
; CHECK: [[SIGNED16_LEFT:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32{{.*}}i64 8, <32 x i32> [[WORD_RENORMALIZED]],
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 9, <32 x i32> [[SIGNED16_LEFT]],
; CHECK: call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32>
; CHECK-NOT: llvm.linx.experimental.element.region
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}

!6 = distinct !{}
!7 = distinct !{!7, !8}
!8 = !{!"llvm.loop.linx.pto.element.region", !6}

!9 = distinct !{}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.linx.pto.element.region", !9}

!12 = distinct !{}
!13 = distinct !{!13, !14}
!14 = !{!"llvm.loop.linx.pto.element.region", !12}

!15 = distinct !{}
!16 = distinct !{!16, !17}
!17 = !{!"llvm.loop.linx.pto.element.region", !15}

!18 = distinct !{}
!19 = distinct !{!19, !20}
!20 = !{!"llvm.loop.linx.pto.element.region", !18}

!21 = distinct !{}
!22 = distinct !{!22, !23}
!23 = !{!"llvm.loop.linx.pto.element.region", !21}
