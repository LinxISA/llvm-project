# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped -filetype=obj %t/main.s -o %t/main.o
# RUN: llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped -filetype=obj %t/definition.s -o %t/definition.o
# RUN: llvm-readobj -r %t/main.o | FileCheck %s --check-prefix=RELOC
# RUN: not ld.lld -m elf64linx %t/main.o %t/definition.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=OLD-ABI
# RUN: ld.lld -m elf64linx --linx-pto-v0591-scoped -e backward %t/main.o %t/definition.o -o %t/out
# RUN: llvm-objdump -d %t/out | FileCheck %s --check-prefix=DIS
# RUN: llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped -filetype=obj %t/unaligned.s -o %t/unaligned.o
# RUN: not ld.lld -m elf64linx --linx-pto-v0591-scoped %t/unaligned.o %t/definition.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=UNALIGNED
# RUN: llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped -filetype=obj %t/range.s -o %t/range.o
# RUN: not ld.lld -m elf64linx --linx-pto-v0591-scoped --defsym=far=0x80000 %t/range.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=RANGE
# RUN: yaml2obj -DINSN=1F00392100000000 -DRELOC=0x21 %t/bad.yaml -o %t/bad-prefix.o
# RUN: not ld.lld -m elf64linx --linx-pto-v0591-scoped %t/bad-prefix.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD-CARRIER
# RUN: yaml2obj -DINSN=1F04392100000100 -DRELOC=0x21 %t/bad.yaml -o %t/bad-padding.o
# RUN: not ld.lld -m elf64linx --linx-pto-v0591-scoped %t/bad-padding.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD-CARRIER
# RUN: yaml2obj -DINSN=1F04050000000000 -DRELOC=0x21 %t/bad.yaml -o %t/bad-inner.o
# RUN: not ld.lld -m elf64linx --linx-pto-v0591-scoped %t/bad-inner.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD-INNER
# RUN: yaml2obj -DINSN=1F04392100000000 -DRELOC=0x23 %t/bad.yaml -o %t/bad-layout.o
# RUN: not ld.lld -m elf64linx --linx-pto-v0591-scoped %t/bad-layout.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD-LAYOUT

# RELOC: 0x2 R_LINX_SCOPED_PCR17_LOAD forward 0x0
# RELOC: 0xA R_LINX_SCOPED_PCR17_STORE backward 0x0
# RELOC: 0x12 R_LINX_SCOPED_HL_PCR29_LOAD forward 0x0
# RELOC: 0x1A R_LINX_SCOPED_HL_PCR29_STORE backward 0x0
# RELOC: 0x22 R_LINX_SCOPED_PCR17_LOAD external 0x4

# DIS: 11212: {{.*}}lw.pcr.private{{.*}}[0xb]{{.*}}<forward>
# DIS: 1121a: {{.*}}sw.pcr.intra{{.*}}[0xfffffffffffffffe]{{.*}}<backward>
# DIS: 11222: {{.*}}hl.lw.pcr.inter{{.*}}[0x7]{{.*}}<forward>
# DIS: 1122a: {{.*}}hl.sw.pcr.private{{.*}}[0xfffffffffffffffa]{{.*}}<backward>
# DIS: 11232: {{.*}}lw.pcr.inter{{.*}}[0x5]{{.*}}<external+0x4>
# UNALIGNED: error: {{.*}}scoped PCR target is not 4-byte aligned
# RANGE: error: {{.*}}relocation R_LINX_SCOPED_PCR17_LOAD out of range
# OLD-ABI: error: {{.*}}release mismatch (expected 0.58.7)
# BAD-CARRIER: error: {{.*}}scoped PCR relocation requires a matching 64-bit carrier
# BAD-INNER: error: {{.*}}scoped PCR relocation does not match the embedded PCR form
# BAD-LAYOUT: error: {{.*}}scoped PCR relocation requires a matching 64-bit carrier

#--- main.s
.text
.globl backward
backward:
  C.BSTOP
  lw.pcr.private [forward], ->a0
  sw.pcr.intra a1, [backward]
  hl.lw.pcr.inter [forward], ->a2
  hl.sw.pcr.private a3, [backward]
  lw.pcr.inter [external+4], ->a4
  C.BSTOP
  .p2align 2
.globl forward
forward:
  C.BSTOP

#--- definition.s
.text
.globl external
external:
  C.BSTOP

#--- unaligned.s
.text
.globl _start
_start:
  lw.pcr.private [external+2], ->a0

#--- range.s
.text
.globl _start
_start:
  lw.pcr.private [far], ->a0

#--- bad.yaml
--- !ELF
FileHeader:
  Class: ELFCLASS64
  Data: ELFDATA2LSB
  Type: ET_REL
  Machine: 0xE9
Sections:
  - Name: .text
    Type: SHT_PROGBITS
    Flags: [ SHF_ALLOC, SHF_EXECINSTR ]
    AddressAlign: 4
    Content: [[INSN]]0000
  - Name: .note.pto.isa
    Type: SHT_NOTE
    Flags: [ SHF_ALLOC ]
    AddressAlign: 4
    Notes:
      - Name: PTO
        Desc: 7B22656E636F64696E675F616269223A2270746F2D6973612D302E35392E312D6D6F64652D66756E6374696F6E2D7631222C22656E636F64696E675F70726F6A656374696F6E5F736861323536223A2264393063383831373534643636663263643236663630313464306235383263626138383330376537303132333538336438376663353961393138323239633238222C2272656C65617365223A22302E35392E31227D
        Type: NT_VERSION
  - Name: .rela.text
    Type: SHT_RELA
    Link: .symtab
    Info: .text
    Relocations:
      - Symbol: target
        Type: [[RELOC]]
  - Type: SectionHeaderTable
Symbols:
  - Name: target
    Section: .text
    Value: 8
...
