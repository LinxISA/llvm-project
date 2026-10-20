// RUN: %clang -target linx64v5-unknown-linux-musl -O2 \
// RUN:   -mllvm -linxv5-enable-generic-element=true -c %s -o %t.direct.o
// RUN: %clang -target linx64v5-unknown-linux-musl -O2 \
// RUN:   -mllvm -linxv5-enable-generic-element=true -S %s -o %t.s
// RUN: FileCheck %s --check-prefix=ASM < %t.s
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %t.s -o %t.roundtrip.o
// RUN: llvm-objdump -d --disassembler-options=no-tile-macros \
// RUN:   --disassemble-symbols=domain8,default15 %t.direct.o | \
// RUN:   sed '1,2d' > %t.direct.dis
// RUN: llvm-objdump -d --disassembler-options=no-tile-macros \
// RUN:   --disassemble-symbols=domain8,default15 %t.roundtrip.o | \
// RUN:   sed '1,2d' > %t.roundtrip.dis
// RUN: cmp %t.direct.dis %t.roundtrip.dis
// RUN: FileCheck %s --check-prefix=DIS < %t.direct.dis

#define PTO_PE_DOMAIN(Mask)                                                  \
  __attribute__((annotate("pto.pe_domain", Mask), noinline))

// ASM-LABEL: domain8:
// ASM: .pto_pe_domain 8
// ASM: .pto_pe_domain 15
// ASM-LABEL: default15:
// ASM: .pto_pe_domain 15
// ASM: .pto_pe_domain 15

// DIS-LABEL: <domain8>:
// DIS-LABEL: <default15>:

extern "C" PTO_PE_DOMAIN(8) int domain8(int *value) {
  return *value + 1;
}

extern "C" int default15(int *value) {
  return *value + 2;
}
