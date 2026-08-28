; RUN: not opt -passes=verify -disable-output < %s 2>&1 | FileCheck %s

target triple = "linx64v5"

declare token @llvm.linx.experimental.tile.array.empty(i32, i32, i64, i64, i32)
declare token @llvm.linx.experimental.tile.array.insert.v4i32(token, <4 x i32>, i32, i32, i32)
declare <4 x i32> @llvm.linx.experimental.tassembly.v4i32(token, i64)

define <4 x i32> @duplicate(<4 x i32> %fragment) {
  %empty = call token @llvm.linx.experimental.tile.array.empty(i32 1, i32 2, i64 128, i64 256, i32 0)
  %first = call token @llvm.linx.experimental.tile.array.insert.v4i32(token %empty, <4 x i32> %fragment, i32 0, i32 0, i32 0)
  %second = call token @llvm.linx.experimental.tile.array.insert.v4i32(token %first, <4 x i32> %fragment, i32 0, i32 0, i32 0)
  %result = call <4 x i32> @llvm.linx.experimental.tassembly.v4i32(token %second, i64 256)
  ret <4 x i32> %result
}

; CHECK: Linx Tile assembly slot is written more than once
