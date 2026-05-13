; RUN: llc < %s -enable-all-vector-as-tilereg=true -mcpu=janus --march=linx64v5 -linxv5-enable-clock-hand-opt=false -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; CHECK-LABEL: _Z3fooPdS_:
; CHECK: BSTART.CUBE      MAMULB, FP64
; CHECK: B.IOTI  [t#1, u#1], last  ->t<8KB>
; CHECK: B.IOR   [a0],[]
; CHECK: C.B.DIMI        128,    ->lb0
define dso_local void @_Z3fooPdS_(ptr %p1, ptr %p2) local_unnamed_addr {
entry:
  %0 = tail call <512 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v512f64(ptr nonnull @_Z6copyinDv512_dPd, i64 128, i64 1, i64 1, ptr %p1)
  %1 = tail call <512 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v512f64(ptr nonnull @_Z6copyinDv512_dPd, i64 128, i64 1, i64 1, ptr %p2)
  %2 = tail call <512 x double> asm sideeffect "BSTART.CUBE 0, ${1:c}\0AB.IOTI [$2, $3], last ->$0<${4:c}>\0AB.IOR [$5],[]\0AC.B.DIMI ${6:c}, ->lb0\0A", "=@2Tr,i,@2Tr,@2Tr,i,r,i"(i32 0, <512 x double> %0, <512 x double> %1, i32 9, i32 1, i32 128)
  ret void
}

declare dso_local void @_Z6copyinDv512_dPd(<512 x double> noundef, ptr noundef)

declare <512 x double> @llvm.linx.mcall.par.1d0u.v512f64(ptr, i64, i64, i64, ...)


