// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

// PTO ISA 0.58.4 ADR-0101:
//   bit 7 = TransA, bit 8 = TransB, bit 9 = CScaleEn, bit 10 reserved.
.text
.byte 0xa3, 0x20, 0x00, 0x00 // TransA=1
.byte 0x23, 0x21, 0x00, 0x00 // TransB=1
.byte 0x23, 0x22, 0x00, 0x00 // CScaleEn=1
.byte 0x23, 0x24, 0x00, 0x00 // reserved bit 10

// CHECK: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=1, TransB=0, CScaleEn=0
// CHECK: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=1, CScaleEn=0
// CHECK: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=1
// CHECK: <unknown>
