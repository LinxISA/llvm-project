; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t.o
declare void @external(ptr)
define void @runtime(ptr %constructor, ptr %p) {
  call void %constructor() "linx-scalar-shareability"="intra"
  call void @external(ptr %p) "linx-scalar-shareability"="intra"
  ret void
}
