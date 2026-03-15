; RUN: not llc -mtriple=linx64 < %s 2>&1 | FileCheck %s

declare void @llvm.linx.vblock.launch(i32, ptr, i64, i64, i64, i32,
                                      i64, i64, i64, i64, i64, i64,
                                      i64, i64, i64, i64, i64, i64)

define void @reject_archived_raw_base() #0 {
entry:
  call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 8, i64 1, i64 1, i32 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0,
                                     i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

attributes #0 = { "linx-vblock-body-asm"="  v.lwi.u.local [to1, lc0.uh<<2, 8], ->vn.w\0A  C.BSTOP\0A" }

; CHECK: archived raw vector operand name is not allowed in canonical v0.4; use TA/TB/TC/TD/TO/TS
