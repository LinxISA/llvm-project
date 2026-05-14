//===-- LinxV5MCTargetDesc.cpp - LINXV4 Target Descriptions --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file provides LINXV4-specific target descriptions.
///
//===----------------------------------------------------------------------===//

#include "LinxV5MCTargetDesc.h"
#include "LinxV5BaseInfo.h"
#include "LinxV5ELFStreamer.h"
#include "LinxV5InstPrinter.h"
#include "LinxV5MCAsmInfo.h"
#include "LinxV5MCObjectFileInfo.h"
#include "LinxV5TargetStreamer.h"
#include "TargetInfo/LinxV5TargetInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "LinxV5GenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "LinxV5GenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "LinxV5GenSubtargetInfo.inc"

using namespace llvm;

static MCInstrInfo *createLinxV5MCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitLinxV5MCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createLinxV5MCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitLinxV5MCRegisterInfo(X, LinxV5::R10);
  return X;
}

static MCAsmInfo *createLinxV5MCAsmInfo(const MCRegisterInfo &MRI,
                                        const Triple &TT,
                                        const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new LinxV5MCAsmInfo(TT);

  MCRegister SP = MRI.getDwarfRegNum(LinxV5::R1, true);
  MCCFIInstruction Inst = MCCFIInstruction::cfiDefCfa(nullptr, SP, 0);
  MAI->addInitialFrameState(Inst);

  return MAI;
}

static MCObjectFileInfo *
createLinxV5MCObjectFileInfo(MCContext &Ctx, bool PIC,
                             bool LargeCodeModel = false) {
  MCObjectFileInfo *MOFI = new LinxV5MCObjectFileInfo();
  MOFI->initMCObjectFileInfo(Ctx, PIC, LargeCodeModel);
  return MOFI;
}

static MCSubtargetInfo *
createLinxV5MCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  if (CPU.empty())
    CPU = "janus";

  return createLinxV5MCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FS);
}

static MCInstPrinter *createLinxV5MCInstPrinter(const Triple &T,
                                                unsigned SyntaxVariant,
                                                const MCAsmInfo &MAI,
                                                const MCInstrInfo &MII,
                                                const MCRegisterInfo &MRI) {
  return new LinxV5InstPrinter(MAI, MII, MRI);
}

static MCTargetStreamer *
createLinxV5ObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  const Triple &TT = STI.getTargetTriple();
  if (TT.isOSBinFormatELF())
    return new LinxV5TargetELFStreamer(S, STI);
  return nullptr;
}

static MCTargetStreamer *
createLinxV5AsmTargetStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                              MCInstPrinter *InstPrint, bool isVerboseAsm) {
  return new LinxV5TargetAsmStreamer(S, OS);
}

namespace {

class LinxV5MCInstrAnalysis : public MCInstrAnalysis {
public:
  explicit LinxV5MCInstrAnalysis(const MCInstrInfo *Info)
      : MCInstrAnalysis(Info) {}

  // Note: this function can used by objdump for print target symbol name.
  bool evaluateBranch(const MCInst &Inst, uint64_t Addr, uint64_t Size,
                      uint64_t &Target) const override {
    MCInstrDesc const &Desc = Info->get(Inst.getOpcode());
    unsigned MIFrm = LinxV5II::getFormat(Desc.TSFlags);
    if (MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_12 ||
        MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_25 ||
        MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_17 ||
        MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_29 ||
        MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_42) {
      Target = Addr + (Inst.getOperand(0).getImm());
      return true;
    } else if (Inst.getOpcode() == LinxV5::C_ADDPC ||
               Inst.getOpcode() == LinxV5::ADDPC ||
               Inst.getOpcode() == LinxV5::HL_SETRET) {
      Target = Addr + (Inst.getOperand(0).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_SIMT_JUMP) {
      Target = Addr + (Inst.getOperand(0).getImm());
      return true;
    } else if (Inst.getOpcode() == LinxV5::BTEXT) {
      Target = Addr + (Inst.getOperand(0).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_Load_Symbol) {
      Target = Addr + (Inst.getOperand(1).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_Store_Symbol) {
      Target = Addr + (Inst.getOperand(1).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_BRANCH) {
      Target = Addr + (Inst.getOperand(2).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_Load_Symbol_TARGET_42) {
      Target = Addr + (Inst.getOperand(1).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_Store_Symbol_TARGET_42) {
      Target = Addr + (Inst.getOperand(1).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_BRANCH_22) {
      Target = Addr + (Inst.getOperand(0).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_Load_Symbol_TARGET_29) {
      Target = Addr + (Inst.getOperand(1).getImm());
      return true;
    } else if (MIFrm == LinxV5II::InstFormat_Store_Symbol_TARGET_29) {
      Target = Addr + (Inst.getOperand(1).getImm());
      return true;
    }

    return false;
  }
};

} // end anonymous namespace

static MCInstrAnalysis *createLinxV5InstrAnalysis(const MCInstrInfo *Info) {
  return new LinxV5MCInstrAnalysis(Info);
}

namespace {
MCStreamer *createLinxV5ELFStreamer(const Triple &T, MCContext &Context,
                                    std::unique_ptr<MCAsmBackend> &&MAB,
                                    std::unique_ptr<MCObjectWriter> &&MOW,
                                    std::unique_ptr<MCCodeEmitter> &&MCE,
                                    bool RelaxAll) {
  std::string Err;
  const auto *Target = TargetRegistry::lookupTarget(T.str(), Err);
  std::unique_ptr<MCInstrInfo> MII(Target->createMCInstrInfo());
  return createLinxV5ELFStreamer(Context, std::move(MAB), std::move(MOW),
                                 std::move(MCE), RelaxAll);
}
} // end anonymous namespace

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLinxV5TargetMC() {
  for (Target *T : {&getTheLinx64V5Target(), &getTheLinx64V5beTarget()}) {
    TargetRegistry::RegisterMCAsmInfo(*T, createLinxV5MCAsmInfo);
    TargetRegistry::RegisterMCObjectFileInfo(*T, createLinxV5MCObjectFileInfo);
    TargetRegistry::RegisterMCInstrInfo(*T, createLinxV5MCInstrInfo);
    TargetRegistry::RegisterMCRegInfo(*T, createLinxV5MCRegisterInfo);
    TargetRegistry::RegisterMCCodeEmitter(*T, createLinxV5MCCodeEmitter);
    TargetRegistry::RegisterMCInstPrinter(*T, createLinxV5MCInstPrinter);
    TargetRegistry::RegisterMCSubtargetInfo(*T, createLinxV5MCSubtargetInfo);
    TargetRegistry::RegisterELFStreamer(*T, createLinxV5ELFStreamer);
    TargetRegistry::RegisterObjectTargetStreamer(
        *T, createLinxV5ObjectTargetStreamer);
    TargetRegistry::RegisterMCInstrAnalysis(*T, createLinxV5InstrAnalysis);

    // Register the asm target streamer.
    TargetRegistry::RegisterAsmTargetStreamer(*T,
                                              createLinxV5AsmTargetStreamer);
  }

  TargetRegistry::RegisterMCAsmBackend(getTheLinx64V5Target(),
                                       createLinxV5AsmBackend);
  TargetRegistry::RegisterMCAsmBackend(getTheLinx64V5beTarget(),
                                       createLinxV5beAsmBackend);
}
