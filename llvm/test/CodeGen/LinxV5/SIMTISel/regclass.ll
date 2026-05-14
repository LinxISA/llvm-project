; RUN: llc < %s -enable-all-vector-as-tilereg=true -stop-after=finalize-isel --march=linx64 -O2 | FileCheck %s --dump-input always -vv

; CHECK-NOT: _and_

; CHECK:      %0:simtcgv = COPY $simt_ta
; CHECK-NEXT: %1:simtcgv = COPY $simt_lc0
; CHECK-NEXT: %2:simtcgvl = SIMT_SLLI 2, %1, 6, 5, implicit $simt_p
; CHECK-NEXT: %3:cgsl = LUI
; CHECK-NEXT: SIMT_SW
define void @vfoo(<1024 x float> %a) #1 {
  %pa = call ptr @llvm.blkv.get.tile.ptr.v1024f32(<1024 x float> %a)
  %x = call i16 @llvm.blkv.get.index.x()
  %off = mul i16 %x, 32
  %aidx = getelementptr inbounds float, ptr %pa, i16 %off
  store float 1.0, ptr %aidx
  ret void
}

declare ptr @llvm.blkv.get.tile.ptr.v1024f32(<1024 x float>)
declare i16 @llvm.blkv.get.index.x()

attributes #1 = {"__vec__"}
