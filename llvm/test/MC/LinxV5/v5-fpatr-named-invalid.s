// RUN: not llvm-mc -triple=linx64v5 -show-encoding %s 2>&1 | FileCheck %s

B.FPATR UnknownField=0
// CHECK: error: unknown B.FPATR field

B.FPATR PreQuantMode=NoQuant, PreQuantMode=NoQuant
// CHECK: error: duplicate B.FPATR field

B.FPATR GroupN=0, GroupNCode=0
// CHECK: error: duplicate B.FPATR field

B.FPATR GroupNCode=0, GroupN=0
// CHECK: error: duplicate B.FPATR field

B.FPATR PreQuantMode=NoQuant
// CHECK: error: missing B.FPATR field 'ReluMode'

B.FPATR PreQuantMode=NoQuant, ReluMode None
// CHECK: error: expected '=' after B.FPATR field

B.FPATR PreQuantMode=NoQuant ReluMode=None
// CHECK: error: expected ',' between B.FPATR fields

B.FPATR PreQuantMode=NoQuant,
// CHECK: error: expected a B.FPATR field name

B.FPATR PreQuantMode=NoQuant, 0
// CHECK: error: expected a B.FPATR field name

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=3, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: invalid B.FPATR GroupN column count

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=-1, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: invalid B.FPATR GroupN column count

B.FPATR PreQuantMode=6, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Invalid!

B.FPATR PreQuantMode=NoQuant, ReluMode=4, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Invalid!

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=1, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Invalid!

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=1, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Invalid!

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=32, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Invalid!

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=1, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Invalid!

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=2, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Error!

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=-1, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Error!

B.FPATR PreQuantMode=64, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Error!

B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupNCode=10, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
// CHECK: error: Match Instruction Invalid!

BSTART.CUBE TGEMVMX.ACC, FP32
B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=1
// CHECK: error: CScaleEn must be 0 for TGEMV CUBE operations
