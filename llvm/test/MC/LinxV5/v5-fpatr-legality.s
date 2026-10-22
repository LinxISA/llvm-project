// PTO 0.58.1 B.FPATR field/combo legality (Work Package P0-2).
// Legal values/combinations assemble; illegal ones are rejected by the
// AsmParser without reaching the encoder.
// RUN: llvm-mc %s --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s --check-prefix=POS

// POS: B.FPATR PreQuantMode=QF322B8_PRE, ReluMode=ScalarLReLU, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
B.FPATR 24, 2, 0, 0, 0, 0, 0, 0, 0, 0
// POS: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=16, RowMaxEn=1, GroupMaxEn=1, RowMaxInit=1, MaxAbsEn=1, TransA=0, TransB=0, CScaleEn=0
B.FPATR 0, 0, 2, 1, 1, 1, 1, 0, 0, 0
// POS: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=128, RowMaxEn=1, GroupMaxEn=1, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
B.FPATR 0, 0, 9, 1, 1, 0, 0, 0, 0, 0
// POS: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=1, TransB=1, CScaleEn=0
B.FPATR 0, 0, 0, 0, 0, 0, 0, 1, 1, 0
// POS: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=1
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
