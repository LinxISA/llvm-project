; RUN: llc < %s --march=linx64 -O2 -enable-all-vector-as-tilereg=true 2>&1 | FileCheck %s --dump-input always -vv

define void @vfoo(<1024 x float> %in, <1024 x i8> %in1) #0 {
  %i = tail call i16 @llvm.blkv.get.index.x()
  %convi = zext i16 %i to i64
  %j = tail call i16 @llvm.blkv.get.index.y()
  %convj = zext i16 %j to i64
  %p = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v1024f32(<1024 x float> %in)
  %p1 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v1024i8(<1024 x i8> %in1)
  %mul = shl i64 %convj, 6
  %add = add i64 %mul, %convi
  %a = getelementptr inbounds float, ptr addrspace(6) %p, i64 %add
; CHECK: v.sw.local t#1.sw, [ta, lc0<<2, lc1.uh<<8]
  store float 1.0e+00, ptr addrspace(6) %a

  %noff = add i64 %add, %convi
  %n = getelementptr inbounds float, ptr addrspace(6) %p, i64 %noff
; CHECK: v.lw.local [ta, vt#{{[1-4]}}.sd<<2]
  %tmp.0 = load volatile float, ptr addrspace(6) %n

  %coff = add i64 %convi, 64
  %c = getelementptr inbounds float, ptr addrspace(6) %p, i64 %coff
; CHECK: v.lwi.local [ta, lc0<<2, 256]
  %tmp.1 = load volatile float, ptr addrspace(6) %c

  %c1 = getelementptr inbounds i8, ptr addrspace(6) %p1, i64 %coff
; CHECK: v.lbi.local [tb, lc0, 64]
  %tmp.2 = load volatile i8, ptr addrspace(6) %c1

; CHECK: v.lw.local [ta, lc0<<2, lc1.uh<<8], ->vt.w
  %tmp.3 = load volatile float, ptr addrspace(6) %a

  %joff = mul i64 %convi, 5
  %jump = getelementptr inbounds float, ptr addrspace(6) %p, i64 %joff
; CHECK: v.sw.u.local t#1.sw, [ta, vt#1.sd]
  store float 2.0e+00, ptr addrspace(6) %jump
  ret void
}

declare i16 @llvm.blkv.get.index.x()

declare i16 @llvm.blkv.get.index.y()

declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v1024f32(<1024 x float>)
declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v1024i8(<1024 x i8>)

attributes #0 = { "__vec__" }
