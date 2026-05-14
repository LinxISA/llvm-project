; RUN: llc < %s -enable-all-vector-as-tilereg=true --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; CHECK: VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a0], ->t<512B>
; CHECK: VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a1], ->t<512B>
; CHECK: VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a2], ->t<512B>
; CHECK: TMATMUL.BIAS
; CHECK: TMATMUL.ACC
; CHECK: ACCCVT
; CHECK: VPAR  copyout, <M: 4, N: 4, K: 1, MR> t#1, [a3]
define dso_local void @tile_caller(ptr noundef %p1, ptr noundef %p2, ptr noundef %p3, ptr noundef %p4) local_unnamed_addr  {
entry:
  %0 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p1)
  %1 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p2)
  %2 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p3)
  %3 = tail call <64 x double> @llvm.linx.blk.matmul.ac.v64f64.v64f64.v64f64.v64f64(i64 4, i64 4, i64 1, i64 2, i64 2, <64 x double> %0, <64 x double> %1, <64 x double> %2)
  %4 = tail call <64 x double> @llvm.linx.blk.matmul.ac.v64f64.v64f64.v64f64.v64f64(i64 4, i64 4, i64 1, i64 2, i64 2, <64 x double> %0, <64 x double> %1, <64 x double> %3)
  %5 = tail call <64 x double> @llvm.linx.blk.acccvt.v64f64.v64f64(i64 4, i64 4, i64 1, i64 0, i64 0, i64 0, <64 x double> %4)
  tail call void (ptr, i64, i64, i64, <64 x double>, ...) @llvm.linx.vcall.par.0d1u.v64f64(ptr nonnull @copyout, i64 4, i64 4, i64 1, <64 x double> %5, ptr %p4)
  ret void
}

declare void @copyin(<64 x double> noundef, ptr noundef)

declare <64 x double> @llvm.linx.vcall.par.1d0u.v64f64(ptr, i64, i64, i64, ...)

declare <64 x double> @llvm.linx.blk.matmul.ac.v64f64.v64f64.v64f64.v64f64(i64, i64, i64, i64, i64, <64 x double>, <64 x double>, <64 x double>)

declare <64 x double> @llvm.linx.blk.acccvt.v64f64.v64f64(i64, i64, i64, i64, i64, i64, <64 x double>)

declare <64 x double> @llvm.linx.vcall.par.1d2u.v64f64(ptr, i64, i64, i64, <64 x double>, <64 x double>, ...)

declare void @copyout(<64 x double> noundef, ptr noundef)

declare void @llvm.linx.vcall.par.0d1u.v64f64(ptr, i64, i64, i64, <64 x double>, ...)

; CHECK-LABEL: tile_caller_matmulmx:
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a0], ->t<512B>
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a1], ->t<512B>
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a2], ->t<512B>
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a3], ->t<512B>
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a4], ->t<512B>
; CHECK:       TMATMULMX
; CHECK:       TMATMULMX.BIAS
; CHECK:       ACCCVT
; CHECK:       VPAR  copyout, <M: 4, N: 4, K: 1, MR> t#1, [a5]

; CHECK-LABEL: tile_caller_matmulmxb:
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a0], ->t<512B>
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a1], ->t<512B>
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a2], ->t<512B>
; CHECK:       VPAR  copyin, <M: 4, N: 4, K: 1, MR> [a3], ->t<512B>
; CHECK:       TMATMULMX
; CHECK:       TMATMULMX.BIAS
; CHECK:       ACCCVT
; CHECK:       VPAR  copyout, <M: 4, N: 4, K: 1, MR> t#1, [a4]

define dso_local void @tile_caller_matmulmx(ptr noundef %p1,
                                            ptr noundef %p2,
                                            ptr noundef %p3,
                                            ptr noundef %p4,
                                            ptr noundef %p5,
                                            ptr noundef %p6) local_unnamed_addr {
entry:
  %0 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p1)
  %1 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p2)
  %2 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p3)
  %3 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p4)
  %4 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p5)

  %5 = tail call <64 x double> @llvm.linx.blk.matmulmx.v64f64.v64f64.v64f64.v64f64.v64f64(
      i64 4, i64 4, i64 1, i64 2, i64 3,
      <64 x double> %0, <64 x double> %1, <64 x double> %2, <64 x double> %3)

  %6 = tail call <64 x double> @llvm.linx.blk.matmulmx.ac.v64f64.v64f64.v64f64.v64f64.v64f64.v64f64(
      i64 4, i64 4, i64 1, i64 2, i64 3,
      <64 x double> %0, <64 x double> %1, <64 x double> %2, <64 x double> %3, <64 x double> %4)

  %7 = tail call <64 x double> @llvm.linx.blk.acccvt.v64f64.v64f64(
      i64 4, i64 4, i64 1, i64 0, i64 0, i64 0, <64 x double> %6)

  tail call void (ptr, i64, i64, i64, <64 x double>, ...) @llvm.linx.vcall.par.0d1u.v64f64(
      ptr nonnull @copyout, i64 4, i64 4, i64 1, <64 x double> %7, ptr %p6)
  ret void
}

define dso_local void @tile_caller_matmulmxb(ptr noundef %p1,
                                             ptr noundef %p2,
                                             ptr noundef %p3,
                                             ptr noundef %p4,
                                             ptr noundef %p5) local_unnamed_addr {
entry:
  %0 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p1)
  %1 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p2)
  %2 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p3)
  %3 = tail call <64 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v64f64(
      ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p4)

  %4 = tail call <64 x double> @llvm.linx.blk.matmulmxb.v64f64.v64f64.v64f64.v64f64(
      i64 4, i64 4, i64 1, i64 2, i64 3,
      <64 x double> %0, <64 x double> %1, <64 x double> %2)

  %5 = tail call <64 x double> @llvm.linx.blk.matmulmxb.ac.v64f64.v64f64.v64f64.v64f64.v64f64(
      i64 4, i64 4, i64 1, i64 2, i64 3,
      <64 x double> %0, <64 x double> %1, <64 x double> %2, <64 x double> %3)

  %6 = tail call <64 x double> @llvm.linx.blk.acccvt.v64f64.v64f64(
      i64 4, i64 4, i64 1, i64 0, i64 0, i64 0, <64 x double> %5)

  tail call void (ptr, i64, i64, i64, <64 x double>, ...) @llvm.linx.vcall.par.0d1u.v64f64(
      ptr nonnull @copyout, i64 4, i64 4, i64 1, <64 x double> %6, ptr %p5)
  ret void
}

declare <64 x double> @llvm.linx.blk.matmulmx.v64f64.v64f64.v64f64.v64f64.v64f64(
    i64, i64, i64, i64, i64, <64 x double>, <64 x double>, <64 x double>, <64 x double>)

declare <64 x double> @llvm.linx.blk.matmulmx.ac.v64f64.v64f64.v64f64.v64f64.v64f64.v64f64(
    i64, i64, i64, i64, i64, <64 x double>, <64 x double>, <64 x double>, <64 x double>, <64 x double>)

declare <64 x double> @llvm.linx.blk.matmulmxb.v64f64.v64f64.v64f64.v64f64(
    i64, i64, i64, i64, i64, <64 x double>, <64 x double>, <64 x double>)

declare <64 x double> @llvm.linx.blk.matmulmxb.ac.v64f64.v64f64.v64f64.v64f64.v64f64(
    i64, i64, i64, i64, i64, <64 x double>, <64 x double>, <64 x double>, <64 x double>)
