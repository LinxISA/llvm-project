; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

target triple = "linx64v5"

; Verify that two Tile outputs do not offset the %Z operand lookup and that
; the first destination binding remains non-terminating.
;
; CHECK-LABEL: <tsort_f16>:
; CHECK: BSTART.TEPL TSORT32, FP16
; CHECK: B.IOT {{.*}}, mask=1111, {{[[:space:]]*}}->{{.*}}<2KB>
; CHECK-NEXT: B.IOT mask=1111, last, {{[[:space:]]*}}->{{.*}}<4KB>
define void @tsort_f16(ptr %value_dst, ptr %index_dst, ptr %source_ptr,
                       i32 %descending) {
  %source = load <1024 x half>, ptr %source_ptr, align 32
  %result = call { <1024 x half>, <1024 x i32> } asm sideeffect "BSTART.TEPL 108, ${3:c}\0AB.DIM $4, 0, ->lb0\0AB.IOR [$5], []\0AB.IOT $2, mask=1111, ->$0<${6:Z}>\0AB.IOT mask=1111, last, ->$1<${7:Z}>\0A", "=&@2Tr,=&@2Tr,@2Tr,i,r,r,i,i"(<1024 x half> %source, i32 4, i32 32, i32 %descending, i32 5, i32 6)
  %values = extractvalue { <1024 x half>, <1024 x i32> } %result, 0
  %indices = extractvalue { <1024 x half>, <1024 x i32> } %result, 1
  store <1024 x half> %values, ptr %value_dst, align 32
  store <1024 x i32> %indices, ptr %index_dst, align 32
  ret void
}
