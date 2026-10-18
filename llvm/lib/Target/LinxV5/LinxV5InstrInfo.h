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
#include <array>

#define GET_INSTRINFO_HEADER
#include "LinxV5GenInstrInfo.inc"

namespace llvm {

class Function;
class LinxV5Subtarget;

class LinxV5InstrInfo : public LinxV5GenInstrInfo {
public:
  explicit LinxV5InstrInfo(LinxV5Subtarget &STI);
  virtual ~LinxV5InstrInfo() = default;

  bool isSchedulingBoundary(const MachineInstr &MI,
                            const MachineBasicBlock *MBB,
                            const MachineFunction &MF) const override;
  MachineBasicBlock::iterator
  getFirstInsertionPoint(MachineBasicBlock &MBB) const override;

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

struct TileDescriptor {
  unsigned DataType = 0;
  unsigned Layout = 0;
  unsigned ValidRow = 0;
  unsigned ValidCol = 0;
  unsigned PhysicalCol = 0;
  unsigned SizeCode = 0;
  // The initial packed predicate profile retains exactly one physical row.
  bool PackedPredicate = false;
  // Runtime B.DIM operands are preserved for legacy compiler-generated TMOVs.
  // Generic element lowering still requires fully static geometry.
  std::array<Register, 3> DimensionRegs = {Register(), Register(), Register()};
  std::array<int64_t, 3> DimensionImms = {1, 1, 1};
  bool HasRuntimeDimensions = false;
  MachineInstr *RuntimeDimensionProducer = nullptr;

  bool operator==(const TileDescriptor &Other) const {
    return DataType == Other.DataType && Layout == Other.Layout &&
           ValidRow == Other.ValidRow && ValidCol == Other.ValidCol &&
           PhysicalCol == Other.PhysicalCol && SizeCode == Other.SizeCode &&
           PackedPredicate == Other.PackedPredicate &&
           DimensionRegs == Other.DimensionRegs &&
           DimensionImms == Other.DimensionImms &&
           HasRuntimeDimensions == Other.HasRuntimeDimensions &&
           RuntimeDimensionProducer == Other.RuntimeDimensionProducer;
  }
};

struct LocalTileABIInfo {
  unsigned ArgumentIndex = 0;
  TileDescriptor Descriptor;
};

/// Parse and validate the closed Local-Tile call-boundary contract carried by
/// `linx-local-tile-abi`. Invalid or partial contracts fail closed; absence of
/// the attribute returns None.
Optional<LocalTileABIInfo> getLocalTileABIInfo(const Function &F);

SingleAsm parseSingleAsm(MachineInstr *MI);

int getPseudoMap(uint16_t Opcode);

bool isRAInstrOfInlineASMBlock(MachineInstr &MI);

bool isTileOp(const MachineInstr &MI);
/// Return a destination-free B.ASSEMBLE continuation's Local ParentRef, or
/// NoRegister when the inline-asm bundle has no such source-form binder.
Register getAssembleParentRef(const MachineInstr &MI);
/// Resolve the OperandIndex-th positional B.ASSEMBLE control operand.
/// Named or otherwise non-constant operands return None so callers fail closed.
Optional<bool> getAssembleControlBit(const MachineInstr &MI,
                                     unsigned OperandIndex);
unsigned getTileOpRegSize(MachineInstr &MI, Register Reg);
bool isIsolateInstr(MachineInstr &MI);

bool isPhysScratchRegAvailable(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator At, Register Reg);

unsigned getTileRegSize(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI, MCRegister Reg,
                        bool isSpill);

/// Resolve the complete descriptor reaching Reg immediately before MBBI.
/// Every predecessor must provide the same known descriptor. Unknown producer
/// semantics and conflicting control-flow joins fail closed.
TileDescriptor getTileRegDescriptor(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    MCRegister Reg);
Optional<TileDescriptor>
tryGetTileRegDescriptor(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI, MCRegister Reg);

/// Prove that every physical register carrying a runtime B.DIM value reaches
/// the proposed copy without a definition or regmask clobber. Accepted paths
/// have stale kill flags cleared and cross-block live-ins made explicit.
bool runtimeDimensionsReachCopy(const TileDescriptor &Descriptor,
                                MachineBasicBlock &Target,
                                MachineBasicBlock::iterator Before,
                                const TargetRegisterInfo &TRI);

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
