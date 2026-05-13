; RUN: llc < %s -enable-all-vector-as-tilereg=true -mcpu=janus --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK
; RUN: llc < %s -enable-all-vector-as-tilereg=true -mcpu=janus --march=linx64v5 -O2 -stop-after=prologepilog | FileCheck %s --dump-input always -vv --check-prefixes=CHECK-STACK

; ModuleID = 'test.cpp'
source_filename = "test.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; CHECK-LABEL: _Z11tile_callerPdS_S_:
; CHECK: lui	18, 	->t
; CHECK: addi	t#1, 256, 	->t
; CHECK: add	sp, t#1, 	->a0
; CHECK: TSTORE.NORM <LB0: 1024, LB1: 1, LB2: 1024, S64>, t#1, [a0]
; CHECK: lui	18, 	->t
; CHECK: addi	t#1, 256, 	->t
; CHECK: add	sp, t#1, 	->a0
; CHECK: TLOAD.NORM	<LB0: 1024, LB1: 1, LB2: 1024, S64, Zero>	[a0], 	->t<8KB>

; During the PEI phase, the object size of FrameIndex has been corrected.
; CHECK-STACK:  - { id: 17, name: '', type: spill-slot, offset: -216, size: 8, alignment: 8,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 18, name: '', type: spill-slot, offset: -8448, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 19, name: '', type: spill-slot, offset: -16640, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 20, name: '', type: spill-slot, offset: -24832, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 21, name: '', type: spill-slot, offset: -33024, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 22, name: '', type: spill-slot, offset: -41216, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 23, name: '', type: spill-slot, offset: -49408, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 24, name: '', type: spill-slot, offset: -57600, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 25, name: '', type: spill-slot, offset: -65792, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 26, name: '', type: spill-slot, offset: -73984, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }
; CHECK-STACK:  - { id: 27, name: '', type: spill-slot, offset: -82176, size: 8192, alignment: 256,
; CHECK-STACK:      stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-STACK:      debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }

; Function Attrs: mustprogress nounwind
define dso_local void @_Z11tile_callerPdS_S_(ptr noundef %p1, ptr noundef %p2, ptr noundef %p3, ptr noundef %p4, ptr noundef %p5,
                                             ptr noundef %p6, ptr noundef %p7, ptr noundef %p8,ptr noundef %p9, ptr noundef %p10,
                                             ptr noundef %p11, ptr noundef %p12, ptr noundef %p13, ptr noundef %p14, ptr noundef %p15,
                                             ptr noundef %p16, ptr noundef %p17, ptr noundef %p18, ptr noundef %p19, ptr noundef %p20,
                                             ptr noundef %p21, ptr noundef %p22, ptr noundef %p23, ptr noundef %p24, ptr noundef %p25,
                                             ptr noundef %p26, ptr noundef %p27, ptr noundef %p28, ptr noundef %p29, ptr noundef %p30,
                                             ptr noundef %p31, ptr noundef %p32, ptr noundef %p33, ptr noundef %p34, ptr noundef %p35,
                                             ptr noundef %p36, ptr noundef %p37, ptr noundef %p38) local_unnamed_addr #0 {
entry:
  %0 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %1 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %2 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p3)
  %3 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p4)
  %4 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p5)
  %5 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p6)
  %6 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p7)
  %7 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p8)
  %8 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p9)
  %9 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p10)
  %10 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p11)
  %11 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p12)
  %12 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p13)
  %13 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p14)
  %14 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p15)
  %15 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p16)
  %16 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p17)
  %17 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p18)
  %18 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p19)
  %19 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p20)
  %20 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p21)
  %21 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p22)
  %22 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p23)
  %23 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p24)
  %24 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p25)
  %25 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p26)
  %26 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p27)
  %27 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p28)
  %28 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p29)
  %29 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p30)
  %30 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p31)
  %31 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p32)
  %32 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p33)
  %33 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p34)
  %34 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p35)
  %35 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p36)
  %36 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p37)
  %37 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p38)
  %38 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)

  %39 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %1,  <1024 x double> %38)
  %40 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %2,  <1024 x double> %39)
  %41 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %3,  <1024 x double> %40)
  %42 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %4,  <1024 x double> %41)
  %43 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %5,  <1024 x double> %42)
  %44 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %6,  <1024 x double> %43)
  %45 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %7,  <1024 x double> %44)
  %46 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %8,  <1024 x double> %45)
  %47 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %9,  <1024 x double> %46)
  %48 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %10, <1024 x double> %47)
  %49 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %11, <1024 x double> %48)
  %50 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %12, <1024 x double> %49)
  %51 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %13, <1024 x double> %50)
  %52 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %14, <1024 x double> %51)
  %53 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %15, <1024 x double> %52)
  %54 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %16, <1024 x double> %53)
  %55 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %17, <1024 x double> %54)
  %56 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %18, <1024 x double> %55)
  %57 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %19, <1024 x double> %56)
  %58 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %20, <1024 x double> %57)
  %59 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %21, <1024 x double> %58)
  %60 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %22, <1024 x double> %59)
  %61 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %23, <1024 x double> %60)
  %62 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %24, <1024 x double> %61)
  %63 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %25, <1024 x double> %62)
  %64 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %26, <1024 x double> %63)
  %65 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %27, <1024 x double> %64)
  %66 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %28, <1024 x double> %65)
  %67 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %29, <1024 x double> %66)
  %68 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %30, <1024 x double> %67)
  %69 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %31, <1024 x double> %68)
  %70 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %32, <1024 x double> %69)
  %71 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %33, <1024 x double> %70)
  %72 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %34, <1024 x double> %71)
  %73 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %35, <1024 x double> %72)
  %74 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %36, <1024 x double> %73)
  %75 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %37, <1024 x double> %74)
  %76 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %38, <1024 x double> %75)

  tail call void (ptr, i64, i64, i64, <1024 x double>, ...) @llvm.linx.mcall.par.0d1u.v1024f64(ptr nonnull @_Z7copyoutu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, <1024 x double> %76, ptr %p3)
  ret void
}

declare void @_Z6copyinu11matrix_typeILm1ELm1024EdEPd(<1024 x double> noundef, ptr noundef) #1

; Function Attrs: nounwind
declare <1024 x double> @llvm.linx.mcall.par.1d0u.v1024f64(ptr, i64, i64, i64, ...) #2

declare void @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_(<1024 x double> noundef, <1024 x double> noundef, <1024 x double> noundef) #3

; Function Attrs: nounwind
declare <1024 x double> @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) #2

declare void @_Z7copyoutu11matrix_typeILm1ELm1024EdEPd(<1024 x double> noundef, ptr noundef) #1

; Function Attrs: nounwind
declare void @llvm.linx.mcall.par.0d1u.v1024f64(ptr, i64, i64, i64, <1024 x double>, ...) #2

attributes #0 = { mustprogress nounwind "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { "__mtc__" "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #2 = { nounwind }
attributes #3 = { "__vec__" "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (ssh://git@codehub-dg-y.huawei.com:2222/linx/ISA-Codesign/BlockISA/linx-llvm.git d82a7c69d7ead45f0a00c9312a0d4525e102197f)"}
