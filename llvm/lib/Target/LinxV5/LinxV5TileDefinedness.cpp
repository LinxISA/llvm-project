//===-- LinxV5TileDefinedness.cpp - Publish typed undefined PHI inputs -------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/InlineAsm.h"

using namespace llvm;
namespace {
class LinxV5TileDefinedness : public MachineFunctionPass {
public:
  static char ID;
  LinxV5TileDefinedness() : MachineFunctionPass(ID) {}
  StringRef getPassName() const override { return "LinxV5 Tile Definedness"; }
  bool runOnMachineFunction(MachineFunction &MF) override {
    MachineRegisterInfo &MRI = MF.getRegInfo();
    if (!MRI.isSSA())
      return false;
    const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
    auto IsTile = [&](Register R) {
      const TargetRegisterClass *RC = MRI.getRegClass(R);
      return LinxV5::Tile_ABSRegClass.hasSubClassEq(RC) ||
             LinxV5::Tile_ABS_CGRegClass.hasSubClassEq(RC);
    };
    SmallVector<MachineInstr *, 8> Seeds;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB)
        if (MI.isImplicitDef() && MI.getOperand(0).getReg().isVirtual() &&
            IsTile(MI.getOperand(0).getReg()))
          Seeds.push_back(&MI);
    bool Changed = false;
    for (MachineInstr *Seed : Seeds) {
      // Undefined ordinary LLVM values may choose zero, but the hardware
      // source must name a real publication. Obtain its storage contract only
      // from concrete inputs of the same SSA PHI/COPY component.
      SmallVector<Register, 8> Work{Seed->getOperand(0).getReg()};
      DenseSet<Register> Seen;
      Optional<LinxV5::TileDescriptor> Descriptor;
      bool Conflict = false;
      while (!Work.empty() && !Conflict) {
        Register Reg = Work.pop_back_val();
        if (!Seen.insert(Reg).second)
          continue;
        for (MachineInstr &User : MRI.use_nodbg_instructions(Reg)) {
          // An open B.ASSEMBLE parent has a separate initialization protocol;
          // publishing it here would consume its reserved hand too early.
          if (User.isInlineAsm() &&
              StringRef(User.getOperand(InlineAsm::MIOp_AsmString)
                            .getSymbolName()).contains("B.ASSEMBLE")) {
            Conflict = true;
            break;
          }
          if (!User.isCopy() && !User.isPHI())
            continue;
          Register Output = User.getOperand(0).getReg();
          if (!Output.isVirtual() || !IsTile(Output))
            continue;
          if (auto D = LinxV5::tryGetVirtualTileDescriptor(MF, Output)) {
            if (Descriptor && !(*Descriptor == *D)) {
              Conflict = true;
              break;
            }
            Descriptor = D;
          }
          Work.push_back(Output);
        }
      }
      if (Conflict || !Descriptor)
        continue;
      const auto &D = *Descriptor;
      if (D.PackedPredicate || D.HasRuntimeDimensions || D.ValidRowReg ||
          D.ValidColReg || D.PhysicalColReg || D.PEMask != 0b1111 ||
          (D.Layout != LinxV5Op::ArgFormat::NORM &&
           D.Layout != LinxV5Op::ArgFormat::CUBE_M16 &&
           D.Layout != LinxV5Op::ArgFormat::CUBE_M32))
        continue;
      Register Target = Seed->getOperand(0).getReg();
      Register Published = MRI.getRegClass(Target) == &LinxV5::Tile_ABS_CGRegClass
                               ? MRI.createVirtualRegister(&LinxV5::Tile_ABSRegClass)
                               : Target;
      // TCI only generates integer sequences. A floating undef may choose
      // zero too, but must publish it with a typed scalar broadcast rather
      // than an illegal floating TCI. Keep the complete storage descriptor.
      const bool IntegerSequence =
          D.DataType == LinxV5Op::DataType::S16 ||
          D.DataType == LinxV5Op::DataType::U16 ||
          D.DataType == LinxV5Op::DataType::S32 ||
          D.DataType == LinxV5Op::DataType::U32 ||
          D.DataType == LinxV5Op::DataType::S64 ||
          D.DataType == LinxV5Op::DataType::U64;
      auto Publish = BuildMI(
          *Seed->getParent(), Seed->getIterator(), Seed->getDebugLoc(),
          TII->get(IntegerSequence ? LinxV5::PseudoTCI_EW_Row_SizeI
                                   : LinxV5::PseudoTEXPANDS_Row_SizeI),
          Published)
          .addReg(LinxV5::R0)
          .addImm(D.ValidRow)
          .addReg(LinxV5::R0)
          .addImm(D.ValidCol)
          .addImm(D.DataType)
          .addImm(D.Layout)
          .addImm(D.SizeCode)
          .addReg(LinxV5::R0);
      if (IntegerSequence)
        Publish.addReg(LinxV5::R0);
      Publish.addImm(D.PhysicalCol);
      if (Published != Target)
        BuildMI(*Seed->getParent(), Seed->getIterator(), Seed->getDebugLoc(),
                TII->get(TargetOpcode::COPY), Target).addReg(Published);
      Seed->eraseFromParent();
      Changed = true;
    }
    return Changed;
  }
};
} // namespace
char LinxV5TileDefinedness::ID = 0;
INITIALIZE_PASS(LinxV5TileDefinedness, "linxv5-tile-definedness",
                "LinxV5 Tile Definedness", false, false)
FunctionPass *llvm::createLinxV5TileDefinednessPass() {
  return new LinxV5TileDefinedness();
}
