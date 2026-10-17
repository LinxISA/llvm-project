; RUN: llc -mtriple=linx64 -O2 -filetype=obj < %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

define i64 @load_indexed(ptr %base, i64 %index) {
entry:
  %slot = getelementptr i64, ptr %base, i64 %index
  %value = load i64, ptr %slot, align 8
  ret i64 %value
}

define void @store_indexed(ptr %base, i64 %index, i64 %value) {
entry:
  %slot = getelementptr i64, ptr %base, i64 %index
  store i64 %value, ptr %slot, align 8
  ret void
}

; Address SrcRType raw 00 is the canonical no-modifier encoding. The final
; byte's low SrcRType bits are zero for both load and store.
; CHECK-LABEL: <load_indexed>:
; CHECK: 09 31 31 18{{[[:space:]]+}}ld{{[[:space:]]+}}[a0, a1<<3],{{[[:space:]]+}}->a0
; CHECK-LABEL: <store_indexed>:
; CHECK: 49 30 31 20{{[[:space:]]+}}sd{{[[:space:]]+}}a2, [a0, a1<<3]
