; RUN: llc < %s -linxv5-kill-insert=callsite -linxv5-kill-exclude-csr=false -linxv5-kill-exclude-argreg=false -march=linx64be -O3 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK1
; RUN: llc < %s -linxv5-kill-insert=callsite -linxv5-kill-exclude-csr=true -linxv5-kill-exclude-argreg=true -march=linx64be -O3 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK2

; CHECK1: kill [a2, a3, a4, a5, a6, a7, x0, x1, x2, x3]
; CHECK2: kill [x0, x1, x2, x3]

define dso_local signext i32 @B(i32 noundef signext %num1, i32 noundef signext %num2) local_unnamed_addr #0 {
entry:
  %add = add nsw i32 %num2, %num1
  ret i32 %add
}

; Function Attrs: nounwind
define dso_local signext i32 @A(i32 noundef signext %num1, i32 noundef signext %num2) local_unnamed_addr #1 {
entry:
  %add.i = add nsw i32 %num2, %num1
  %call1 = tail call signext i32 @C(i32 noundef signext %num1) #3
  %add = add nsw i32 %add.i, %call1
  ret i32 %add
}

declare dso_local signext i32 @C(i32 noundef signext) local_unnamed_addr #2