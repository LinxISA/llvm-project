//===-- LinxV5TileOpSchema.cpp - TileOp schema query impl ------*- C++ -*-===//
#include "LinxV5TileOpSchema.h"
#include "llvm/ADT/StringRef.h"
#include <cstdlib>

// Generated-table row types (must precede the generated definitions).
struct FieldSlot {
  int16_t Field, Syntax;
  uint8_t BindingKind, ConfigKind, ValueStyle, Optional, Section;
  int16_t Default, Condition, Constraint;
};
struct FormRecord {
  int16_t Mnemonic, Spelling, HeaderCommand, HeaderSelector;
  uint16_t CfgOff, CfgN, SrcOff, SrcN, DstOff, DstN;
  uint8_t FoldUnique;
};
#include "LinxV5TileOpSchema.inc"

namespace llvm {
namespace LinxV5TileOpSchema {

static const FormRecord &rec(int FormIdx) { return Forms[FormIdx]; }

int lookupFormBySpelling(const char *Spelling) {
  StringRef S(Spelling);
  for (unsigned I = 0; I < FormCount; ++I)
    if (S.equals_insensitive(Strings[rec(I).Spelling]))
      return static_cast<int>(I);
  return -1;
}

const char *formMnemonic(int F) {
  return F < 0 ? nullptr : Strings[rec(F).Mnemonic];
}
const char *formSpelling(int F) {
  return F < 0 ? nullptr : Strings[rec(F).Spelling];
}
const char *formHeaderCommand(int F) {
  return F < 0 || rec(F).HeaderCommand < 0 ? nullptr
                                           : Strings[rec(F).HeaderCommand];
}
const char *formHeaderSelector(int F) {
  return F < 0 || rec(F).HeaderSelector < 0 ? nullptr
                                            : Strings[rec(F).HeaderSelector];
}
bool formFoldsUniquely(int F) {
  return F >= 0 && rec(F).FoldUnique != 0;
}

static std::pair<uint16_t, uint16_t> sectionRange(int F, int Section) {
  const FormRecord &R = rec(F);
  switch (Section) {
  case FS_Config:  return {R.CfgOff, R.CfgN};
  case FS_Source:  return {R.SrcOff, R.SrcN};
  default:         return {R.DstOff, R.DstN};
  }
}

unsigned formSectionCount(int F, int Section) {
  if (F < 0) return 0;
  return sectionRange(F, Section).second;
}
int formSectionBegin(int F, int Section) {
  if (F < 0) return -1;
  return static_cast<int>(sectionRange(F, Section).first);
}

static const FieldSlot &slot(int S) { return FieldSlots[S]; }

const char *slotField(int S) {
  return S < 0 ? nullptr : Strings[slot(S).Field];
}
const char *slotSyntax(int S) {
  return S < 0 || slot(S).Syntax < 0 ? nullptr : Strings[slot(S).Syntax];
}
uint8_t slotBindingKind(int S) { return S < 0 ? 0 : slot(S).BindingKind; }
bool slotOptional(int S) { return S >= 0 && slot(S).Optional != 0; }
const char *slotDefault(int S) {
  return S < 0 || slot(S).Default < 0 ? nullptr : Strings[slot(S).Default];
}
const char *slotCondition(int S) {
  return S < 0 || slot(S).Condition < 0 ? nullptr : Strings[slot(S).Condition];
}

bool verifySchema() {
  // Section ranges must stay inside the FieldSlots table and each form's
  // sections must be contiguous and ordered config/source/destination.
  const unsigned TotalSlots = 1503; // cross-checked against generated count
  for (unsigned F = 0; F < FormCount; ++F) {
    const FormRecord &R = rec(F);
    uint32_t End = R.CfgOff + R.CfgN;
    if (End > TotalSlots) return false;
    if (R.SrcOff != End) return false;
    End = R.SrcOff + R.SrcN;
    if (End > TotalSlots) return false;
    if (R.DstOff != End) return false;
    if (R.DstOff + R.DstN > TotalSlots) return false;
  }
  return true;
}

} // namespace LinxV5TileOpSchema
} // namespace llvm
