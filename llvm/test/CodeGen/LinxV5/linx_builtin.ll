; RUN: clang %s --target=linx64 -S -O2 -mllvm -linxv5-enable-legacy-isel=true  -mllvm -stop-after=finalize-isel -o - | FileCheck %s --check-prefixes=CHECK,VBX
; RUN: clang %s --target=linx64 -S -O2 -mllvm -linxv5-enable-legacy-isel=false -mllvm -stop-after=finalize-isel -o - | FileCheck %s --check-prefixes=CHECK,DAG

target triple = "linx64"

; CHECK-LABEL: name: builtin_test1
; VBX: VBXSYSGET 16
; DAG: SSR_GET 16

define dso_local signext i32 @builtin_test1() local_unnamed_addr {
  %1 = tail call i64 @llvm.linx.get.sysreg(i64 16)
  %2 = trunc i64 %1 to i32
  ret i32 %2
}

declare i64 @llvm.linx.get.sysreg(i64)
