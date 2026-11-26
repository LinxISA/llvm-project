; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t.o
; Calls synthesized after Clang emission have no call-site contract.
declare void @_Unwind_Resume(ptr)
define void @cleanup(ptr %exception) "linx-runtime-intra-abi" {
  call void @_Unwind_Resume(ptr %exception)
  unreachable
}
