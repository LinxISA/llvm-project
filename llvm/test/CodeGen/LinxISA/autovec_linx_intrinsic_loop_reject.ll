; RUN: llc -mtriple=linx64 -O2 \
; RUN:   --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq \
; RUN:   --linx-simt-autovec-remarks=%t.remarks.json \
; RUN:   < %s | FileCheck %s --check-prefix=ASM
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tileop.binary(%linx.tile, %linx.tile, i32, i32, i32)

define void @tile_intrinsic_loop(ptr %a, ptr %dst) {
entry:
  %seed = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  br label %loop

loop:                                             ; preds = %loop, %entry
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %acc = phi %linx.tile [ %seed, %entry ], [ %acc.next, %loop ]
  %acc.next = call %linx.tile @llvm.linx.tileop.binary(%linx.tile %acc, %linx.tile %seed, i32 0, i32 6, i32 1)
  %i.next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %i.next, 4
  br i1 %done, label %exit, label %loop

exit:                                             ; preds = %loop
  %outv = phi %linx.tile [ %acc.next, %loop ]
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %outv, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; ASM-LABEL: tile_intrinsic_loop:
; ASM-NOT: BSTART.MSEQ
; ASM-NOT: BSTART.MPAR

; REMARK: "function":"tile_intrinsic_loop"
; REMARK: "status":"reject"
; REMARK: "reason":"linx_tile_intrinsic_loop"
