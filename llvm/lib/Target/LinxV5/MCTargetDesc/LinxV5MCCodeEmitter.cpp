//===-- LinxV5MCCodeEmitter.cpp - Convert LinxV5 code to machine code ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the LinxV5MCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "MCTargetDesc/LinxV5CompressInst.h"
#include "MCTargetDesc/LinxV5FixupKinds.h"
#include "MCTargetDesc/LinxV5MCExpr.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "MCTargetDesc/LinxV5TileOpExpand.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::opt<cl::boolOrDefault>
    EnableLinkerInstRelax("linxv5-enable-linker-inst-relax",
                          cl::desc("Inst Linker Relax"), cl::Hidden);

static bool boolVal(cl::boolOrDefault Val) { return Val == cl::BOU_TRUE; }

static bool enableLinkerInstRelax(const MCSubtargetInfo &STI) {
  if (EnableLinkerInstRelax == cl::BOU_UNSET)
    return STI.getCPU().str() != "v0.43w";
  else
    return boolVal(EnableLinkerInstRelax);
}

#define DEBUG_TYPE "mccodeemitter"

STATISTIC(MCNumEmitted, "Number of MC instructions emitted");
STATISTIC(MCNumFixups, "Number of MC fixups created");

namespace {
class LinxV5MCCodeEmitter : public MCCodeEmitter {
  LinxV5MCCodeEmitter(const LinxV5MCCodeEmitter &) = delete;
  void operator=(const LinxV5MCCodeEmitter &) = delete;
  MCContext &Ctx;
  MCInstrInfo const &MCII;

public:
  LinxV5MCCodeEmitter(MCContext &ctx, MCInstrInfo const &MCII)
      : Ctx(ctx), MCII(MCII) {}

  ~LinxV5MCCodeEmitter() override = default;

  void encodeInstruction(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

  /// TableGen'erated function for getting the binary encoding for an
  /// instruction.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  void expandPseudoMCall(const MCInst &MI, raw_ostream &OS,
                                  SmallVectorImpl<MCFixup> &Fixups,
                                  const MCSubtargetInfo &STI) const;

  void expandPseudoTCOPY(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

  void expandPseudoEmptyTile(const MCInst &MI, raw_ostream &OS,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  void expandPseudoTLoadStore(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

  void expandPseudoVCall(const MCInst &MI, raw_ostream &OS,
                                  SmallVectorImpl<MCFixup> &Fixups,
                                  const MCSubtargetInfo &STI) const;

  void expandPseudoCCall(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;
  void expandPseudoV5TLSU(const MCInst &MI, raw_ostream &OS,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &STI) const;

  void writeBinaryCodes(raw_ostream &OS, SmallVectorImpl<MCFixup> &Fixups,
                        const MCSubtargetInfo &STI,
                        llvm::SmallVector<MCInst> McVec, unsigned &Count,
                        bool isNeedFixUp = false) const;

  /// Return binary encoding of operand. If the machine operand requires
  /// relocation, record the relocation and return zero.
  unsigned getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getImmOpValueTileSize(const MCInst &MI, unsigned OpNo,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  unsigned getImmOpValuePE_MASK(const MCInst &MI, unsigned OpNo,
                                SmallVectorImpl<MCFixup> &Fixups,
                                const MCSubtargetInfo &STI) const;

  unsigned getImmOpValueTSize(const MCInst &MI, unsigned OpNo,
                              SmallVectorImpl<MCFixup> &Fixups,
                              const MCSubtargetInfo &STI) const;

  unsigned getImmOpValueSharedTID(const MCInst &MI, unsigned OpNo,
                                  SmallVectorImpl<MCFixup> &Fixups,
                                  const MCSubtargetInfo &STI) const;

  unsigned getImmOpValueAsr1(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getImmOpValueAsr3(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getImmOpValueMinus1(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  unsigned getImmOpValue(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

  template <unsigned N>
  unsigned getSImmShiftN(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const {
    return getImmShiftN<true, N>(MI, OpNo, Fixups, STI);
  };

  template <unsigned N>
  unsigned getUImmShiftN(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const {
    return getImmShiftN<false, N>(MI, OpNo, Fixups, STI);
  };

  template <bool Signed, unsigned N>
  unsigned getImmShiftN(const MCInst &MI, unsigned OpNo,
                        SmallVectorImpl<MCFixup> &Fixups,
                        const MCSubtargetInfo &STI) const;

  unsigned encodingSIMTReg(const MCInst &MI, unsigned OpNo,
                           SmallVectorImpl<MCFixup> &Fixups,
                           const MCSubtargetInfo &STI) const;

  MCRegister trans2SIMTRegIfScalar(MCRegister Reg) const;
};
} // end anonymous namespace

static llvm::SmallVector<MCInst>
compressMCInstVec(llvm::SmallVector<MCInst> McVec, const MCSubtargetInfo &STI,
                  MCContext &Ctx) {
  for (MCInst &I : McVec) {
    MCInst CInst;
    if (llvm::LinxV5::tryCompressInst(CInst, I, STI, Ctx))
      I = CInst;
  }
  return McVec;
}

MCCodeEmitter *llvm::createLinxV5MCCodeEmitter(const MCInstrInfo &MCII,
                                               MCContext &Ctx) {
  return new LinxV5MCCodeEmitter(Ctx, MCII);
}

// TODO: Consider compress inst, and big/litter-end here.
void LinxV5MCCodeEmitter::encodeInstruction(const MCInst &MI, raw_ostream &OS,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  const MCInstrDesc &Desc = MCII.get(MI.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  if (MI.getOpcode() == LinxV5::PseudoV5GMOV ||
      MI.getOpcode() == LinxV5::PseudoV5SharedL2S ||
      MI.getOpcode() == LinxV5::PseudoV5SharedS2L) {
    expandPseudoV5TLSU(MI, OS, Fixups, STI);
    return;
  }
  if (LinxV5II::isTileOp(TSFlags)) {
    if (LinxV5II::isTileOpAtVEC(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
      expandPseudoVCall(MI, OS, Fixups, STI);
      return;
    }

    if (LinxV5II::isTileOpAtMTC(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
      expandPseudoMCall(MI, OS, Fixups, STI);
      return;
    }

    if (MI.getOpcode() == LinxV5::PseudoTCOPY) {
      expandPseudoTCOPY(MI, OS, Fixups, STI);
      return;
    }

    if (MI.getOpcode() == LinxV5::PseudoEmptyTile) {
      expandPseudoEmptyTile(MI, OS, Fixups, STI);
      return;
    }

    if (LinxV5II::isTileOpAtCUBE(TSFlags) && LinxV5II::isHeaderOnly(TSFlags)) {
      expandPseudoCCall(MI, OS, Fixups, STI);
      return;
    }

    // TCOPY instruction is not processed here; it is separately excluded in the preceding PseudoTCOPY conditions.
    if (LinxV5II::isTileOpAtMTC(TSFlags) && LinxV5II::isHeaderOnly(TSFlags)) {
      expandPseudoTLoadStore(MI, OS, Fixups, STI);
      return;
    }

    llvm::errs() << MI;
    assert(0 && "All TileOp need Expand!");
  }

  if (MI.getOpcode() == LinxV5::SIMT_ADDI_SCAR &&
      MI.getOperand(0).getReg() == LinxV5::SIMT_P) {
    auto Inst = MCInstBuilder(LinxV5::SIMT_ADDI_SCAR_P)
                    .addOperand(MI.getOperand(0))
                    .addOperand(MI.getOperand(1))
                    .addOperand(MI.getOperand(2))
                    .addOperand(MI.getOperand(3))
                    .addOperand(MI.getOperand(4));
    uint64_t Bits = getBinaryCodeForInstr(Inst, Fixups, STI);
    support::endian::write(OS, Bits, support::little);
    ++MCNumEmitted;
    return;
  }

  if (MI.getOpcode() == LinxV5::SIMT_ORI_SCAR &&
      MI.getOperand(0).getReg() == LinxV5::SIMT_P) {
    auto Inst = MCInstBuilder(LinxV5::SIMT_ORI_SCAR_P)
                    .addOperand(MI.getOperand(0))
                    .addOperand(MI.getOperand(1))
                    .addOperand(MI.getOperand(2))
                    .addOperand(MI.getOperand(3))
                    .addOperand(MI.getOperand(4));
    uint64_t Bits = getBinaryCodeForInstr(Inst, Fixups, STI);
    support::endian::write(OS, Bits, support::little);
    ++MCNumEmitted;
    return;
  }

  if (MI.getOpcode() == LinxV5::SIMT_XORI_SCAR &&
      MI.getOperand(0).getReg() == LinxV5::SIMT_P) {
    auto Inst = MCInstBuilder(LinxV5::SIMT_XORI_SCAR_P)
                    .addOperand(MI.getOperand(0))
                    .addOperand(MI.getOperand(1))
                    .addOperand(MI.getOperand(2))
                    .addOperand(MI.getOperand(3))
                    .addOperand(MI.getOperand(4));
    uint64_t Bits = getBinaryCodeForInstr(Inst, Fixups, STI);
    support::endian::write(OS, Bits, support::little);
    ++MCNumEmitted;
    return;
  }

  // Get byte count of instruction.
  unsigned Size = Desc.getSize();

  switch (Size) {
  default:
    llvm_unreachable("Unhandled encodeInstruction length!");
  case 2: {
    uint16_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
    support::endian::write<uint16_t>(OS, Bits, support::little);
    break;
  }
  case 4: {
    uint32_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
    support::endian::write(OS, Bits, support::little);
    break;
  }
  case 6: {
    uint64_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
    uint32_t BitsLow = Bits & 0xFFFFFFFF;
    uint16_t BitsHigh = (Bits >> 32) & 0xFFFF;
    support::endian::write<uint32_t>(OS, BitsLow, support::little);
    support::endian::write<uint16_t>(OS, BitsHigh, support::little);
    break;
  }
  case 8: {
    uint64_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
    support::endian::write(OS, Bits, support::little);
    break;
  }
  }

  ++MCNumEmitted; // Keep track of the # of mi's emitted.
}

static bool isCompressedOpcode(unsigned Opcode) {
  switch (Opcode) {
  default:
    return false;

  // compressed / short-form opcodes
  case LinxV5::MOVR:
  case LinxV5::MOVI:

  case LinxV5::SEXT_B:
  case LinxV5::SEXT_H:
  case LinxV5::SEXT_W:
  case LinxV5::ZEXT_B:
  case LinxV5::ZEXT_H:
  case LinxV5::ZEXT_W:

  case LinxV5::C_ADD:
  case LinxV5::C_ADDI:
  case LinxV5::C_SUB:
  case LinxV5::C_AND:
  case LinxV5::C_OR:

  case LinxV5::C_SETC_EQ:
  case LinxV5::C_SETC_NE:
  case LinxV5::C_SETC_TGT:

  case LinxV5::C_LWI:
  case LinxV5::C_LDI:
  case LinxV5::C_SWI:
  case LinxV5::C_SDI:

  case LinxV5::C_CMP_EQI:
  case LinxV5::C_CMP_NEI:

  case LinxV5::C_SLLI:
  case LinxV5::C_SRLI:

  case LinxV5::BSTART_STD_WITHOUT_TARGET_16:
  case LinxV5::BSTART_AUX_WITHOUT_TARGET_16:
  case LinxV5::BSTART_FP_WITHOUT_TARGET_16:

  case LinxV5::C_B_DIMI:
    return true;
  }
}

void LinxV5MCCodeEmitter::writeBinaryCodes(raw_ostream &OS,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI,
                                           llvm::SmallVector<MCInst> McVec,
                                           unsigned &Count, bool isNeedFixUp) const {
  for (MCInst &inst : McVec) {
    MCInst out;
    // if (llvm::LinxV5::tryCompressInst(out, inst, STI, this->Ctx)) {
    //   uint16_t Bits = getBinaryCodeForInstr(out, Fixups, STI);
    //   if (isNeedFixUp && !Fixups.empty()) {
    //     Fixups.back().setOffset(Count);
    //   }
    //   support::endian::write<uint16_t>(OS, Bits, support::little);
    //   // 16bits inst = 2 byte
    //   Count += 2;
    //   continue;
    // }
    uint32_t Bits = getBinaryCodeForInstr(inst, Fixups, STI);
    if (isNeedFixUp && !Fixups.empty()) {
      if (Fixups.back().getTargetKind() == llvm::LinxV5::fixup_linxv5_relax &&
          Fixups.size() >= 2 &&
          Fixups[Fixups.size() - 2].getTargetKind() ==
              llvm::LinxV5::fixup_linxv5_stack_size) {
        Fixups[Fixups.size() - 2].setOffset(Count);
        isNeedFixUp = false;
      }
      Fixups.back().setOffset(Count);
    }

    if (isCompressedOpcode(inst.getOpcode())) {
      support::endian::write<uint16_t>(
          OS, static_cast<uint16_t>(Bits), support::little);
      Count += 2;
    } else {
      support::endian::write<uint32_t>(OS, Bits, support::little);
    // 32bits inst = 4 byte
      Count += 4;
    }
  }
}

void LinxV5MCCodeEmitter::expandPseudoVCall(
    const MCInst &MI, raw_ostream &OS, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  unsigned ByteCount = 0;
  // bstart.par
  writeBinaryCodes(
      OS, Fixups, STI,
      {MCInstBuilder(LinxV5::BSTART_VPAR)
           .addOperand(MCOperand::createImm(LinxV5Op::TileOPMode::VS16))},
      ByteCount);
  // b.catr DR
  writeBinaryCodes(OS, Fixups, STI, getBATTRFromInst(MI, MCII), ByteCount);
  // b.iot
  writeBinaryCodes(OS, Fixups, STI, getBIOTFromInst(MI, MCII), ByteCount, true);
  // b.ior
  writeBinaryCodes(OS, Fixups, STI, getBIORFromInst(MI, MCII), ByteCount);
  // b.dim
  writeBinaryCodes(OS, Fixups, STI,
                   compressMCInstVec(getBDIMFromInst(MI, MCII), STI, Ctx),
                   ByteCount);
  // b.iod
  writeBinaryCodes(OS, Fixups, STI, getBIODFromInst(MI, MCII), ByteCount);
  // b.text
  writeBinaryCodes(OS, Fixups, STI, getBTEXTTFromInst(MI, MCII), ByteCount, true);
}

void LinxV5MCCodeEmitter::expandPseudoMCall(
    const MCInst &MI, raw_ostream &OS, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  unsigned ByteCount = 0;
  // bstart.par
  writeBinaryCodes(
      OS, Fixups, STI,
      {MCInstBuilder(LinxV5::BSTART_MPAR)
           .addOperand(MCOperand::createImm(LinxV5Op::TileOPMode::VS16))},
      ByteCount);
  // b.catr DR
  writeBinaryCodes(OS, Fixups, STI, getBATTRFromInst(MI, MCII), ByteCount);
  // b.iot
  writeBinaryCodes(OS, Fixups, STI, getBIOTFromInst(MI, MCII), ByteCount, true);
  // b.ior
  writeBinaryCodes(OS, Fixups, STI, getBIORFromInst(MI, MCII), ByteCount);
  // b.dim
  writeBinaryCodes(OS, Fixups, STI,
                   compressMCInstVec(getBDIMFromInst(MI, MCII), STI, Ctx),
                   ByteCount);
  // b.iod
  writeBinaryCodes(OS, Fixups, STI, getBIODFromInst(MI, MCII), ByteCount);
  // b.text
  writeBinaryCodes(OS, Fixups, STI, getBTEXTTFromInst(MI, MCII), ByteCount, true);
}

void LinxV5MCCodeEmitter::expandPseudoTCOPY(const MCInst &MI, raw_ostream &OS,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  unsigned ByteCount = 0;
  // bstart.par
  writeBinaryCodes(
      OS, Fixups, STI,
      {MCInstBuilder(LinxV5::BSTART_TMA)
           .addOperand(
               MCOperand::createImm(llvm::LinxV5Op::DataType::EMPTY_DataType))
           .addOperand(MCOperand::createImm(LinxV5Op::TileOPTMA::TMOV))},
      ByteCount);
  // b.iot
  writeBinaryCodes(OS, Fixups, STI, getBIOTFromInst(MI, MCII), ByteCount);
}

void LinxV5MCCodeEmitter::expandPseudoV5TLSU(
    const MCInst &MI, raw_ostream &OS, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  unsigned ByteCount = 0;
  if (MI.getOpcode() == LinxV5::PseudoV5GMOV) {
    writeBinaryCodes(
        OS, Fixups, STI,
        {MCInstBuilder(LinxV5::BSTART_TMA)
             .addOperand(MI.getOperand(1))
             .addOperand(MCOperand::createImm(LinxV5Op::TileOPTMA::GMOV))},
        ByteCount);
    writeBinaryCodes(
        OS, Fixups, STI,
        {MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
             .addOperand(MI.getOperand(0))
             .addOperand(MI.getOperand(2))
             .addOperand(MI.getOperand(3))
             .addOperand(MCOperand::createImm(1))
             .addOperand(MI.getOperand(5))},
        ByteCount);
    writeBinaryCodes(
        OS, Fixups, STI,
        {MCInstBuilder(LinxV5::B_IO)
             .addOperand(MCOperand::createReg(LinxV5::R0))
             .addOperand(MI.getOperand(4))
             .addOperand(MCOperand::createReg(LinxV5::R0))
             .addOperand(MCOperand::createReg(LinxV5::R0))},
        ByteCount);
    return;
  }

  if (MI.getOpcode() == LinxV5::PseudoV5SharedL2S) {
    writeBinaryCodes(
        OS, Fixups, STI,
        {MCInstBuilder(LinxV5::BSTART_TMA)
             .addOperand(MI.getOperand(2))
             .addOperand(MI.getOperand(1))},
        ByteCount);
    // PTO v0.58 reissue: destination B.IOS (mask, ->S<id><size>).
    writeBinaryCodes(
        OS, Fixups, STI,
        {MCInstBuilder(LinxV5::B_IOS)
             .addOperand(MCOperand::createImm(
                 Ctx.getRegisterInfo()->getEncodingValue(
                     MI.getOperand(0).getReg())))  // SharedTID
             .addOperand(MI.getOperand(3))         // PE_MASK
             .addOperand(MI.getOperand(4))},       // TSize
        ByteCount);
    writeBinaryCodes(
        OS, Fixups, STI,
        {MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
             .addOperand(MI.getOperand(3))
             .addOperand(MCOperand::createImm(1))
             .addOperand(MI.getOperand(5))},
        ByteCount);
    return;
  }
  writeBinaryCodes(
      OS, Fixups, STI,
      {MCInstBuilder(LinxV5::BSTART_TMA)
           .addOperand(MI.getOperand(2))
           .addOperand(MI.getOperand(1))},
      ByteCount);
  // PTO v0.58 reissue: source B.IOS (S<id>, mask=...).
  writeBinaryCodes(OS, Fixups, STI,
                   {MCInstBuilder(LinxV5::B_IOS)
                        .addOperand(MCOperand::createImm(
                            Ctx.getRegisterInfo()->getEncodingValue(
                                MI.getOperand(3).getReg())))  // SharedTID
                        .addOperand(MI.getOperand(4))          // PE_MASK
                        .addOperand(MCOperand::createImm(0))}, // TSize=0
                   ByteCount);
  writeBinaryCodes(
      OS, Fixups, STI,
      {MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
           .addOperand(MI.getOperand(0))
           .addOperand(MI.getOperand(4))
           .addOperand(MI.getOperand(5))
           .addOperand(MCOperand::createImm(1))},
      ByteCount);
}

void LinxV5MCCodeEmitter::expandPseudoEmptyTile(
    const MCInst &MI, raw_ostream &OS, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  unsigned ByteCount = 0;
  // bstart.par
  writeBinaryCodes(
      OS, Fixups, STI,
      {MCInstBuilder(LinxV5::BSTART_VPAR)
           .addOperand(MCOperand::createImm(LinxV5Op::TileOPMode::VS16))},
      ByteCount);
  // b.iot
  writeBinaryCodes(OS, Fixups, STI, getBIOTFromInst(MI, MCII), ByteCount);
}

void LinxV5MCCodeEmitter::expandPseudoTLoadStore(const MCInst &MI, raw_ostream &OS,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  unsigned Dummy = 0;
  // bstart.par
  writeBinaryCodes(OS, Fixups, STI,
                   {MCInstBuilder(LinxV5::BSTART_TMA)
                        .addOperand(MI.getOperand(7))
                        .addOperand(MCOperand::createImm(
                            getPseudoTILEOpcode(MI.getOpcode())))},
                   Dummy);
  // b.arg
  writeBinaryCodes(OS, Fixups, STI, getBARGFromInst(MI, MCII), Dummy);
  // b.iot
  writeBinaryCodes(OS, Fixups, STI, getBIOTFromInst(MI, MCII), Dummy);
  // b.ior
  writeBinaryCodes(OS, Fixups, STI, getBIORFromInst(MI, MCII), Dummy);
  // b.dim
  writeBinaryCodes(OS, Fixups, STI,
                   compressMCInstVec(getBDIMFromInst(MI, MCII), STI, Ctx),
                   Dummy);
  // b.iod
    writeBinaryCodes(OS, Fixups, STI, getBIODFromInst(MI, MCII), Dummy);
}

void LinxV5MCCodeEmitter::expandPseudoCCall(const MCInst &MI, raw_ostream &OS,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
    unsigned Dummy = 0;
    // bstart.par
    writeBinaryCodes(OS, Fixups, STI,
                     {MCInstBuilder(LinxV5::BSTART_CUBE)
                          .addOperand(MI.getOperand(7))
                          .addOperand(MCOperand::createImm(
                              getPseudoTILEOpcode(MI.getOpcode())))},
                     Dummy);
  // v5: every active Matrix CUBE bundle (TMATMUL/TMATMULMX, and TGEMV once
  // defined) carries exactly one B.FPATR after B.DATR. The predicate is the
  // whole-family isActiveMatrixPseudo; the deleted TMATMUL*_FIXP opcodes
  // (Function 9-14) are reserved/illegal and never reach here. The ten
  // immediates are synthesized as zero for now; a future change threads real
  // FPATR operands through the pseudos/ISel.
  if (isActiveMatrixPseudo(MI.getOpcode())) {
    writeBinaryCodes(OS, Fixups, STI, getBATTRFromInst(MI, MCII), Dummy);
    writeBinaryCodes(
        OS, Fixups, STI,
        {MCInstBuilder(LinxV5::B_FPATR)
             .addOperand(MCOperand::createImm(0))
             .addOperand(MCOperand::createImm(0))
             .addOperand(MCOperand::createImm(0))
             .addOperand(MCOperand::createImm(0))
             .addOperand(MCOperand::createImm(0))
             .addOperand(MCOperand::createImm(0))
             .addOperand(MCOperand::createImm(0))
             .addOperand(MCOperand::createImm(0))
             .addOperand(MCOperand::createImm(0))},
        Dummy);
  }
  if (MI.getOpcode() == LinxV5::PseudoMAMULB_SharedRight_SizeI) {
    // PTO v0.58 reissue: CUBE Shared binder is a source B.IOS (TSize=0,
    // mask=1111). operand(11)=SharedTID.
    writeBinaryCodes(
        OS, Fixups, STI,
        {MCInstBuilder(LinxV5::B_IOS)
             .addOperand(MCOperand::createImm(
                 Ctx.getRegisterInfo()->getEncodingValue(
                     MI.getOperand(11).getReg())))  // SharedTID
             .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
             .addOperand(MCOperand::createImm(0))},     // TSize=0
        Dummy);
  }
  // b.iot
  writeBinaryCodes(OS, Fixups, STI, getBIOTFromInst(MI, MCII), Dummy);
  // b.dim
  writeBinaryCodes(OS, Fixups, STI,
                   compressMCInstVec(getBDIMFromInst(MI, MCII), STI, Ctx),
                   Dummy);
}

unsigned
LinxV5MCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {

  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());

  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());

  llvm_unreachable("Unhandled expression!");
  return 0;
}

unsigned
LinxV5MCCodeEmitter::getImmOpValueTileSize(const MCInst &MI, unsigned OpNo,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());

  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());

  return getImmOpValue(MI, OpNo, Fixups, STI);
}

unsigned
LinxV5MCCodeEmitter::getImmOpValuePE_MASK(const MCInst &MI, unsigned OpNo,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  // PTO-ISA ADR 0069: the operand holds a 4-bit PE mask; [11:9] carries the
  // 3-bit PEMode. Convert the mask to its PEMode via the table; a mask with
  // no PEMode must not reach the encoder (the AsmParser rejects it).
  const MCOperand &MO = MI.getOperand(OpNo);
  unsigned Mask;
  if (MO.isReg())
    Mask = Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  else if (MO.isImm())
    Mask = static_cast<unsigned>(MO.getImm());
  else
    return getImmOpValue(MI, OpNo, Fixups, STI);
  Optional<unsigned> Mode = LinxV5PEMode::modeForMask(Mask);
  if (!Mode)
    report_fatal_error("PE mask has no PEMode encoding");
  return *Mode;
}

unsigned
LinxV5MCCodeEmitter::getImmOpValueTSize(const MCInst &MI, unsigned OpNo,
                                         SmallVectorImpl<MCFixup> &Fixups,
                                         const MCSubtargetInfo &STI) const {
  // v5 SizeCode: 4-bit immediate (0..12; 13..15 reserved by the parser).
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  if (MO.isImm())
    return static_cast<unsigned>(MO.getImm());
  return getImmOpValue(MI, OpNo, Fixups, STI);
}

unsigned LinxV5MCCodeEmitter::getImmOpValueSharedTID(
    const MCInst &MI, unsigned OpNo, SmallVectorImpl<MCFixup> &Fixups,
    const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  int64_t Value;
  if (MO.isImm())
    Value = MO.getImm();
  else if (MO.isExpr() && MO.getExpr()->evaluateAsAbsolute(Value))
    ;
  else
    report_fatal_error("SharedTID requires an absolute immediate");

  if (!isUInt<6>(Value))
    report_fatal_error("SharedTID immediate is out of range");
  return static_cast<unsigned>(Value);
}

unsigned
LinxV5MCCodeEmitter::getImmOpValueAsr1(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);

  if (MO.isImm()) {
    unsigned Res = MO.getImm();
    assert((Res & 1) == 0 && "LSB is non-zero");
    return Res >> 1;
  }

  return getImmOpValue(MI, OpNo, Fixups, STI);
}

unsigned
LinxV5MCCodeEmitter::getImmOpValueAsr3(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups,
                                       const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);

  if (MO.isImm()) {
    unsigned Res = MO.getImm();
    assert((Res & 0x7) == 0 && "LSB is non-zero");
    return Res >> 3;
  }

  return getImmOpValue(MI, OpNo, Fixups, STI);
}

template <bool Signed, unsigned N>
unsigned LinxV5MCCodeEmitter::getImmShiftN(const MCInst &MI, unsigned OpNo,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);
  if (MO.isImm()) {
    int64_t Imm = MO.getImm();
    int64_t shift, base;
    if (Signed ? isInt<N>(Imm) : isUInt<N>(Imm)) {
      shift = 0;
    } else {
      shift = llvm::countTrailingZeros((uint64_t)Imm);
      if (!isUInt<5>(shift))
        shift = 0x1f;
    }
    assert(isUInt<5>(shift) && "unexpected shifted imm");
    base = Signed ? Imm >> shift : ((uint64_t)Imm) >> shift;
    assert((Signed ? isInt<N>(base) : isUInt<N>(base)) &&
           "unexpected shifted imm");
    return (base << 5) | shift;
  }

  // encode 0
  return getImmOpValue(MI, OpNo, Fixups, STI);
}

unsigned
LinxV5MCCodeEmitter::getImmOpValueMinus1(const MCInst &MI, unsigned OpNo,
                                         SmallVectorImpl<MCFixup> &Fixups,
                                         const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);

  if (MO.isImm()) {
    unsigned Res = MO.getImm();
    assert(Res != 0 && "Imm is non-zero");
    return Res - 1;
  }

  return getImmOpValue(MI, OpNo, Fixups, STI);
}

MCRegister LinxV5MCCodeEmitter::trans2SIMTRegIfScalar(MCRegister Reg) const {
  switch (Reg) {
  default:
    break;
  case LinxV5::R0:
    return LinxV5::SIMT_ZERO;
  case LinxV5::TOS1:
  case LinxV5::TOS2:
  case LinxV5::TOS3:
  case LinxV5::TOS4:
    return LinxV5::SIMT_OST1 + Reg - LinxV5::TOS1;
  case LinxV5::UOS1:
  case LinxV5::UOS2:
  case LinxV5::UOS3:
  case LinxV5::UOS4:
    return LinxV5::SIMT_OSU1 + Reg - LinxV5::UOS1;
  case LinxV5::T:
    return LinxV5::SIMT_T;
  case LinxV5::U:
    return LinxV5::SIMT_U;
  }
  return Reg;
}

unsigned
LinxV5MCCodeEmitter::encodingSIMTReg(const MCInst &MI, unsigned OpNo,
                                     SmallVectorImpl<MCFixup> &Fixups,
                                     const MCSubtargetInfo &STI) const {
  unsigned Reg = MI.getOperand(OpNo).getReg();
  unsigned Res = Ctx.getRegisterInfo()->getEncodingValue(trans2SIMTRegIfScalar(Reg));
  return Res;
}

unsigned LinxV5MCCodeEmitter::getImmOpValue(const MCInst &MI, unsigned OpNo,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  bool EnableRelax = STI.getFeatureBits()[LinxV5::FeatureRelax];
  // TODO: The next version will support relax optimization.
  const MCOperand &MO = MI.getOperand(OpNo);

  MCInstrDesc const &Desc = MCII.get(MI.getOpcode());
  unsigned MIFrm = LinxV5II::getFormat(Desc.TSFlags);
  bool isSIMT = LinxV5II::getPEMask(Desc.TSFlags) == LinxV5II::PET_FVEC;

  // If the destination is an immediate, there is nothing to do.
  if (MO.isImm())
    return MO.getImm();

  assert(MO.isExpr() && "getImmOpValue expects only expressions or immediates");
  const MCExpr *Expr = MO.getExpr();
  MCExpr::ExprKind Kind = Expr->getKind();
  LinxV5::Fixups FixupKind = LinxV5::fixup_linxv5_invalid;
  bool RelaxCandidate = false;
  if (Kind == MCExpr::Target) {
    const LinxV5MCExpr *RVExpr = cast<LinxV5MCExpr>(Expr);
    switch (RVExpr->getKind()) {
    default:
      llvm_unreachable("Unhandled fixup kind!");
    case LinxV5MCExpr::VK_LinxV5_TPREL_HI:
      assert(!isSIMT && "Unhandled SIMT fixup kind");
      FixupKind = LinxV5::fixup_linxv5_tprel_hi20;
      break;
    case LinxV5MCExpr::VK_LinxV5_TPREL_LO:
      assert(!isSIMT && "Unhandled SIMT fixup kind");
      if (MIFrm == LinxV5II::InstFormat_ADDI)
        FixupKind = LinxV5::fixup_linxv5_tprel_lo12_i;
      else if (MIFrm == LinxV5II::InstFormat_LoadI_UnScaled)
        FixupKind = LinxV5::fixup_linxv5_tprel_lo12_l;
      else if (MIFrm == LinxV5II::InstFormat_StoreI_UnScaled)
        FixupKind = LinxV5::fixup_linxv5_tprel_lo12_s;
      break;
    case LinxV5MCExpr::VK_LinxV5_TPCREL_HI:
      assert(MI.getOpcode() == LinxV5::ADDTPC);
      FixupKind = LinxV5::fixup_linxv5_tpcrel_hi20;
      break;
    case LinxV5MCExpr::VK_LinxV5_TPCREL_HI32:
      assert(MI.getOpcode() == LinxV5::HL_ADDTPC);
      FixupKind = LinxV5::fixup_linxv5_tpcrel_hi32;
      break;
    case LinxV5MCExpr::VK_LinxV5_TPCREL_LO:
      if (MIFrm == LinxV5II::InstFormat_ADDI)
        FixupKind = isSIMT ? LinxV5::fixup_linxv5_simt_tpcrel_lo12_i
                           : LinxV5::fixup_linxv5_tpcrel_lo12_i;
      else if (MIFrm == LinxV5II::InstFormat_LoadI_UnScaled)
        FixupKind = isSIMT ? LinxV5::fixup_linxv5_simt_tpcrel_lo12_l
                           : LinxV5::fixup_linxv5_tpcrel_lo12_l;
      else if (MIFrm == LinxV5II::InstFormat_StoreI_UnScaled)
        FixupKind = isSIMT ? LinxV5::fixup_linxv5_simt_tpcrel_lo12_s
                           : LinxV5::fixup_linxv5_tpcrel_lo12_s;
      else
        llvm_unreachable(
            "VK_LinxV5_TPCREL_LO used with unexpected instruction format");
      break;
    }
  } else if (Kind == MCExpr::SymbolRef &&
             cast<MCSymbolRefExpr>(Expr)->getKind() ==
                 MCSymbolRefExpr::VK_None) {
    if (MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_25) {
      FixupKind = LinxV5::fixup_linxv5_bnext;
      RelaxCandidate = true;
    } else if (MI.getOpcode() == LinxV5::BTEXT) {
      FixupKind = LinxV5::fixup_linxv5_btext;
    } else if (MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_17) {
      FixupKind = LinxV5::fixup_linxv5_32_bnext;
    } else if (MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_29) {
      FixupKind = LinxV5::fixup_linxv5_48_bnext;
      RelaxCandidate = true;
    } else if (MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_42) {
      FixupKind = LinxV5::fixup_linxv5_64_bnext;
      RelaxCandidate = true;
    } else if (MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_12) {
      FixupKind = LinxV5::fixup_linxv5_bnext_c;
    } else if (MI.getOpcode() == LinxV5::C_ADDPC) {
      FixupKind = LinxV5::fixup_linxv5_c_addpc;
    } else if (MI.getOpcode() == LinxV5::ADDPC) {
      FixupKind = LinxV5::fixup_linxv5_addpc;
      RelaxCandidate = enableLinkerInstRelax(STI);
    } else if (MI.getOpcode() == LinxV5::HL_SETRET) {
      FixupKind = LinxV5::fixup_linxv5_hlsetret;
      RelaxCandidate = enableLinkerInstRelax(STI);
    } else if (MIFrm == LinxV5II::InstFormat_SIMT_BRANCH) {
      if (OpNo == 4) // br_label
        FixupKind = LinxV5::fixup_linxv5_simt_branch;
      else if (OpNo == 5) // rc_label
        FixupKind = LinxV5::fixup_linxv5_simt_branch_rc;
    } else if (MIFrm == LinxV5II::InstFormat_SIMT_JUMP) {
      FixupKind = LinxV5::fixup_linxv5_simt_jump;
    } else if (MIFrm == LinxV5II::InstFormat_Load_Symbol) {
      FixupKind = LinxV5::fixup_linxv5_load_symbol;
    } else if (MIFrm == LinxV5II::InstFormat_Store_Symbol) {
      FixupKind = LinxV5::fixup_linxv5_store_symbol;
    } else if (MIFrm == LinxV5II::InstFormat_BRANCH) {
      FixupKind = LinxV5::fixup_linxv5_branch;
    } else if (MIFrm == LinxV5II::InstFormat_Load_Symbol_TARGET_42) {
      FixupKind = LinxV5::fixup_linxv5_load_symbol_target_42;
    } else if (MIFrm == LinxV5II::InstFormat_Store_Symbol_TARGET_42) {
      FixupKind = LinxV5::fixup_linxv5_store_symbol_target_42;
    } else if (MIFrm == LinxV5II::InstFormat_BRANCH_22) {
      FixupKind = LinxV5::fixup_linxv5_branch_22;
    } else if (MIFrm == LinxV5II::InstFormat_Load_Symbol_TARGET_29) {
      FixupKind = LinxV5::fixup_linxv5_load_symbol_target_29;
    } else if (MIFrm == LinxV5II::InstFormat_Store_Symbol_TARGET_29) {
      FixupKind = LinxV5::fixup_linxv5_store_symbol_target_29;
    } else if (MIFrm == LinxV5II::InstFormat_STACK_SIZE) {
      FixupKind = LinxV5::fixup_linxv5_stack_size;
      RelaxCandidate = true;
    }
  }

  assert(FixupKind != LinxV5::fixup_linxv5_invalid && "Unhandled expression!");

  Fixups.push_back(
      MCFixup::create(0, Expr, MCFixupKind(FixupKind), MI.getLoc()));
  ++MCNumFixups;

  // Ensure an R_LinxV5_RELAX relocation will be emitted if linker relaxation is
  // enabled and the current fixup will result in a relocation that may be
  // relaxed.
  if (EnableRelax && RelaxCandidate) {
    const MCConstantExpr *Dummy = MCConstantExpr::create(0, Ctx);
    Fixups.push_back(MCFixup::create(
        0, Dummy, MCFixupKind(LinxV5::fixup_linxv5_relax), MI.getLoc()));
    ++MCNumFixups;
  }

  return 0;
}

#include "LinxV5GenMCCodeEmitter.inc"
