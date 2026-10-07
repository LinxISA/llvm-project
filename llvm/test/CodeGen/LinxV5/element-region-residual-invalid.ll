; RUN: not llc -mtriple=linx64v5 -o /dev/null %s 2>&1 | FileCheck %s
; RUN: not opt -mtriple=linx64v5 -passes='default<O2>' -disable-output %s 2>&1 | FileCheck %s
; RUN: opt -passes='function(no-op-function)' %s -o %t.bc
; RUN: not opt -mtriple=linx64v5 -passes='default<O2>' -disable-output %t.bc 2>&1 | FileCheck %s

declare void @llvm.linx.experimental.element.region(metadata)

define void @residual_region() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0), !dbg !5
  ret void
}

; CHECK: error: element-region.cpp:9:3: {{.*}}PTO element region: required region lowering did not complete

!llvm.dbg.cu = !{!1}
!llvm.module.flags = !{!4}
!0 = distinct !{}
!1 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !2,
                             producer: "test", isOptimized: false,
                             runtimeVersion: 0, emissionKind: FullDebug)
!2 = !DIFile(filename: "element-region.cpp", directory: "/")
!3 = distinct !DISubprogram(name: "residual_region", scope: !2, file: !2,
                            line: 7, type: !6, scopeLine: 7,
                            spFlags: DISPFlagDefinition, unit: !1)
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !DILocation(line: 9, column: 3, scope: !3)
!6 = !DISubroutineType(types: !7)
!7 = !{}
