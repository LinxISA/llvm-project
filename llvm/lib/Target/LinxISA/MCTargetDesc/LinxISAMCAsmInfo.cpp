#include "MCTargetDesc/LinxISAMCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

LinxISAMCAsmInfo::LinxISAMCAsmInfo(const Triple &TT, bool Is64Bit) {
  IsLittleEndian = true;
  CodePointerSize = Is64Bit ? 8 : 4;
  CalleeSaveStackSlotSize = Is64Bit ? 8 : 4;
  // Use "# " so tokens like `t#1` remain lexable (the lexer treats a single
  // '#' comment string as starting a comment anywhere in the line).
  CommentString = "# ";
  Data16bitsDirective = "\t.half\t";
  Data32bitsDirective = "\t.word\t";
  Data64bitsDirective = "\t.dword\t";

  // TODO: fill in ELF/ABI details once the LinxISA ABI is defined.
}

void LinxISAMCAsmInfo::printSpecifierExpr(raw_ostream &OS,
                                         const MCSpecifierExpr &Expr) const {
  if (Expr.getSpecifier() == LinxISA::S_None) {
    printExpr(OS, *Expr.getSubExpr());
    return;
  }

  switch (Expr.getSpecifier()) {
  case LinxISA::S_PLT: {
    const MCExpr *Sub = Expr.getSubExpr();
    const bool NeedParens = Sub->getKind() == MCExpr::Binary ||
                            Sub->getKind() == MCExpr::Unary;
    if (NeedParens)
      OS << '(';
    printExpr(OS, *Sub);
    if (NeedParens)
      OS << ')';
    OS << "@plt";
    return;
  }
  case LinxISA::S_GOT: {
    const MCExpr *Sub = Expr.getSubExpr();
    const bool NeedParens = Sub->getKind() == MCExpr::Binary ||
                            Sub->getKind() == MCExpr::Unary;
    if (NeedParens)
      OS << '(';
    printExpr(OS, *Sub);
    if (NeedParens)
      OS << ')';
    OS << "@got";
    return;
  }
  default:
    llvm_unreachable("Invalid LinxISA specifier");
  }
}
