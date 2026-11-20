; RUN: not llvm-as %s -disable-output 2>&1 | FileCheck %s

declare i64 @llvm.linx.get.sysreg(i64)

define i64 @invalid_address(i64 %address) {
; CHECK: immarg operand has non-immediate parameter
  %result = call i64 @llvm.linx.get.sysreg(i64 %address)
  ret i64 %result
}
