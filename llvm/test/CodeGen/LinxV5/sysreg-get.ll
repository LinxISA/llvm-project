; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t
; RUN: llvm-objdump -d %t | FileCheck %s

declare i64 @llvm.linx.get.sysreg(i64 immarg)

define i64 @core_id() {
; CHECK-LABEL: <core_id>:
; CHECK:      0840{{[ \t]+}}C.BSTART.AUX
; CHECK-NEXT: 0210013b{{[ \t]+}}ssrget{{[ \t]+}}33,{{[ \t]+}}->a0
; CHECK-NEXT: {{[0-9a-f]+}}{{[ \t]+}}C.BSTART.STD
  %id = call i64 @llvm.linx.get.sysreg(i64 33)
  ret i64 %id
}

define void @read_and_store(ptr %destination) {
; CHECK-LABEL: <read_and_store>:
; CHECK:      0840{{[ \t]+}}C.BSTART.AUX
; CHECK-NEXT: {{[0-9a-f]+}}{{[ \t]+}}ssrget{{[ \t]+}}38,
; CHECK:      0840{{[ \t]+}}C.BSTART.AUX
; CHECK-NEXT: {{[0-9a-f]+}}{{[ \t]+}}hl.ssrget{{[ \t]+}}7938,
; CHECK-NEXT: {{[0-9a-f]+}}{{[ \t]+}}C.BSTART.STD
; CHECK-NEXT: {{[0-9a-f]+}}{{[ \t]+}}{{(c\.)?}}add{{[ \t]+}}t#2, t#1,
; CHECK-NEXT: {{[0-9a-f ]+}}{{[ \t]+}}{{sd(i\.intra)?}}{{[ \t]+}}
  %thread = call i64 @llvm.linx.get.sysreg(i64 38)
  %extended = call i64 @llvm.linx.get.sysreg(i64 7938)
  %sum = add i64 %thread, %extended
  store i64 %sum, ptr %destination
  ret void
}
