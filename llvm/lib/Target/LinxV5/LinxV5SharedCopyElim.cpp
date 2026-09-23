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
    if (!MI.isDebugInstr())
      return &MI;
    I = std::prev(I);
  }
  return nullptr;
}

static bool isShared(MCRegister Reg) {
  return LinxV5::Shared_ABSRegClass.contains(Reg);
}

static bool isGPR(MCRegister Reg) {
  return LinxV5::GRRegClass.contains(Reg) ||
         LinxV5::GRNoRARegClass.contains(Reg) ||
         LinxV5::GRNoR0RegClass.contains(Reg) ||
         LinxV5::MixedGPRRegClass.contains(Reg) ||
         LinxV5::MixedGPRNoRARegClass.contains(Reg);
}

bool LinxV5SharedCopyElim::runOnMachineFunction(MachineFunction &MF) {
  SmallVector<std::pair<MachineInstr *, MachineInstr *>, 8> ToErase;

  for (MachineBasicBlock &MBB : MF) {
    if (std::distance(MBB.pred_begin(), MBB.pred_end()) < 2)
      continue;

    MachineInstr *MergeCopy = firstNonDebug(MBB);
    if (!MergeCopy || !MergeCopy->isCopy() ||
        MergeCopy->getNumOperands() < 2)
      continue;

    MCRegister SharedDst = MergeCopy->getOperand(0).getReg();
    MCRegister BridgeGPR = MergeCopy->getOperand(1).getReg();
    if (!Register::isPhysicalRegister(SharedDst) ||
        !Register::isPhysicalRegister(BridgeGPR) ||
        !isShared(SharedDst) || !isGPR(BridgeGPR) ||
        !MergeCopy->getOperand(1).isKill())
      continue;

    MachineInstr *SourceCopy = nullptr;
    unsigned MatchingPreds = 0;
    bool Ambiguous = false;
    for (MachineBasicBlock *Pred : MBB.predecessors()) {
      MachineInstr *PredCopy = lastNonDebugBeforeTerminator(*Pred);
      if (!PredCopy || !PredCopy->isCopy() || PredCopy->getNumOperands() < 2)
        continue;

      MCRegister PredDst = PredCopy->getOperand(0).getReg();
      MCRegister PredSrc = PredCopy->getOperand(1).getReg();
      if (PredDst != BridgeGPR || !Register::isPhysicalRegister(PredSrc) ||
          PredSrc != SharedDst || !isShared(PredSrc) ||
          !PredCopy->getOperand(1).isKill())
        continue;

      if (SourceCopy && SourceCopy != PredCopy) {
        Ambiguous = true;
        break;
      }
      SourceCopy = PredCopy;
      ++MatchingPreds;
    }

    if (Ambiguous || !SourceCopy || MatchingPreds != 1)
      continue;

    ToErase.emplace_back(SourceCopy, MergeCopy);
  }

  for (auto &Pair : ToErase) {
    Pair.first->eraseFromParent();
    Pair.second->eraseFromParent();
  }
  return !ToErase.empty();
}

FunctionPass *llvm::createLinxV5SharedCopyElimPass() {
  return new LinxV5SharedCopyElim();
}
