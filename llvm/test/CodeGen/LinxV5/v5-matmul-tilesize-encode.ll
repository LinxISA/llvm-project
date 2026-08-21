; RUN: split-file %s %t.dir
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %t.dir/pos.ll -o %t.pos.o
; RUN: llvm-objdump -d --no-show-raw-insn %t.pos.o | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %t.dir/neg.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=ERR

; v5: B.IOT SizeCode is encoded directly from the IR tile type size at
; per-PE granularity (PTO-ISA ADR 0069). There is NO 4-PE (whole-core)
; multiplier: the developer writes a tile whose byte size IS the encoded
; SizeCode, so "write N bytes -> encode N bytes". Legal per-PE sizes are
; 128 B..64 KB (SizeCode 1..10 = Log2(S)-6); 128 KB/256 KB are rejected for
; B.IOT (Local).
;
;   <32 x float>   = 128 B  -> SizeCode 1
;   <64 x float>   = 256 B  -> SizeCode 2
;   <128 x float>  = 512 B  -> SizeCode 3
;   <256 x float>  = 1 KB   -> SizeCode 4
;   <512 x float>  = 2 KB   -> SizeCode 5
;   <1024 x float> = 4 KB   -> SizeCode 6
;   <2048 x float> = 8 KB   -> SizeCode 7
;   <4096 x float> = 16 KB  -> SizeCode 8
;   <8192 x float> = 32 KB  -> SizeCode 9
;   <16384 x float>= 64 KB  -> SizeCode 10
;
; Tile types above 64 KB (<32768 x float> = 128 KB, <65536 x float> = 256 KB)
; must be rejected for a Local B.IOT destination.

; CHECK-LABEL: <t128>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<128B>
; CHECK-LABEL: <t256>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<256B>
; CHECK-LABEL: <t512>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<512B>
; CHECK-LABEL: <t1k>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<1KB>
; CHECK-LABEL: <t2k>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<2KB>
; CHECK-LABEL: <t4k>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<4KB>
; CHECK-LABEL: <t8k>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<8KB>
; CHECK-LABEL: <t16k>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<16KB>
; CHECK-LABEL: <t32k>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<32KB>
; CHECK-LABEL: <t64k>:
; CHECK: B.IOT {{.*}} mask=1111, last, ->{{.*}}<64KB>

; ERR: LLVM ERROR: LinxV5 Tile size exceeds the per-PE capacity for its
; ERR: destination role

;--- pos.ll
target triple = "linx64v5"

define void @t128(ptr %a, ptr %b, ptr %out) {
  %x = call <32 x float> @llvm.linx.blk.tload.v32f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 16)
  %y = call <32 x float> @llvm.linx.blk.tload.v32f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 16)
  %r = call <32 x float> @llvm.linx.blk.matmul.v32f32.v32f32.v32f32(i64 16, i64 16, i64 16, i64 1, i64 1, <32 x float> %x, <32 x float> %y)
  call void @llvm.linx.blk.tstore.v32f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <32 x float> %r)
  ret void
}

define void @t256(ptr %a, ptr %b, ptr %out) {
  %x = call <64 x float> @llvm.linx.blk.tload.v64f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 16)
  %y = call <64 x float> @llvm.linx.blk.tload.v64f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 16)
  %r = call <64 x float> @llvm.linx.blk.matmul.v64f32.v64f32.v64f32(i64 16, i64 16, i64 16, i64 1, i64 1, <64 x float> %x, <64 x float> %y)
  call void @llvm.linx.blk.tstore.v64f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <64 x float> %r)
  ret void
}

define void @t512(ptr %a, ptr %b, ptr %out) {
  %x = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 16)
  %y = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 16)
  %r = call <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %x, <128 x float> %y)
  call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <128 x float> %r)
  ret void
}

define void @t1k(ptr %a, ptr %b, ptr %out) {
  %x = call <256 x float> @llvm.linx.blk.tload.v256f32(i64 32, i64 32, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 32)
  %y = call <256 x float> @llvm.linx.blk.tload.v256f32(i64 32, i64 32, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 32)
  %r = call <256 x float> @llvm.linx.blk.matmul.v256f32.v256f32.v256f32(i64 32, i64 32, i64 32, i64 1, i64 1, <256 x float> %x, <256 x float> %y)
  call void @llvm.linx.blk.tstore.v256f32(i64 32, i64 32, i64 1, i64 1, i64 0, ptr %out, i64 32, <256 x float> %r)
  ret void
}

define void @t2k(ptr %a, ptr %b, ptr %out) {
  %x = call <512 x float> @llvm.linx.blk.tload.v512f32(i64 64, i64 32, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 64)
  %y = call <512 x float> @llvm.linx.blk.tload.v512f32(i64 64, i64 32, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 64)
  %r = call <512 x float> @llvm.linx.blk.matmul.v512f32.v512f32.v512f32(i64 64, i64 32, i64 32, i64 1, i64 1, <512 x float> %x, <512 x float> %y)
  call void @llvm.linx.blk.tstore.v512f32(i64 64, i64 32, i64 1, i64 1, i64 0, ptr %out, i64 64, <512 x float> %r)
  ret void
}

define void @t4k(ptr %a, ptr %b, ptr %out) {
  %x = call <1024 x float> @llvm.linx.blk.tload.v1024f32(i64 128, i64 32, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 128)
  %y = call <1024 x float> @llvm.linx.blk.tload.v1024f32(i64 128, i64 32, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 128)
  %r = call <1024 x float> @llvm.linx.blk.matmul.v1024f32.v1024f32.v1024f32(i64 128, i64 32, i64 32, i64 1, i64 1, <1024 x float> %x, <1024 x float> %y)
  call void @llvm.linx.blk.tstore.v1024f32(i64 128, i64 32, i64 1, i64 1, i64 0, ptr %out, i64 128, <1024 x float> %r)
  ret void
}

define void @t8k(ptr %a, ptr %b, ptr %out) {
  %x = call <2048 x float> @llvm.linx.blk.tload.v2048f32(i64 128, i64 64, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 128)
  %y = call <2048 x float> @llvm.linx.blk.tload.v2048f32(i64 128, i64 64, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 128)
  %r = call <2048 x float> @llvm.linx.blk.matmul.v2048f32.v2048f32.v2048f32(i64 128, i64 64, i64 64, i64 1, i64 1, <2048 x float> %x, <2048 x float> %y)
  call void @llvm.linx.blk.tstore.v2048f32(i64 128, i64 64, i64 1, i64 1, i64 0, ptr %out, i64 128, <2048 x float> %r)
  ret void
}

define void @t16k(ptr %a, ptr %b, ptr %out) {
  %x = call <4096 x float> @llvm.linx.blk.tload.v4096f32(i64 128, i64 128, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 128)
  %y = call <4096 x float> @llvm.linx.blk.tload.v4096f32(i64 128, i64 128, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 128)
  %r = call <4096 x float> @llvm.linx.blk.matmul.v4096f32.v4096f32.v4096f32(i64 128, i64 128, i64 128, i64 1, i64 1, <4096 x float> %x, <4096 x float> %y)
  call void @llvm.linx.blk.tstore.v4096f32(i64 128, i64 128, i64 1, i64 1, i64 0, ptr %out, i64 128, <4096 x float> %r)
  ret void
}

define void @t32k(ptr %a, ptr %b, ptr %out) {
  %x = call <8192 x float> @llvm.linx.blk.tload.v8192f32(i64 128, i64 256, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 128)
  %y = call <8192 x float> @llvm.linx.blk.tload.v8192f32(i64 128, i64 256, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 128)
  %r = call <8192 x float> @llvm.linx.blk.matmul.v8192f32.v8192f32.v8192f32(i64 128, i64 256, i64 256, i64 1, i64 1, <8192 x float> %x, <8192 x float> %y)
  call void @llvm.linx.blk.tstore.v8192f32(i64 128, i64 256, i64 1, i64 1, i64 0, ptr %out, i64 128, <8192 x float> %r)
  ret void
}

define void @t64k(ptr %a, ptr %b, ptr %out) {
  %x = call <16384 x float> @llvm.linx.blk.tload.v16384f32(i64 256, i64 256, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 256)
  %y = call <16384 x float> @llvm.linx.blk.tload.v16384f32(i64 256, i64 256, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 256)
  %r = call <16384 x float> @llvm.linx.blk.matmul.v16384f32.v16384f32.v16384f32(i64 256, i64 256, i64 256, i64 1, i64 1, <16384 x float> %x, <16384 x float> %y)
  call void @llvm.linx.blk.tstore.v16384f32(i64 256, i64 256, i64 1, i64 1, i64 0, ptr %out, i64 256, <16384 x float> %r)
  ret void
}

declare <32 x float> @llvm.linx.blk.tload.v32f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v32f32(i64, i64, i64, i64, i64, ptr, i64, <32 x float>)
declare <32 x float> @llvm.linx.blk.matmul.v32f32.v32f32.v32f32(i64, i64, i64, i64, i64, <32 x float>, <32 x float>)
declare <64 x float> @llvm.linx.blk.tload.v64f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v64f32(i64, i64, i64, i64, i64, ptr, i64, <64 x float>)
declare <64 x float> @llvm.linx.blk.matmul.v64f32.v64f32.v64f32(i64, i64, i64, i64, i64, <64 x float>, <64 x float>)
declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
declare <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, <128 x float>)
declare <256 x float> @llvm.linx.blk.tload.v256f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v256f32(i64, i64, i64, i64, i64, ptr, i64, <256 x float>)
declare <256 x float> @llvm.linx.blk.matmul.v256f32.v256f32.v256f32(i64, i64, i64, i64, i64, <256 x float>, <256 x float>)
declare <512 x float> @llvm.linx.blk.tload.v512f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v512f32(i64, i64, i64, i64, i64, ptr, i64, <512 x float>)
declare <512 x float> @llvm.linx.blk.matmul.v512f32.v512f32.v512f32(i64, i64, i64, i64, i64, <512 x float>, <512 x float>)
declare <1024 x float> @llvm.linx.blk.tload.v1024f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v1024f32(i64, i64, i64, i64, i64, ptr, i64, <1024 x float>)
declare <1024 x float> @llvm.linx.blk.matmul.v1024f32.v1024f32.v1024f32(i64, i64, i64, i64, i64, <1024 x float>, <1024 x float>)
declare <2048 x float> @llvm.linx.blk.tload.v2048f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v2048f32(i64, i64, i64, i64, i64, ptr, i64, <2048 x float>)
declare <2048 x float> @llvm.linx.blk.matmul.v2048f32.v2048f32.v2048f32(i64, i64, i64, i64, i64, <2048 x float>, <2048 x float>)

declare <4096 x float> @llvm.linx.blk.tload.v4096f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v4096f32(i64, i64, i64, i64, i64, ptr, i64, <4096 x float>)
declare <4096 x float> @llvm.linx.blk.matmul.v4096f32.v4096f32.v4096f32(i64, i64, i64, i64, i64, <4096 x float>, <4096 x float>)
declare <8192 x float> @llvm.linx.blk.tload.v8192f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v8192f32(i64, i64, i64, i64, i64, ptr, i64, <8192 x float>)
declare <8192 x float> @llvm.linx.blk.matmul.v8192f32.v8192f32.v8192f32(i64, i64, i64, i64, i64, <8192 x float>, <8192 x float>)
declare <16384 x float> @llvm.linx.blk.tload.v16384f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v16384f32(i64, i64, i64, i64, i64, ptr, i64, <16384 x float>)
declare <16384 x float> @llvm.linx.blk.matmul.v16384f32.v16384f32.v16384f32(i64, i64, i64, i64, i64, <16384 x float>, <16384 x float>)

;--- neg.ll
target triple = "linx64v5"
define void @too_big_128k(ptr %a, ptr %b, ptr %out) {
  %x = call <32768 x float> @llvm.linx.blk.tload.v32768f32(i64 512, i64 256, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 512)
  %y = call <32768 x float> @llvm.linx.blk.tload.v32768f32(i64 512, i64 256, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 512)
  %r = call <32768 x float> @llvm.linx.blk.matmul.v32768f32.v32768f32.v32768f32(i64 512, i64 256, i64 256, i64 1, i64 1, <32768 x float> %x, <32768 x float> %y)
  call void @llvm.linx.blk.tstore.v32768f32(i64 512, i64 256, i64 1, i64 1, i64 0, ptr %out, i64 512, <32768 x float> %r)
  ret void
}
declare <32768 x float> @llvm.linx.blk.tload.v32768f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v32768f32(i64, i64, i64, i64, i64, ptr, i64, <32768 x float>)
declare <32768 x float> @llvm.linx.blk.matmul.v32768f32.v32768f32.v32768f32(i64, i64, i64, i64, i64, <32768 x float>, <32768 x float>)

define void @too_big_256k(ptr %a, ptr %b, ptr %out) {
  %x = call <65536 x float> @llvm.linx.blk.tload.v65536f32(i64 512, i64 512, i64 1, i64 1, i64 3, i64 4, ptr %a, i64 512)
  %y = call <65536 x float> @llvm.linx.blk.tload.v65536f32(i64 512, i64 512, i64 1, i64 1, i64 3, i64 3, ptr %b, i64 512)
  %r = call <65536 x float> @llvm.linx.blk.matmul.v65536f32.v65536f32.v65536f32(i64 512, i64 512, i64 512, i64 1, i64 1, <65536 x float> %x, <65536 x float> %y)
  call void @llvm.linx.blk.tstore.v65536f32(i64 512, i64 512, i64 1, i64 1, i64 0, ptr %out, i64 512, <65536 x float> %r)
  ret void
}
declare <65536 x float> @llvm.linx.blk.tload.v65536f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v65536f32(i64, i64, i64, i64, i64, ptr, i64, <65536 x float>)
declare <65536 x float> @llvm.linx.blk.matmul.v65536f32.v65536f32.v65536f32(i64, i64, i64, i64, i64, <65536 x float>, <65536 x float>)