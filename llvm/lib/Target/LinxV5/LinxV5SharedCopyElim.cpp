//===-- LinxV5SharedCopyElim.cpp - Remove unsupported Shared bridge copies ===//

#include "LinxV5.h"
#include "LinxV5RegisterInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include <iterator>

using namespace llvm;

#define DEBUG_TYPE "linxv5-shared-copy-elim"

namespace {
class LinxV5SharedCopyElim : public MachineFunctionPass {
public:
  static char ID;
  LinxV5SharedCopyElim() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "LinxV5 Shared Copy Elimination";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // namespace

char LinxV5SharedCopyElim::ID = 0;

INITIALIZE_PASS(LinxV5SharedCopyElim, DEBUG_TYPE,
                "LinxV5 Shared Copy Elimination", false, false)

static bool isShared(Register Reg, MachineRegisterInfo &MRI) {
  return Register::isPhysicalRegister(Reg)
             ? LinxV5::Shared_ABSRegClass.contains(Reg)
             : MRI.getRegClass(Reg) == &LinxV5::Shared_ABSRegClass;
}

static bool isGPR(Register Reg, MachineRegisterInfo &MRI) {
  if (Register::isPhysicalRegister(Reg))
    return LinxV5::GRRegClass.contains(Reg) ||
           LinxV5::GRNoRARegClass.contains(Reg) ||
           LinxV5::GRNoR0RegClass.contains(Reg) ||
           LinxV5::MixedGPRRegClass.contains(Reg) ||
           LinxV5::MixedGPRNoRARegClass.contains(Reg);
  const TargetRegisterClass *RC = MRI.getRegClass(Reg);
  return RC == &LinxV5::GRRegClass || RC == &LinxV5::GRNoRARegClass ||
         RC == &LinxV5::GRNoR0RegClass || RC == &LinxV5::MixedGPRRegClass ||
         RC == &LinxV5::MixedGPRNoRARegClass;
}

static MachineInstr *previousReal(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I) {
  while (I != MBB.begin()) {
    MachineInstr &MI = *std::prev(I);
    I = MI.getIterator();
    if (!MI.isDebugInstr())
      return &MI;
  }
  return nullptr;
}

static MachineInstr *findMergeLoad(MachineBasicBlock &MBB,
                                   MachineRegisterInfo &MRI, Register &Dst,
                                   Register &Bridge, int &FI) {
  for (MachineInstr &MI : MBB) {
    if (MI.isDebugInstr())
      continue;
    if (!MI.isCopy())
      continue;
    if (MI.getNumOperands() < 2 || !MI.getOperand(0).isReg() ||
        !MI.getOperand(1).isReg() ||
        !isShared(MI.getOperand(0).getReg(), MRI) ||
        !isGPR(MI.getOperand(1).getReg(), MRI))
      return nullptr;
    MachineInstr *Load = previousReal(MBB, MI.getIterator());
    if (!Load || Load->getOpcode() != LinxV5::LDI ||
        Load->getNumOperands() < 3 || !Load->getOperand(0).isReg() ||
        !Load->getOperand(1).isFI() || !Load->getOperand(2).isImm() ||
        Load->getOperand(2).getImm() != 0 ||
        Load->getOperand(0).getReg() != MI.getOperand(1).getReg())
      return nullptr;
    Dst = MI.getOperand(0).getReg();
    Bridge = MI.getOperand(1).getReg();
    FI = Load->getOperand(1).getIndex();
    return &MI;
  }
  return nullptr;
}

struct PredBridge {
  MachineInstr *SharedToGPR = nullptr;
  MachineInstr *Store = nullptr;
  Register Source = 0;
};

static bool findPredBridge(MachineBasicBlock &MBB, MachineRegisterInfo &MRI,
                           int FI, PredBridge &Result) {
  auto I = MBB.getFirstTerminator();
  while (I != MBB.begin()) {
    MachineInstr &Store = *std::prev(I);
    I = Store.getIterator();
    if (Store.isDebugInstr())
      continue;
    if (Store.getOpcode() != LinxV5::SDI || Store.getNumOperands() < 3 ||
        !Store.getOperand(0).isReg() || !Store.getOperand(1).isFI() ||
        !Store.getOperand(2).isImm() || Store.getOperand(1).getIndex() != FI ||
        Store.getOperand(2).getImm() != 0)
      return false;
    MachineInstr *Copy = previousReal(MBB, Store.getIterator());
    if (!Copy || !Copy->isCopy() || Copy->getNumOperands() < 2 ||
        !Copy->getOperand(0).isReg() || !Copy->getOperand(1).isReg() ||
        Copy->getOperand(0).getReg() != Store.getOperand(0).getReg() ||
        !isGPR(Copy->getOperand(0).getReg(), MRI) ||
        !isShared(Copy->getOperand(1).getReg(), MRI))
      return false;
    Result.SharedToGPR = Copy;
    Result.Store = &Store;
    Result.Source = Copy->getOperand(1).getReg();
    return true;
  }
  return false;
}

static bool sourceIsOnlyBridge(MachineFunction &MF, MachineInstr *Bridge,
                               Register Source) {
  unsigned Definitions = 0;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.isDebugInstr())
        continue;
      for (unsigned Op = 0; Op < MI.getNumOperands(); ++Op) {
        const MachineOperand &MO = MI.getOperand(Op);
        if (!MO.isReg() || MO.getReg() != Source)
          continue;
        if (&MI == Bridge && Op == 1)
          continue;
        if (!MO.isDef() || !MI.isInlineAsm())
          return false;
        if (++Definitions != 1)
          return false;
      }
    }
  }
  return Definitions == 1;
}

static bool isPhysRegUsed(MachineFunction &MF, MCRegister Reg) {
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      for (const MachineOperand &MO : MI.operands())
        if (MO.isReg() && MO.getReg() == Reg)
          return true;
  return false;
}

static MCRegister findFreeSharedReg(MachineFunction &MF) {
  for (unsigned I = 0; I < LinxV5::Shared_ABSRegClass.getNumRegs(); ++I) {
    MCRegister Reg = LinxV5::Shared_ABSRegClass.getRegister(I);
    if (!isPhysRegUsed(MF, Reg))
      return Reg;
  }
  return 0;
}

static MachineInstr *firstNonDebug(MachineBasicBlock &MBB) {
  for (MachineInstr &MI : MBB)
    if (!MI.isDebugInstr())
      return &MI;
  return nullptr;
}

static MachineInstr *lastNonDebugBeforeTerminator(MachineBasicBlock &MBB) {
  auto I = MBB.getFirstTerminator();
  while (I != MBB.begin()) {
    MachineInstr &MI = *std::prev(I);
    I = MI.getIterator();
    if (!MI.isDebugInstr())
      return &MI;
  }
  return nullptr;
}

static bool eliminatePhysicalBridge(MachineBasicBlock &Merge,
                                    SmallVectorImpl<MachineInstr *> &ToErase) {
  MachineInstr *MergeCopy = firstNonDebug(Merge);
  if (!MergeCopy || !MergeCopy->isCopy() || MergeCopy->getNumOperands() < 2 ||
      !MergeCopy->getOperand(0).isReg() ||
      !MergeCopy->getOperand(1).isReg())
    return false;

  MCRegister SharedDst = MergeCopy->getOperand(0).getReg();
  MCRegister BridgeGPR = MergeCopy->getOperand(1).getReg();
  if (!Register::isPhysicalRegister(SharedDst) ||
      !Register::isPhysicalRegister(BridgeGPR) ||
      !LinxV5::Shared_ABSRegClass.contains(SharedDst) ||
      !LinxV5::GRRegClass.contains(BridgeGPR) &&
          !LinxV5::GRNoRARegClass.contains(BridgeGPR) &&
          !LinxV5::GRNoR0RegClass.contains(BridgeGPR) &&
          !LinxV5::MixedGPRRegClass.contains(BridgeGPR) &&
          !LinxV5::MixedGPRNoRARegClass.contains(BridgeGPR))
    return false;

  MachineInstr *SourceCopy = nullptr;
  unsigned MatchingPreds = 0;
  for (MachineBasicBlock *Pred : Merge.predecessors()) {
    MachineInstr *PredCopy = lastNonDebugBeforeTerminator(*Pred);
    if (!PredCopy || !PredCopy->isCopy() || PredCopy->getNumOperands() < 2 ||
        !PredCopy->getOperand(0).isReg() ||
        !PredCopy->getOperand(1).isReg())
      continue;
    if (PredCopy->getOperand(0).getReg() != BridgeGPR ||
        PredCopy->getOperand(1).getReg() != SharedDst ||
        !LinxV5::Shared_ABSRegClass.contains(
            PredCopy->getOperand(1).getReg()))
      continue;
    SourceCopy = PredCopy;
    ++MatchingPreds;
  }
  if (!SourceCopy || MatchingPreds != 1)
    return false;

  ToErase.push_back(SourceCopy);
  ToErase.push_back(MergeCopy);
  return true;
}

bool LinxV5SharedCopyElim::runOnMachineFunction(MachineFunction &MF) {
  SmallVector<MachineInstr *, 32> ToErase;
  bool Changed = false;
  MachineRegisterInfo &MRI = MF.getRegInfo();

  for (MachineBasicBlock &Merge : MF) {
    if (std::distance(Merge.pred_begin(), Merge.pred_end()) < 2)
      continue;

    if (eliminatePhysicalBridge(Merge, ToErase)) {
      Changed = true;
      continue;
    }

    Register Destination = 0;
    Register BridgeGPR = 0;
    int FI = -1;
    MachineInstr *MergeCopy =
        findMergeLoad(Merge, MRI, Destination, BridgeGPR, FI);
    if (!MergeCopy)
      continue;

    SmallVector<std::pair<MachineBasicBlock *, PredBridge>, 8> Preds;
    bool Valid = true;
    unsigned MissingPreds = 0;
    for (MachineBasicBlock *Pred : Merge.predecessors()) {
      PredBridge Bridge;
      if (!findPredBridge(*Pred, MRI, FI, Bridge)) {
        ++MissingPreds;
        continue;
      }
      if (!sourceIsOnlyBridge(MF, Bridge.SharedToGPR, Bridge.Source)) {
        Valid = false;
        break;
      }
      Preds.emplace_back(Pred, Bridge);
    }
    if (!Valid || Preds.empty() || MissingPreds > 1)
      continue;

    MCRegister Canonical = findFreeSharedReg(MF);
    if (!Canonical)
      continue;
    for (auto &Entry : Preds)
      MRI.replaceRegWith(Entry.second.Source, Canonical);
    MRI.replaceRegWith(Destination, Canonical);

    for (auto &Entry : Preds) {
      ToErase.push_back(Entry.second.SharedToGPR);
      ToErase.push_back(Entry.second.Store);
    }
    MachineInstr *Load = previousReal(Merge, MergeCopy->getIterator());
    ToErase.push_back(Load);
    ToErase.push_back(MergeCopy);
    Changed = true;
  }

  for (MachineInstr *MI : ToErase)
    if (MI)
      MI->eraseFromParent();
  return Changed;
}

FunctionPass *llvm::createLinxV5SharedCopyElimPass() {
  return new LinxV5SharedCopyElim();
}
