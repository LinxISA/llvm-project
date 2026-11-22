# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=linx64 ordinary.s -o ordinary.o
# RUN: ld.lld --no-relax -m elf64linx --section-start=.text=0x12000 \
# RUN:   --section-start=.data=0x10000 -e _start ordinary.o -o ordinary
# RUN: llvm-objdump -d --no-show-raw-insn ordinary | FileCheck %s --check-prefix=ORDINARY
# RUN: llvm-mc -filetype=obj -triple=linx64 linux.s -o linux.o
# RUN: ld.lld --no-relax -m elf64linx -T linux.ld linux.o -o linux
# RUN: llvm-objdump -d --no-show-raw-insn linux | FileCheck %s --check-prefix=LINUX
# RUN: yaml2obj got.yaml -o got.o
# RUN: ld.lld --no-relax -m elf64linx --linx-pto-v0591-scoped \
# RUN:   --section-start=.text=0x12000 \
# RUN:   --section-start=.data=0x10000 --section-start=.got=0x14000 \
# RUN:   -e _start got.o -o got
# RUN: llvm-objdump -d --no-show-raw-insn got | FileCheck %s --check-prefix=GOT
# RUN: llc -mtriple=linx64-linux-gnu -relocation-model=pic -filetype=obj \
# RUN:   plt.ll -o plt.o
# RUN: ld.lld -shared -m elf64linx --unresolved-symbols=ignore-all \
# RUN:   --section-start=.plt=0x12000 --section-start=.got.plt=0x13808 \
# RUN:   plt.o -o plt.so
# RUN: llvm-objdump -d --no-show-raw-insn plt.so | FileCheck %s --check-prefix=PLT

# ADDTPC adds its page-scaled immediate to the exact instruction TPC, not its
# page base. The HI20/LO12 pair must therefore split S + A - P_hi. In
# particular, the low relocation must compensate for P_hi's page offset.

# ORDINARY:      <_start>:
# ORDINARY-NEXT: C.BSTART.STD
# ORDINARY-NEXT: addtpc  1048573, ->a0
# ORDINARY-NEXT: addi    a0, 4094, ->a0

# This matches the Linux setup_vm failure: a page-aligned target and a HI20 at
# page offset 0xc7a must encode a 0x386 low part, not zero.
# LINUX:      600c7a: addtpc  1047039, ->a0
# LINUX-NEXT: 600c7e: addi    a0, 902, ->a0

# GOT:      <_start>:
# GOT-NEXT: C.BSTART.STD
# GOT-NEXT: addtpc  1, ->a0
# GOT-NEXT: addi    a0, 4094, ->a0

# The synthetic PLT uses a signed unscaled load. This placement gives the low
# displacement bit 11, so the rounded HI20 must pair with -2044 and still
# address the naturally aligned GOT slot at 0x13808.
# PLT:      <.plt>:
# PLT-NEXT: BSTART.STD IND
# PLT-NEXT: addtpc  2, ->t
# PLT-NEXT: ldi.u   [t#1, -2044], ->u
# PLT-NEXT: setc.tgt u#1
# PLT-NEXT: BSTOP

#--- ordinary.s
  .text
  .globl _start
_start:
  c.bstart
  addtpc %tpcrel_hi(target), ->a0
  addi a0, %tpcrel_lo(target), ->a0

  .data
target:
  .quad 0

#--- linux.s
  .text
  .space 0xc7a
  .globl _start
_start:
  addtpc %tpcrel_hi(target), ->a0
  addi a0, %tpcrel_lo(target), ->a0

#--- linux.ld
SECTIONS {
  target = 0;
  .text 0x600000 : { *(.text) }
}

# llvm-mc does not currently parse textual @got operands, so use the same
# zero-immediate ADDTPC/ADDI carrier with the GOT relocation numbers emitted by
# Clang (R_LINX_GOT_HI20=23, R_LINX_GOT_LO12=24).
#--- got.yaml
--- !ELF
FileHeader:
  Class:   ELFCLASS64
  Data:    ELFDATA2LSB
  Type:    ET_REL
  Machine: 0xE9
Sections:
  - Name:         .text
    Type:         SHT_PROGBITS
    Flags:        [ SHF_ALLOC, SHF_EXECINSTR ]
    AddressAlign: 0x4
    Content:      00080701000015010100
  - Name:         .data
    Type:         SHT_PROGBITS
    Flags:        [ SHF_WRITE, SHF_ALLOC ]
    AddressAlign: 0x1
    Content:      '0000000000000000'
  - Name:         .note.pto.isa
    Type:         SHT_NOTE
    Flags:        [ SHF_ALLOC ]
    AddressAlign: 0x4
    Notes:
      - Name: PTO
        Desc: 7B22656E636F64696E675F616269223A2270746F2D6973612D302E35392E312D6D6F64652D66756E6374696F6E2D7631222C22656E636F64696E675F70726F6A656374696F6E5F736861323536223A2264393063383831373534643636663263643236663630313464306235383263626138383330376537303132333538336438376663353961393138323239633238222C2272656C65617365223A22302E35392E31227D
        Type: NT_VERSION
  - Name:         .rela.text
    Type:         SHT_RELA
    Flags:        [ SHF_INFO_LINK ]
    Link:         .symtab
    Info:         .text
    Relocations:
      - Offset: 0x2
        Symbol: .data
        Type:   0x17
      - Offset: 0x6
        Symbol: .data
        Type:   0x18
Symbols:
  - Name:    target
    Section: .data
  - Name:    .data
    Type:    STT_SECTION
    Section: .data
  - Name:    _start
    Section: .text
    Binding: STB_GLOBAL
...

#--- plt.ll
declare void @callee()

define void @caller() {
entry:
  call void @callee()
  ret void
}
