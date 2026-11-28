; RUN: llc -mtriple=linx64v5 -enable-all-vector-as-tilereg=true -filetype=obj %s -o %t.o
; RUN: llvm-objdump -d --no-show-raw-insn -M no-tile-macros %t.o | FileCheck %s --implicit-check-not=65536
;
; Verify the encoded load AND store, not just their SelectionDAG/MIR operands.
; 1 x 65536 U8 wraps LB0/LB2 to zero during execution. A 2 x 32768
; rectangle with a 32768-byte stride covers every byte of the 64 KiB vector.
; CHECK-LABEL: <copy_u8_64k>:
; CHECK: lui{{[ \t]+}}8,{{.*}}->[[PITCH:[a-z][a-z0-9]*]]
; CHECK: BSTART.{{.*}}TLOAD{{.*}}U8
; CHECK: B.DIM{{.*}}32768,{{.*}}->lb0
; CHECK: {{(B.DIM|C.B.DIMI)}}{{.*}}2,{{.*}}->lb1
; CHECK: B.DIM{{.*}}32768,{{.*}}->lb2
; CHECK: B.IOT{{.*}}<64KB>
; CHECK: B.IOR{{.*}}[[PITCH]]
; CHECK: BSTART.{{.*}}TSTORE{{.*}}U8
; CHECK: B.DIM{{.*}}32768,{{.*}}->lb0
; CHECK: {{(B.DIM|C.B.DIMI)}}{{.*}}2,{{.*}}->lb1
; CHECK: B.DIM{{.*}}32768,{{.*}}->lb2
; CHECK: B.IOR{{.*}}[[PITCH]]

target triple = "linx64v5-unknown-linux-musl"
define void @copy_u8_64k(ptr %src, ptr %dst) {
  %v = load volatile <65536 x i8>, ptr %src, align 32
  store volatile <65536 x i8> %v, ptr %dst, align 32
  ret void
}
