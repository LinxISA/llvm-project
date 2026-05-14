; RUN: llc < %s -march=linx64v5 -O2 | FileCheck %s --dump-input always -vv

; COMMENT: int32 space:
; COMMENT: addi/subi presents [0xfffffffffffff001, 0xfff]. Test
; COMMENT:   0xfffffffffffff000(-4096), 0xfffffffffffff001(-4095),
; COMMENT:   0xfff(4095), 0x1000(4096)
; COMMENT: lui presents [0xffffffff80000, 0xfffffffffffff]000, [0x1, 0x7ffff]000. Test
; COMMENT:   0xffffffff7fffffff, 0xffffffff80000000, 0xfffffffffffff000, 0xfffffffffffff001,
; COMMENT:   0xfff(4095), 0x1000, 0x7ffff000, 0x7ffff001
; COMMENT: lui + addi(uint12) presents [0xffffffff80000001, 0xffffffffffffefff], [0x1001, 0x7fffffff]. Test
; COMMENT:   0xffffffff80000000, 0xffffffff80000001, 0xffffffffffffefff, 0xfffffffffffff000,
; COMMENT:   0x1000, 0x1001, 0x7fffffff, 0x80000000
; COMMENT: uint32 space:
; COMMENT: subi + ext.uw presents [0xfffff001, 0xffffffff]. Test
; COMMENT:   0xfffff000, 0xfffff001, 0xffffffff, 0x100000000
; COMMENT: lui + ext.uw presents [0x80000, 0xfffff]000. Test
; COMMENT:   0x7ffff000, 0x80000000, 0xfffff000, 0x100000000
; COMMENT: lui + addi + ext.uw presents [0x80000001, 0xffffffff]. Test
; COMMENT:   0x80000000, 0x80000001, 0xffffffff, 0x100000000
; COMMENT: int64 space, int64 is generated as (u)int32 followed by several (slli + addi/subi)s:
; COMMENT: Test 0x8000000000000000, 0x7fffffffffffffff

define i64 @func1() {
; CHECK-LABEL: func1:
; CHECK: lui 1, ->a0
  ret i64 4096 ; 0x1000
}

define i64 @func2() {
; CHECK-LABEL: func2:
; CHECK: lui 67817, ->t
; CHECK-NEXT: slli t#1, 4, ->t
; CHECK-NEXT: addi t#1, 1472, ->a0
  ret i64 4444456384
}

define i64 @func3() {
; CHECK-LABEL: func3:
; CHECK: lui -67817, ->t
; CHECK-NEXT: slli t#1, 4, ->t
; CHECK-NEXT: subi t#1, 1472, ->a0
  ret i64 -4444456384
}

define i64 @func4() {
; CHECK-LABEL: func4:
; CHECK: addi zero, 4095, ->a0
  ret i64 4095 ; 0xfff
}

define i64 @func5() {
; CHECK-LABEL: func5:
; CHECK: subi zero, 4095, ->a0
  ret i64 -4095 ; 0xfffffffffffff001
}

define i64 @func6() {
; CHECK-LABEL: func6:
; CHECK: lui -21555, ->t
; CHECK: addi t#1, 273, ->t
; CHECK: zero, t#1.uw, ->a0
  ret i64 4206678289
}

define i64 @func7() {
; CHECK-LABEL: func7:
; CHECK: lui -1, ->a0
  ret i64 -4096 ; 0xfffffffffffff000
}

define i64 @func8() {
; CHECK-LABEL: func8:
; CHECK: lui 524287, ->t
; CHECK-NEXT: addi t#1, 4095, ->a0
  ret i64 2147483647 ; INT_MAX, 0x7fffffff
}

define i64 @func9() {
; CHECK-LABEL: func9:
; CHECK: lui -2, ->t
; CHECK-NEXT: addi t#1, 1, ->a0
  ret i64 -8191 ; 0xffffffffffffe001
}

define i64 @func10() {
; CHECK-LABEL: func10:
; CHECK: lui -524288, ->t
; CHECK-NEXT: or zero, t#1.uw, ->a0
  ret i64 2147483648 ; 0x80000000
}

define i64 @func11() {
; CHECK-LABEL: func11:
; CHECK: movi 1, ->t
; CHECK-NEXT: slli t#1, 33, ->t
; CHECK-NEXT: subi t#1, 4095, ->a0
  ret i64 8589930497 ; 0x1fffff001
}

define i64 @func12() {
; CHECK-LABEL: func12:
; CHECK: lui -524288, ->t
; CHECK-NEXT: subi t#1, 1, ->a0
  ret i64 -2147483649 ; 0xffffffff7fffffff
}

define i64 @func13() {
; CHECK-LABEL: func13:
; CHECK: lui -524288, ->a0
  ret i64 -2147483648 ; 0xffffffff80000000
}

define i64 @func14() {
; CHECK-LABEL: func14:
; CHECK: lui 524287, ->a0
  ret i64 2147479552 ; 0x7ffff000
}

define i64 @func15() {
; CHECK-LABEL: func15:
; CHECK: lui 524287, ->t
; CHECK-NEXT: addi t#1, 1, ->a0
  ret i64 2147479553 ; 0x7ffff001
}

define i64 @func16() {
; CHECK-LABEL: func16:
; CHECK: lui -524288, ->t
; CHECK-NEXT: addi t#1, 1, ->a0
  ret i64 -2147483647 ; 0xffffffff80000001
}

define i64 @func17() {
; CHECK-LABEL: func17:
; CHECK: lui -2, ->t
; CHECK-NEXT: addi t#1, 4095, ->a0
  ret i64 -4097 ; 0xffffffffffffefff
}

define i64 @func18() {
; CHECK-LABEL: func18:
; CHECK: lui 1, ->t
; CHECK-NEXT: addi t#1, 1, ->a0
  ret i64 4097 ; 0x1001
}

define i64 @func19() {
; CHECK-LABEL: func19:
; CHECK: lui -1, ->t
; CHECK-NEXT: or zero, t#1.uw, ->a0
  ret i64 4294963200 ; 0xfffff000
}

define i64 @func20() {
; CHECK-LABEL: func20:
; CHECK: movi 1, ->t
; CHECK-NEXT: slli t#1, 32, ->a0
  ret i64 4294967296 ; 0x100000000
}

define i64 @func21() {
; CHECK-LABEL: func21:
; CHECK: lui -524288, ->t
; CHECK-NEXT: addi t#1, 1, ->t
; CHECK-NEXT: or zero, t#1.uw, ->a0
  ret i64 2147483649 ; 0x80000001
}

define i64 @func22() {
; CHECK-LABEL: func22:
; CHECK: movi -1, ->t
; CHECK-NEXT: or zero, t#1.uw, ->a0
  ret i64 4294967295 ; 0xffffffff
}

define i64 @func23() {
; CHECK-LABEL: func23:
; CHECK: subi zero, 4095, ->t
; CHECK-NEXT: or zero, t#1.uw, ->a0
  ret i64 4294963201 ; 0xfffff001
}

define i64 @func24() {
; CHECK-LABEL: func24:
; CHECK: movi -1, ->t
; CHECK-NEXT: slli t#1, 63, ->a0
  ret i64 -9223372036854775808 ; 0x8000000000000000
}

define i64 @func25() {
; CHECK-LABEL: func25:
; CHECK: movi -1, ->t
; CHECK-NEXT: slli t#1, 63, ->t
; CHECK-NEXT: subi t#1, 1, ->a0
  ret i64 9223372036854775807 ; 0x7fffffffffffffff
}

define i64 @func26() {
; CHECK-LABEL: func26:
; CHECK: lui 1810, ->t
; CHECK-NEXT: addi t#1, 837, ->t
; CHECK-NEXT: slli t#1, 13, ->t
; CHECK-NEXT: subi t#1, 3313, ->t
; CHECK-NEXT: slli t#1, 13, ->t
; CHECK-NEXT: addi t#1, 1711, ->t
; CHECK-NEXT: slli t#1, 14, ->t
; CHECK-NEXT: addi t#1, 3567, ->a0
  ret i64 8152435172137749999 ; 0x7123449879abcdef
}
