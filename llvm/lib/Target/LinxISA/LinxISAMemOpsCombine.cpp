//===-- LinxISAMemOpsCombine.cpp - Fuse HL scalar memory ops --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Pre-emit peepholes to reduce dynamic instruction count by using 48-bit HL
// memory operations:
//   - load/store pairs (HL.*IP)
//   - pre/post-index writeback (HL.*I.{PO,PR,UPO,UPR})
//
// This pass is intentionally conservative: it only fuses adjacent instructions
// and avoids volatile/atomic memory operations.
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "LinxISAInstrInfo.h"
#include "LinxISASubtarget.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include <iterator>
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "linx-memops-combine"

namespace {

class LinxISAMemOpsCombine : public MachineFunctionPass {
public:
  static char ID;
  LinxISAMemOpsCombine() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return "Linx MemOps Combine"; }
};

} // namespace

char LinxISAMemOpsCombine::ID = 0;

INITIALIZE_PASS(LinxISAMemOpsCombine, "linx-memops-combine",
                "Linx MemOps Combine", false, false)

static MachineBasicBlock::iterator nextNonDebug(MachineBasicBlock &MBB,
                                                MachineBasicBlock::iterator I) {
  auto E = MBB.end();
  if (I == E)
    return E;
  ++I;
  while (I != E && I->isDebugInstr())
    ++I;
  return I;
}

static bool isVolatileOrAtomicMem(const MachineInstr &MI) {
  for (const MachineMemOperand *MMO : MI.memoperands()) {
    if (MMO->isAtomic() || !MMO->isUnordered())
      return true;
  }
  return false;
}

static bool getPairOpc(unsigned BaseOpc, unsigned &HLOpc) {
  switch (BaseOpc) {
  case LinxISA::LWI:
    HLOpc = LinxISA::HL_LWIP;
    return true;
  case LinxISA::LWUI:
    HLOpc = LinxISA::HL_LWUIP;
    return true;
  case LinxISA::LDI:
    HLOpc = LinxISA::HL_LDIP;
    return true;
  case LinxISA::SWI:
    HLOpc = LinxISA::HL_SWIP;
    return true;
  case LinxISA::SDI:
    HLOpc = LinxISA::HL_SDIP;
    return true;
  default:
    return false;
  }
}

static bool getWritebackOpcs(unsigned MemOpc, unsigned &PostScaled,
                             unsigned &PreScaled, unsigned &PostUnscaled,
                             unsigned &PreUnscaled, unsigned &AccessBytes) {
  switch (MemOpc) {
  case LinxISA::LWI:
    PostScaled = LinxISA::HL_LWI_PO;
    PreScaled = LinxISA::HL_LWI_PR;
    PostUnscaled = LinxISA::HL_LWI_UPO;
    PreUnscaled = LinxISA::HL_LWI_UPR;
    AccessBytes = 4;
    return true;
  case LinxISA::LWUI:
    PostScaled = LinxISA::HL_LWUI_PO;
    PreScaled = LinxISA::HL_LWUI_PR;
    PostUnscaled = LinxISA::HL_LWUI_UPO;
    PreUnscaled = LinxISA::HL_LWUI_UPR;
    AccessBytes = 4;
    return true;
  case LinxISA::LDI:
    PostScaled = LinxISA::HL_LDI_PO;
    PreScaled = LinxISA::HL_LDI_PR;
    PostUnscaled = LinxISA::HL_LDI_UPO;
    PreUnscaled = LinxISA::HL_LDI_UPR;
    AccessBytes = 8;
    return true;
  case LinxISA::SWI:
    PostScaled = LinxISA::HL_SWI_PO;
    PreScaled = LinxISA::HL_SWI_PR;
    PostUnscaled = LinxISA::HL_SWI_UPO;
    PreUnscaled = LinxISA::HL_SWI_UPR;
    AccessBytes = 4;
    return true;
  case LinxISA::SDI:
    PostScaled = LinxISA::HL_SDI_PO;
    PreScaled = LinxISA::HL_SDI_PR;
    PostUnscaled = LinxISA::HL_SDI_UPO;
    PreUnscaled = LinxISA::HL_SDI_UPR;
    AccessBytes = 8;
    return true;
  default:
    return false;
  }
}

static bool isAddSubImm(unsigned Opc) {
  switch (Opc) {
  case LinxISA::ADDIri:
  case LinxISA::SUBIri:
  case LinxISA::ADDIWri:
  case LinxISA::SUBIWri:
    return true;
  default:
    return false;
  }
}

static std::optional<int64_t> getAddSubDeltaBytes(const MachineInstr &MI) {
  const unsigned Opc = MI.getOpcode();
  if (!isAddSubImm(Opc))
    return std::nullopt;
  if (MI.getNumOperands() < 3 || !MI.getOperand(2).isImm())
    return std::nullopt;
  const int64_t Imm = MI.getOperand(2).getImm();
  if (Imm == 0)
    return int64_t(0);
  const bool IsSub = (Opc == LinxISA::SUBIri || Opc == LinxISA::SUBIWri);
  return IsSub ? -Imm : Imm;
}

static bool tryCombinePair(MachineFunction &MF, MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator &I,
                           const LinxISAInstrInfo &TII) {
  MachineInstr &MI0 = *I;
  if (MI0.isDebugInstr())
    return false;
  if (isVolatileOrAtomicMem(MI0))
    return false;

  unsigned PairOpc = 0;
  if (!getPairOpc(MI0.getOpcode(), PairOpc))
    return false;

  auto J = nextNonDebug(MBB, I);
  if (J == MBB.end())
    return false;
  MachineInstr &MI1 = *J;
  if (MI1.getOpcode() != MI0.getOpcode())
    return false;
  if (isVolatileOrAtomicMem(MI1))
    return false;

  // Immediate memory ops:
  // - Loads:  (def dst), base, off
  // - Stores: value, base, off
  if (MI0.getNumOperands() < 3 || MI1.getNumOperands() < 3)
    return false;
  if (!MI0.getOperand(2).isImm() || !MI1.getOperand(2).isImm())
    return false;

  const Register Base0 = MI0.getOperand(1).getReg();
  const Register Base1 = MI1.getOperand(1).getReg();
  if (Base0 != Base1)
    return false;

  const int64_t Off0 = MI0.getOperand(2).getImm();
  const int64_t Off1 = MI1.getOperand(2).getImm();
  if (Off1 != Off0 + 1)
    return false;

  const DebugLoc DL = MI0.getDebugLoc();
  MachineInstrBuilder NewMI = BuildMI(MBB, I, DL, TII.get(PairOpc));

  if (MI0.mayLoad()) {
    const Register Dst0 = MI0.getOperand(0).getReg();
    const Register Dst1 = MI1.getOperand(0).getReg();
    if (Dst0 == Base0 || Dst1 == Base0 || Dst0 == Dst1)
      return false;
    NewMI.addReg(Dst0, RegState::Define);
    NewMI.addReg(Dst1, RegState::Define);
    NewMI.addReg(Base0);
    NewMI.addImm(Off0);
  } else if (MI0.mayStore()) {
    const Register Src0 = MI0.getOperand(0).getReg();
    const Register Src1 = MI1.getOperand(0).getReg();
    NewMI.addReg(Src0);
    NewMI.addReg(Src1);
    NewMI.addReg(Base0);
    NewMI.addImm(Off0);
  } else {
    return false;
  }

  for (MachineMemOperand *MMO : MI0.memoperands())
    NewMI.addMemOperand(MMO);
  for (MachineMemOperand *MMO : MI1.memoperands())
    NewMI.addMemOperand(MMO);

  MI0.eraseFromParent();
  MI1.eraseFromParent();

  I = std::next(NewMI.getInstr()->getIterator());
  return true;
}

static bool tryCombinePostIndex(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator &I,
                                const LinxISAInstrInfo &TII) {
  MachineInstr &Mem = *I;
  if (Mem.isDebugInstr())
    return false;
  if (isVolatileOrAtomicMem(Mem))
    return false;

  unsigned PostScaled = 0, PreScaled = 0, PostUnscaled = 0, PreUnscaled = 0;
  unsigned AccessBytes = 0;
  if (!getWritebackOpcs(Mem.getOpcode(), PostScaled, PreScaled, PostUnscaled,
                        PreUnscaled, AccessBytes))
    return false;

  if (Mem.getNumOperands() < 3 || !Mem.getOperand(2).isImm())
    return false;
  if (Mem.getOperand(2).getImm() != 0)
    return false;

  auto J = nextNonDebug(MBB, I);
  if (J == MBB.end())
    return false;
  MachineInstr &Add = *J;

  auto DeltaBytesOpt = getAddSubDeltaBytes(Add);
  if (!DeltaBytesOpt)
    return false;

  if (Add.getNumOperands() < 3 || !Add.getOperand(0).isReg() ||
      !Add.getOperand(1).isReg())
    return false;

  const Register Base = Mem.getOperand(1).getReg();
  const Register AddSrc = Add.getOperand(1).getReg();
  const Register WbDst = Add.getOperand(0).getReg();
  if (AddSrc != Base)
    return false;

  const int64_t DeltaBytes = *DeltaBytesOpt;
  if (DeltaBytes == 0)
    return false;

  int64_t OffField = 0;
  unsigned NewOpc = 0;
  if ((DeltaBytes % int64_t(AccessBytes)) == 0) {
    OffField = DeltaBytes / int64_t(AccessBytes);
    NewOpc = PostScaled;
  } else {
    OffField = DeltaBytes;
    NewOpc = PostUnscaled;
  }

  if (!isInt<17>(OffField))
    return false;

  const DebugLoc DL = Mem.getDebugLoc();
  MachineInstrBuilder NewMI = BuildMI(MBB, I, DL, TII.get(NewOpc));

  if (Mem.mayLoad()) {
    const Register Dst = Mem.getOperand(0).getReg();
    if (Dst == Base || Dst == WbDst)
      return false;
    NewMI.addReg(Dst, RegState::Define);
    NewMI.addReg(WbDst, RegState::Define);
    NewMI.addReg(Base);
    NewMI.addImm(OffField);
  } else if (Mem.mayStore()) {
    const Register Val = Mem.getOperand(0).getReg();
    NewMI.addReg(WbDst, RegState::Define);
    NewMI.addReg(Val);
    NewMI.addReg(Base);
    NewMI.addImm(OffField);
  } else {
    return false;
  }

  for (MachineMemOperand *MMO : Mem.memoperands())
    NewMI.addMemOperand(MMO);

  Mem.eraseFromParent();
  Add.eraseFromParent();

  I = std::next(NewMI.getInstr()->getIterator());
  return true;
}

static bool tryCombinePreIndex(MachineFunction &MF, MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator &I,
                               const LinxISAInstrInfo &TII) {
  MachineInstr &Add = *I;
  if (Add.isDebugInstr())
    return false;

  auto DeltaBytesOpt = getAddSubDeltaBytes(Add);
  if (!DeltaBytesOpt)
    return false;

  if (Add.getNumOperands() < 3 || !Add.getOperand(0).isReg() ||
      !Add.getOperand(1).isReg())
    return false;

  auto J = nextNonDebug(MBB, I);
  if (J == MBB.end())
    return false;
  MachineInstr &Mem = *J;
  if (Mem.isDebugInstr())
    return false;
  if (isVolatileOrAtomicMem(Mem))
    return false;

  unsigned PostScaled = 0, PreScaled = 0, PostUnscaled = 0, PreUnscaled = 0;
  unsigned AccessBytes = 0;
  if (!getWritebackOpcs(Mem.getOpcode(), PostScaled, PreScaled, PostUnscaled,
                        PreUnscaled, AccessBytes))
    return false;

  if (Mem.getNumOperands() < 3 || !Mem.getOperand(2).isImm())
    return false;
  if (Mem.getOperand(2).getImm() != 0)
    return false;

  const Register OldBase = Add.getOperand(1).getReg();
  const Register NewBase = Add.getOperand(0).getReg();
  const Register MemBase = Mem.getOperand(1).getReg();
  if (MemBase != NewBase)
    return false;

  const int64_t DeltaBytes = *DeltaBytesOpt;
  if (DeltaBytes == 0)
    return false;

  int64_t OffField = 0;
  unsigned NewOpc = 0;
  if ((DeltaBytes % int64_t(AccessBytes)) == 0) {
    OffField = DeltaBytes / int64_t(AccessBytes);
    NewOpc = PreScaled;
  } else {
    OffField = DeltaBytes;
    NewOpc = PreUnscaled;
  }

  if (!isInt<17>(OffField))
    return false;

  const DebugLoc DL = Add.getDebugLoc();
  MachineInstrBuilder NewMI = BuildMI(MBB, I, DL, TII.get(NewOpc));

  if (Mem.mayLoad()) {
    const Register Dst = Mem.getOperand(0).getReg();
    if (Dst == NewBase || Dst == OldBase)
      return false;
    NewMI.addReg(Dst, RegState::Define);
    NewMI.addReg(NewBase, RegState::Define);
    NewMI.addReg(OldBase);
    NewMI.addImm(OffField);
  } else if (Mem.mayStore()) {
    const Register Val = Mem.getOperand(0).getReg();
    NewMI.addReg(NewBase, RegState::Define);
    NewMI.addReg(Val);
    NewMI.addReg(OldBase);
    NewMI.addImm(OffField);
  } else {
    return false;
  }

  for (MachineMemOperand *MMO : Mem.memoperands())
    NewMI.addMemOperand(MMO);

  Add.eraseFromParent();
  Mem.eraseFromParent();

  I = std::next(NewMI.getInstr()->getIterator());
  return true;
}

bool LinxISAMemOpsCombine::runOnMachineFunction(MachineFunction &MF) {
  const auto &ST = MF.getSubtarget<LinxISASubtarget>();
  if (!ST.hasExtS32())
    return false;
  if (MF.getFunction().hasOptNone())
    return false;

  const auto *TII = ST.getInstrInfo();
  if (!TII)
    report_fatal_error("Linx: missing InstrInfo for memops combine");

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      if (tryCombinePair(MF, MBB, I, *TII) ||
          tryCombinePostIndex(MF, MBB, I, *TII) ||
          tryCombinePreIndex(MF, MBB, I, *TII)) {
        Changed = true;
        continue;
      }
      ++I;
    }
  }
  return Changed;
}

FunctionPass *llvm::createLinxISAMemOpsCombinePass() {
  return new LinxISAMemOpsCombine();
}
