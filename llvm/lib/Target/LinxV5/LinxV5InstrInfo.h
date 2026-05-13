//===-- LinxV5InstrInfo.h - LinxV5 Instruction Information -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the LinxV5 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_LINXV5INSTRINFO_H
#define LLVM_LIB_TARGET_LINXV5_LINXV5INSTRINFO_H

#include "LinxV5RegisterInfo.h"
#include "MCTargetDesc/LinxV5MatInt.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "LinxV5GenInstrInfo.inc"

namespace llvm {

class LinxV5Subtarget;

class LinxV5InstrInfo : public LinxV5GenInstrInfo {
public:
  explicit LinxV5InstrInfo(LinxV5Subtarget &STI);
  virtual ~LinxV5InstrInfo() = default;

  bool isSchedulingBoundary(const MachineInstr &MI,
                            const MachineBasicBlock *MBB,
                            const MachineFunction &MF) const override;

  unsigned isLoadFromStackSlot(const MachineInstr &MI,
                               int &FrameIndex) const override;
  unsigned isStoreToStackSlot(const MachineInstr &MI,
                              int &FrameIndex) const override;

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                   const DebugLoc &DL, MCRegister DstReg, MCRegister SrcReg,
                   bool KillSrc) const override;

  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI, Register SrcReg,
                           bool IsKill, int FrameIndex,
                           const TargetRegisterClass *RC,
                           const TargetRegisterInfo *TRI) const override;

  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI, Register DstReg,
                            int FrameIndex, const TargetRegisterClass *RC,
                            const TargetRegisterInfo *TRI) const override;

  // Materializes the given integer Val into DstReg.
  void movImm(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
              const DebugLoc &DL, Register DstReg, uint64_t Val,
              MachineInstr::MIFlag Flag = MachineInstr::NoFlags) const;

  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;

  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &dl,
                        int *BytesAdded = nullptr) const override;

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;

  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

  MachineBasicBlock *getBranchDestBlock(const MachineInstr &MI) const override;

  bool isAsCheapAsAMove(const MachineInstr &MI) const override;

  bool isHoistRemat(const MachineInstr &MI) const override;

  bool isReallyTriviallyReMaterializable(const MachineInstr &MI) const override;

  Optional<DestSourcePair>
  isCopyInstrImpl(const MachineInstr &MI) const override;

  bool verifyInstruction(const MachineInstr &MI,
                         StringRef &ErrInfo) const override;

  bool areMemAccessesTriviallyDisjoint(const MachineInstr &MIa,
                                       const MachineInstr &MIb) const override;

  std::pair<unsigned, unsigned>
  decomposeMachineOperandsTargetFlags(unsigned TF) const override;

  ArrayRef<std::pair<unsigned, const char *>>
  getSerializableDirectMachineOperandTargetFlags() const override;

  const TargetRegisterClass *
  getRegClass(const MCInstrDesc &TID, unsigned OpNum,
              const TargetRegisterInfo *TRI,
              const MachineFunction &MF) const override;

  unsigned getExtendedOpcode(unsigned OpBefore, unsigned EType) const;

  unsigned getOppositeOpcode(unsigned Op) const;

protected:
  const LinxV5Subtarget &STI;
};

namespace LinxV5 {

enum SIMTRegSize {
  SIMT_REG_SIZE_B = 1,
  SIMT_REG_SIZE_H = 2,
  SIMT_REG_SIZE_W = 4,
  SIMT_REG_SIZE_D = 8,
};

struct SingleAsm {
  SmallVector<MachineOperand *, 2> Defs;
  SmallVector<unsigned, 2> Sizes;
};

SingleAsm parseSingleAsm(MachineInstr *MI);

int getPseudoMap(uint16_t Opcode);

bool isRAInstrOfInlineASMBlock(MachineInstr &MI);

bool isTileOp(const MachineInstr &MI);
unsigned getTileOpRegSize(MachineInstr &MI, Register Reg);
bool isIsolateInstr(MachineInstr &MI);

bool isPhysScratchRegAvailable(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator At, Register Reg);

unsigned getTileRegSize(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI, MCRegister Reg,
                        bool isSpill);

bool enableBFIOpt();

unsigned getSIMTDstTypeFromBits(unsigned Bits);
unsigned getSizeFromSIMTType(unsigned DstTypeEnum);
unsigned getSIMTDstTypeFromSize(unsigned RegSize);
unsigned getSIMTSrcTypeFromSize(unsigned RegSize);

unsigned getUseRegSizeAtSingleBlock(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    MCRegister Reg, bool &FindDef);
unsigned getUseRegSize(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                       MCRegister Reg);

unsigned getSIMTDstType(MVT VT);
unsigned getSIMTSignedSrcType(MVT VT);
unsigned getSIMTUnsignedSrcType(MVT VT);

void generateMatIntSeq(int64_t Val, LinxV5MatInt::InstSeq &Res, bool HasFloat);
void generateSIMTMatIntSeq(int64_t Val, LinxV5MatInt::SIMTInstSeq &Res, MVT VT);
MachineInstr *findSETC(MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator Before);
} // end namespace LinxV5

} // end namespace llvm
#endif
