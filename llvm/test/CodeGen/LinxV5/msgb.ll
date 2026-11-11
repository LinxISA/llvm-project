; Offset operands name queue positions rather than independent physical registers.
; Verify before that final representation change, then check final assembly/object.
; RUN: llc -mtriple=linx64v5 -mcpu=janus -verify-machineinstrs -stop-before=linxv5-treg-to-offset %s -o /dev/null
; RUN: opt -O2 -S %s -o %t.ll
; RUN: llc -mtriple=linx64v5 -mcpu=janus %t.ll -o - | FileCheck %s
; RUN: opt -O3 -S %s -o %t.ll
; RUN: llc -mtriple=linx64v5 -mcpu=janus %t.ll -o - | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d %t.o | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"
declare i64 @llvm.linx.msgb.read(i64 immarg)
declare void @llvm.linx.msgb.write(i64 immarg, i64)
declare void @llvm.linx.msgb.send(i64)

; CHECK-LABEL: roundtrip:
; CHECK: ssrset a1, 2097
; CHECK: bse a0
; CHECK: ssrget 2119,
; CHECK: ssrget 2117,
; CHECK: ssrget 2118,
; CHECK: ssrset {{.*}}, 2107
; CHECK: ssrget 2108,
; CHECK: ssrget 2109,
; CHECK: ssrget 2097,
; CHECK: ssrset {{.*}}, 2114
; CHECK: ssrget 2115,
; CHECK: ssrget 2116,
; CHECK: ssrset {{.*}}, 2113
; OBJ-LABEL: <roundtrip>:
; OBJ: BSTART.AUX
; OBJ: ssrset
; OBJ: BSTART.AUX
; OBJ: bse
; OBJ: BSTART.AUX
; OBJ: ssrget

define i64 @roundtrip(i64 %control, i64 %word, ptr %output) {
  call void @llvm.linx.msgb.write(i64 2097, i64 %word)
  call void @llvm.linx.msgb.send(i64 %control)
  %tx = call i64 @llvm.linx.msgb.read(i64 2119)
  %hi = call i64 @llvm.linx.msgb.read(i64 2117)
  %lo = call i64 @llvm.linx.msgb.read(i64 2118)
  call void @llvm.linx.msgb.write(i64 2107, i64 1)
  %state = call i64 @llvm.linx.msgb.read(i64 2108)
  %token = call i64 @llvm.linx.msgb.read(i64 2109)
  %payload = call i64 @llvm.linx.msgb.read(i64 2097)
  store i64 %payload, ptr %output
  call void @llvm.linx.msgb.write(i64 2114, i64 %token)
  %ack = call i64 @llvm.linx.msgb.read(i64 2115)
  %echo = call i64 @llvm.linx.msgb.read(i64 2116)
  call void @llvm.linx.msgb.write(i64 2113, i64 %token)
  ret i64 %ack
}

; Even discarded reads change read_mask and must survive optimization.
; CHECK-LABEL: discarded:
; CHECK: ssrget 2097,
; CHECK: ssrget 2097,
; CHECK: bse
; CHECK: bse
define void @discarded(i64 %control) {
  %a = call i64 @llvm.linx.msgb.read(i64 2097)
  %b = call i64 @llvm.linx.msgb.read(i64 2097)
  call void @llvm.linx.msgb.send(i64 %control)
  call void @llvm.linx.msgb.send(i64 %control)
  ret void
}
