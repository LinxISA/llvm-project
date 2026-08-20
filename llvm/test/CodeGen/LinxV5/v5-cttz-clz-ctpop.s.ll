; RUN: llc -mtriple=linx64v5 -mcpu=janus -O2 %s -o - | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -O2 -filetype=obj %s -o %t.o && llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s --check-prefix=DIS

; Issue #62: Lower llvm.cttz/ctlz/ctpop to the ISA scalar ctz/clz/bcnt with
; the full XLEN-wide field (M=0, N=XLEN) instead of Expand->SWAR. The ISA
; instructions count within an N-bit field starting at M and return N for an
; all-zero field, which satisfies both the must-return-XLEN and the
; zero-undef forms of the intrinsic.

; CHECK-NOT: hl.bfi
; CHECK-NOT: lui
; CHECK-NOT: mul

define i64 @cttz_nonzero_undef(i64 %x) {
  ; CHECK-LABEL: cttz_nonzero_undef:
  ; CHECK: ctz a0, 0, 64, ->a0
  ; DIS: ctz{{.*}}0, 64{{.*}}
  %r = call i64 @llvm.cttz.i64(i64 %x, i1 false)
  ret i64 %r
}

define i64 @cttz_zero_undef(i64 %x) {
  ; CHECK-LABEL: cttz_zero_undef:
  ; CHECK: ctz a0, 0, 64, ->a0
  ; DIS: ctz{{.*}}0, 64{{.*}}
  %r = call i64 @llvm.cttz.i64(i64 %x, i1 true)
  ret i64 %r
}

define i64 @ctlz_nonzero_undef(i64 %x) {
  ; CHECK-LABEL: ctlz_nonzero_undef:
  ; CHECK: clz a0, 0, 64, ->a0
  ; DIS: clz{{.*}}0, 64{{.*}}
  %r = call i64 @llvm.ctlz.i64(i64 %x, i1 false)
  ret i64 %r
}

define i64 @ctlz_zero_undef(i64 %x) {
  ; CHECK-LABEL: ctlz_zero_undef:
  ; CHECK: clz a0, 0, 64, ->a0
  ; DIS: clz{{.*}}0, 64{{.*}}
  %r = call i64 @llvm.ctlz.i64(i64 %x, i1 true)
  ret i64 %r
}

define i64 @ctpop(i64 %x) {
  ; CHECK-LABEL: ctpop:
  ; CHECK: bcnt a0, 0, 64, ->a0
  ; DIS: bcnt{{.*}}0, 64{{.*}}
  %r = call i64 @llvm.ctpop.i64(i64 %x)
  ret i64 %r
}

declare i64 @llvm.cttz.i64(i64, i1 immarg)
declare i64 @llvm.ctlz.i64(i64, i1 immarg)
declare i64 @llvm.ctpop.i64(i64)