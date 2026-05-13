// ===----------------------- LinxV5RegReuseMarker.cpp
// ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
// Mark the reuse flag of simt&tile register
//
// ===--------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5TargetMachine.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "reuse-mark"
#define LINX_REUSE_MARK "LinxV5 Reuse Mark"

static cl::opt<bool>
    EnableRegReuseMark("linxv5-reuse-mark", cl::init(true), cl::Hidden,
                       cl::desc("add simt&tile register reuse mark"));

namespace {
class LinxV5RegReuseMarker : public MachineFunctionPass {
public:
  static char ID;
  LinxV5RegReuseMarker() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return LINX_REUSE_MARK; }

private:
  MachineRegisterInfo *MRI;
  llvm::DenseMap<const TargetRegisterClass *, SmallVector<Register, 2>> RCsInfo;
  void markRegReuseFlag(MachineInstr &MI);
  Register TransReg2ReuseReg(Register reg);
  void releaseMemory() override { RCsInfo.clear(); }
};
} // namespace

char LinxV5RegReuseMarker::ID = 0;

INITIALIZE_PASS(LinxV5RegReuseMarker, DEBUG_TYPE, LINX_REUSE_MARK, false, false)

void LinxV5RegReuseMarker::markRegReuseFlag(MachineInstr &MI) {
  for (MachineOperand &MO : MI.uses()) {
    if (!MO.isReg())
      continue;
    if (MO.isKill() && EnableRegReuseMark)
      continue;
    Register Reg = MO.getReg();
    if (LinxV5::SIMT_OSVRRegClass.contains(Reg) ||
        LinxV5::Tile_OSRegClass.contains(Reg)) {
      Register NewReg = TransReg2ReuseReg(Reg);
      if (NewReg != LinxV5::NoRegister)
        MO.setReg(NewReg);
    }
  }
}

Register LinxV5RegReuseMarker::TransReg2ReuseReg(Register Reg) {
  for (auto RCInfo : RCsInfo) {
    const TargetRegisterClass *RC = RCInfo.first;
    if (RC->contains(Reg)) {
      unsigned Offset = Reg - RCInfo.second[0];
      return Offset + RCInfo.second[1];
    }
  }
  return LinxV5::NoRegister;
}

bool LinxV5RegReuseMarker::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "Starting Reg Reuse Marking\n");
  MRI = &MF.getRegInfo();
  RCsInfo = {
      // Register Class,
      // {Register Base, Register Output}
      // Vec Reg
      {&LinxV5::SIMT_OSVTRRegClass, {LinxV5::SIMT_OSVT1, LinxV5::SIMT_OSVT1_RU}},
      {&LinxV5::SIMT_OSVURRegClass, {LinxV5::SIMT_OSVU1, LinxV5::SIMT_OSVU1_RU}},
      {&LinxV5::SIMT_OSVMRRegClass, {LinxV5::SIMT_OSVM1, LinxV5::SIMT_OSVM1_RU}},
      {&LinxV5::SIMT_OSVNRRegClass, {LinxV5::SIMT_OSVN1, LinxV5::SIMT_OSVN1_RU}},
      // Tile Reg
      {&LinxV5::Tile_TROSRegClass, {LinxV5::Tile_TOS1, LinxV5::Tile_TOS1_RU}},
      {&LinxV5::Tile_UROSRegClass, {LinxV5::Tile_UOS1, LinxV5::Tile_UOS1_RU}},
      {&LinxV5::Tile_MROSRegClass, {LinxV5::Tile_MOS1, LinxV5::Tile_MOS1_RU}},
      {&LinxV5::Tile_NROSRegClass, {LinxV5::Tile_NOS1, LinxV5::Tile_NOS1_RU}},
  };

  for (auto &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      markRegReuseFlag(MI);
    }
  }
  return true;
}

FunctionPass *llvm::createLinxV5RegReuseMarkerPass() {
  return new LinxV5RegReuseMarker();
}
