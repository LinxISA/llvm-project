//===-- LinxV5ELFStreamer.cpp - LinxV5 ELF Target Streamer Methods -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides LinxV5 specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "LinxV5ELFStreamer.h"
#include "LinxV5AsmBackend.h"
#include "LinxV5BaseInfo.h"
#include "LinxV5MCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/LinxV5Attributes.h"

using namespace llvm;

// This part is for ELF object output.
LinxV5TargetELFStreamer::LinxV5TargetELFStreamer(MCStreamer &S,
                                                 const MCSubtargetInfo &STI)
    : LinxV5TargetStreamer(S), CurrentVendor("LinxV5"), STI(STI) {
  MCAssembler &MCA = getStreamer().getAssembler();
  const FeatureBitset &Features = STI.getFeatureBits();
  auto &MAB = static_cast<LinxV5AsmBackend &>(MCA.getBackend());
  setTargetABI(LinxV5ABI::computeTargetABI(
      STI.getTargetTriple(), Features, MAB.getTargetOptions().getABIName()));
}

MCELFStreamer &LinxV5TargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}

void LinxV5TargetELFStreamer::emitDirectiveOptionRelax() {}
void LinxV5TargetELFStreamer::emitDirectiveOptionNoRelax() {}

void LinxV5TargetELFStreamer::emitAttribute(unsigned Attribute,
                                            unsigned Value) {
  setAttributeItem(Attribute, Value, /*OverwriteExisting=*/true);
}

void LinxV5TargetELFStreamer::emitTextAttribute(unsigned Attribute,
                                                StringRef String) {
  setAttributeItem(Attribute, String, /*OverwriteExisting=*/true);
}

void LinxV5TargetELFStreamer::emitRawText(StringRef Str) { }


void LinxV5TargetELFStreamer::finishAttributeSection() {
  if (Contents.empty())
    return;

  assert(0 && "TODO: Support attritube section!");
}

void LinxV5TargetELFStreamer::finish() {
  LinxV5TargetStreamer::finish();
  MCAssembler &MCA = getStreamer().getAssembler();
  LinxV5ABI::ABI ABI = getTargetABI();

  unsigned EFlags = MCA.getELFHeaderEFlags();

  // for now, LinxV5 only define ABI = LP64
  switch (ABI) {
  case LinxV5ABI::ABI_LP64:
    break;
  case LinxV5ABI::ABI_Unknown:
    llvm_unreachable("Improperly initialised target ABI");
  }

  MCA.setELFHeaderEFlags(EFlags);
}

void LinxV5TargetELFStreamer::reset() {
  AttributeSection = nullptr;
  Contents.clear();
}

namespace {
class LinxV5ELFStreamer : public MCELFStreamer {

  static std::pair<unsigned, unsigned> getRelocPairForSize(unsigned Size) {
    switch (Size) {
    default:
      llvm_unreachable("unsupported fixup size");
    case 1:
      return std::make_pair(LinxV5::fixup_linxv5_add_8,
                            LinxV5::fixup_linxv5_sub_8);
    case 2:
      return std::make_pair(LinxV5::fixup_linxv5_add_16,
                            LinxV5::fixup_linxv5_sub_16);
    case 4:
      return std::make_pair(LinxV5::fixup_linxv5_add_32,
                            LinxV5::fixup_linxv5_sub_32);
    case 8:
      return std::make_pair(LinxV5::fixup_linxv5_add_64,
                            LinxV5::fixup_linxv5_sub_64);
    }
  }

  static bool requiresFixups(MCContext &C, const MCExpr *Value,
                             const MCExpr *&LHS, const MCExpr *&RHS) {
    const auto *MBE = dyn_cast<MCBinaryExpr>(Value);
    if (MBE == nullptr)
      return false;

    MCValue E;
    if (!Value->evaluateAsRelocatable(E, nullptr, nullptr))
      return false;
    if (E.getSymA() == nullptr || E.getSymB() == nullptr)
      return false;

    const auto &A = E.getSymA()->getSymbol();
    const auto &B = E.getSymB()->getSymbol();

    LHS =
        MCBinaryExpr::create(MCBinaryExpr::Add, MCSymbolRefExpr::create(&A, C),
                             MCConstantExpr::create(E.getConstant(), C), C);
    RHS = E.getSymB();

    // TODO: Handle `-fprofile-instr-generate` ref
    // https://reviews.llvm.org/D127549 if we need.
    return (A.isInSection() ? A.getSection().hasInstructions()
                            : !A.getName().empty()) ||
           (B.isInSection() ? B.getSection().hasInstructions()
                            : !B.getName().empty());
  }

  void reset() override {
    static_cast<LinxV5TargetStreamer *>(getTargetStreamer())->reset();
    MCELFStreamer::reset();
  }

public:
  LinxV5ELFStreamer(MCContext &C, std::unique_ptr<MCAsmBackend> MAB,
                    std::unique_ptr<MCObjectWriter> MOW,
                    std::unique_ptr<MCCodeEmitter> MCE)
      : MCELFStreamer(C, std::move(MAB), std::move(MOW), std::move(MCE)) {}

  void emitValueImpl(const MCExpr *Value, unsigned Size, SMLoc Loc) override {
    const MCExpr *A, *B;
    if (!requiresFixups(getContext(), Value, A, B))
      return MCELFStreamer::emitValueImpl(Value, Size, Loc);

    MCStreamer::emitValueImpl(Value, Size, Loc);

    MCDataFragment *DF = getOrCreateDataFragment();
    flushPendingLabels(DF, DF->getContents().size());
    MCDwarfLineEntry::make(this, getCurrentSectionOnly());

    unsigned Add, Sub;
    std::tie(Add, Sub) = getRelocPairForSize(Size);

    DF->getFixups().push_back(MCFixup::create(
        DF->getContents().size(), A, static_cast<MCFixupKind>(Add), Loc));
    DF->getFixups().push_back(MCFixup::create(
        DF->getContents().size(), B, static_cast<MCFixupKind>(Sub), Loc));

    DF->getContents().resize(DF->getContents().size() + Size, 0);
  }
};

} // namespace

namespace llvm {
MCELFStreamer *createLinxV5ELFStreamer(MCContext &C,
                                       std::unique_ptr<MCAsmBackend> MAB,
                                       std::unique_ptr<MCObjectWriter> MOW,
                                       std::unique_ptr<MCCodeEmitter> MCE,
                                       bool RelaxAll) {
  LinxV5ELFStreamer *S =
      new LinxV5ELFStreamer(C, std::move(MAB), std::move(MOW), std::move(MCE));
  S->getAssembler().setRelaxAll(RelaxAll);
  return S;
}
} // namespace llvm
