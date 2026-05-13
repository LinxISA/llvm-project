; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

define dso_local i64 @bxu1(i64 %a) nounwind {
; CHECK-LABEL: bxu1:
; DAG: bxu a0, 5, 10, ->a0
entry:
  %shifted = lshr i64 %a, 5
  %masked = and i64 %shifted, u0x3FF
  ret i64 %masked
}

define i32 @bxu2(i8 %a) {
; CHECK-LABEL: bxu2:
; DAG: bxu a0, 3, 5, ->a0
  %conv = zext i8 %a to i32
  %shr1 = lshr i32 %conv, 3
  ret i32 %shr1
}

define i64 @bxs1(i64 %a) {
; CHECK-LABEL: bxs1:
; DAG: bxs a0, 1, 53, ->a0
  %shl = shl i64 %a, 10
  %sra = ashr i64 %shl, 11
  ret i64 %sra
}

; generate symmetric 64-bit with BFI
define dso_local void @test_bfi1(i64* %arr) nounwind {
; CHECK-LABEL: test_bfi1
; CHECK: lui 4180, ->t
; CHECK-NEXT: addi t#1, 2333, ->t
; DAG: hl.bfi t#1, t#1, 4, 4, ->t
entry:
  store i64 u0x0105491D0105491D, i64* %arr
  ret void
}

define dso_local void @test_bfi2(i64* %arr) nounwind {
; CHECK-LABEL: test_bfi2
; DAG: lui 291, ->t
; DAG-NEXT: addi t#1, 1110, ->t
; DAG: hl.bfi t#1, t#1, 3, 3, ->t
entry:
  store i64 u0x0000123456123456, i64* %arr
  ret void
}

; should not generate BFI in these cases
define dso_local void @test_bfi3(i64* %arr) nounwind {
; CHECK-LABEL: test_bfi3
; CHECK: lui 74561
; CHECK-NEXT: addi t#1, 564
entry:
  store i64 u0x0000000012341234, i64* %arr
  ret void
}

define dso_local void @test_bfi4(i64* %arr) nounwind {
; CHECK-LABEL: test_bfi4
; CHECK-NOT: hl.bfi t#1, t#1, 3, 3, ->t
entry:
  store i64 u0x1111123456123456, i64* %arr
  ret void
}

; negative symmetric 64-bit long const
define dso_local void @test_bfi5(i64* %arr) nounwind {
; CHECK-LABEL: test_bfi5
; DAG: lui -16
; DAG-NEXT: hl.bfi t#1, t#1, 4, 4, ->t
entry:
  store i64 u0xFFFF0000FFFF0000, i64* %arr
  ret void
}

; negative general 64-bit long const
define dso_local void @test_bfi6(i64* %arr) nounwind {
; CHECK-LABEL: test_bfi6
; CHECK: addi zero, 291
; DAG: hl.bfi t#1, t#1, 3, 3, ->t
; DAG-NEXT: slli t#1, 24
; DAG-NEXT: addi t#1, 291
entry:
  store i64 u0x0123000123000123, i64* %arr
  ret void
}