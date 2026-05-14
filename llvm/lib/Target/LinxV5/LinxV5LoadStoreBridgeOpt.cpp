// ===-------------------- LinxV5LoadStoreBridgeOpt.cpp --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// ===----------------------------------------------------------------------===//
//
// Replace load-store pair with load-store bridge.
//
// ===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5TargetMachine.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

static cl::opt<bool> EnableLoadStoreBridgeOpt(
    "linxv5-enable-ldst-bridge",
    cl::desc("Enable Load Store Bridge Optimization"), cl::init(true),
    cl::Hidden);

#define DEBUG_TYPE "ldst-bridge"
#define LINX_LOAD_STORE_BRIDGE_OPTIMIZE "LinxV5 Load-Store Bridge Optimize"

namespace {
class LinxV5LoadStoreBridgeOpt : public MachineFunctionPass {
public:
  static char ID;
  MachineRegisterInfo *MRI;
  const TargetRegisterInfo *TRI;
  const LinxV5InstrInfo *TII;
  LinxV5LoadStoreBridgeOpt() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override {
    return LINX_LOAD_STORE_BRIDGE_OPTIMIZE;
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  static DenseMap<unsigned, unsigned> LoadBridgeOpMap;
  static DenseMap<unsigned, unsigned> StoreBridgeOpMap;
  bool isLoadInstr(MachineInstr &MI) {
    return LoadBridgeOpMap.find(MI.getOpcode()) != LoadBridgeOpMap.end();
  }
  bool isStoreInstr(MachineInstr &MI) {
    return StoreBridgeOpMap.find(MI.getOpcode()) != StoreBridgeOpMap.end();
  }
  bool isGlobalMemAccess(MachineInstr &MI);
  bool isTileRegAccess(MachineInstr &MI);
  MachineInstr &replaceOpcode(MachineInstr &MI, unsigned Opc);
};
} // namespace

char LinxV5LoadStoreBridgeOpt::ID = 0;

INITIALIZE_PASS(LinxV5LoadStoreBridgeOpt, DEBUG_TYPE, LINX_LOAD_STORE_BRIDGE_OPTIMIZE,
                false, false)

DenseMap<unsigned, unsigned> LinxV5LoadStoreBridgeOpt::LoadBridgeOpMap = {
  {LinxV5::SIMT_LB,     LinxV5::SIMT_LB_BRG},
  {LinxV5::SIMT_LH,     LinxV5::SIMT_LH_BRG},
  {LinxV5::SIMT_LW,     LinxV5::SIMT_LW_BRG},
  {LinxV5::SIMT_LD,     LinxV5::SIMT_LD_BRG},
  {LinxV5::SIMT_LBU,    LinxV5::SIMT_LBU_BRG},
  {LinxV5::SIMT_LHU,    LinxV5::SIMT_LHU_BRG},
  {LinxV5::SIMT_LWU,    LinxV5::SIMT_LWU_BRG},
  {LinxV5::SIMT_LHI,    LinxV5::SIMT_LHI_BRG},
  {LinxV5::SIMT_LWI,    LinxV5::SIMT_LWI_BRG},
  {LinxV5::SIMT_LDI,    LinxV5::SIMT_LDI_BRG},
  {LinxV5::SIMT_LBUI,   LinxV5::SIMT_LBUI_BRG},
  {LinxV5::SIMT_LHUI,   LinxV5::SIMT_LHUI_BRG},
  {LinxV5::SIMT_LWUI,   LinxV5::SIMT_LWUI_BRG},
  {LinxV5::SIMT_LBI,    LinxV5::SIMT_LBI_BRG},
  {LinxV5::SIMT_LHI_U,  LinxV5::SIMT_LHI_U_BRG},
  {LinxV5::SIMT_LWI_U,  LinxV5::SIMT_LWI_U_BRG},
  {LinxV5::SIMT_LDI_U,  LinxV5::SIMT_LDI_U_BRG},
  {LinxV5::SIMT_LHUI_U, LinxV5::SIMT_LHUI_U_BRG},
  {LinxV5::SIMT_LWUI_U, LinxV5::SIMT_LWUI_U_BRG}
};

DenseMap<unsigned, unsigned> LinxV5LoadStoreBridgeOpt::StoreBridgeOpMap = {
  {LinxV5::SIMT_SB,     LinxV5::SIMT_SB_BRG},
  {LinxV5::SIMT_SH,     LinxV5::SIMT_SH_BRG},
  {LinxV5::SIMT_SW,     LinxV5::SIMT_SW_BRG},
  {LinxV5::SIMT_SD,     LinxV5::SIMT_SD_BRG},
  {LinxV5::SIMT_SH_U,   LinxV5::SIMT_SH_U_BRG},
  {LinxV5::SIMT_SW_U,   LinxV5::SIMT_SW_U_BRG},
  {LinxV5::SIMT_SD_U,   LinxV5::SIMT_SD_U_BRG},
  {LinxV5::SIMT_SHI,    LinxV5::SIMT_SHI_BRG},
  {LinxV5::SIMT_SWI,    LinxV5::SIMT_SWI_BRG},
  {LinxV5::SIMT_SDI,    LinxV5::SIMT_SDI_BRG},
  {LinxV5::SIMT_SBI,    LinxV5::SIMT_SBI_BRG},
  {LinxV5::SIMT_SHI_U,  LinxV5::SIMT_SHI_U_BRG},
  {LinxV5::SIMT_SWI_U,  LinxV5::SIMT_SWI_U_BRG},
  {LinxV5::SIMT_SDI_U,  LinxV5::SIMT_SDI_U_BRG}
};

bool LinxV5LoadStoreBridgeOpt::isGlobalMemAccess(MachineInstr &MI) {
  MachineOperand &BaseAddrOp = MI.getOperand(2);  // BaseAddrOp is the third op of load/store
  Register BaseReg = BaseAddrOp.getReg();
  if (BaseReg.isPhysical())
    return LinxV5::SIMT_RIORegClass.contains(BaseReg);
  MachineInstr &ParentMI = *MRI->getVRegDef(BaseReg);
  if (ParentMI.getOpcode() == LinxV5::COPY)
    return LinxV5::SIMT_RIORegClass.contains(ParentMI.getOperand(1).getReg());
  return false;
}

bool LinxV5LoadStoreBridgeOpt::isTileRegAccess(MachineInstr &MI) {
  MachineOperand &BaseAddrOp = MI.getOperand(2);  // BaseAddrOp is the third op of load/store
  Register BaseReg = BaseAddrOp.getReg();
  if (BaseReg.isPhysical())
    return LinxV5::SIMT_TileBaseRegClass.contains(BaseReg);
  MachineInstr &ParentMI = *MRI->getVRegDef(BaseReg);
  if (ParentMI.getOpcode() == LinxV5::COPY)
    return LinxV5::SIMT_TileBaseRegClass.contains(
        ParentMI.getOperand(1).getReg());
  return false;
}

MachineInstr &LinxV5LoadStoreBridgeOpt::replaceOpcode(MachineInstr &MI, unsigned Opc) {
  MachineInstrBuilder MIB = BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), TII->get(Opc));
  for (auto &MO : MI.operands())
    MIB.add(MO);
  MIB.setMIFlags(MI.getFlags());
  MI.eraseFromParent();
  return *MIB;
}

bool LinxV5LoadStoreBridgeOpt::runOnMachineFunction(MachineFunction &MF) {
  if (!EnableLoadStoreBridgeOpt)
    return false;
  if (!MF.getFunction().getFnAttribute("__mtc__").isValid())
    return false;

  MRI = &MF.getRegInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  TII = static_cast<const LinxV5InstrInfo *>(MF.getSubtarget().getInstrInfo());

  bool Changed = false;

  for (auto &MBB : MF) {
    LLVM_DEBUG(dbgs() << "Searching load-store pair on\n"; MF.dump());
    for (auto &MI : make_range(MBB.begin(), MBB.end())) {
        if (!isLoadInstr(MI))
          continue;
        assert(MI.getNumDefs() == 1 && "Load MI should only has 1 def.");
        Register BridgeReg = MI.getOperand(0).getReg();
        if (BridgeReg.isPhysical() || !MRI->hasOneUse(BridgeReg))
          continue;
        MachineInstr &UseMI = *MRI->use_instr_begin(BridgeReg);
        if (!isStoreInstr(UseMI))
          continue;
        if ((isGlobalMemAccess(MI) && isTileRegAccess(UseMI)) ||
            (isTileRegAccess(MI) && isGlobalMemAccess(UseMI))) {
          LLVM_DEBUG(dbgs() << "replacing: \n"; MI.dump(); UseMI.dump(););
          MachineInstr &MIBridge = replaceOpcode(MI, LoadBridgeOpMap[MI.getOpcode()]);
          MachineInstr &UseMIBridge = replaceOpcode(UseMI, StoreBridgeOpMap[UseMI.getOpcode()]);
          MRI->setRegClass(BridgeReg,
                           TRI->getRegClass(LinxV5::SIMTCGSRegClassID));
          LLVM_DEBUG(dbgs() << "to: \n"; MIBridge.dump(); UseMIBridge.dump(); dbgs() << "\n");
          Changed = true;
        }
    }
  }

  return Changed;
}

FunctionPass *llvm::createLinxV5LoadStoreBridgeOptPass() {
  return new LinxV5LoadStoreBridgeOpt();
}
