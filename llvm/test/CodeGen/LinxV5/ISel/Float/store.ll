; RUN: llc < %s --march=linx64v5 -linxv5-enable-legacy-isel=false -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: store.fd:
; DAG: sdi a1, [a0, 1024]
; DAG: c.setc.tgt ra
define void @store.fd(ptr %p, double %a) {
  %ptr = getelementptr inbounds i8, ptr %p, i64 1024
  store double %a, ptr %ptr
  ret void
}
