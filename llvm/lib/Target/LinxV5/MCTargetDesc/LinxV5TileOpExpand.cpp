//===-- LinxV5TileOpExpand.cpp - LinxV5 Assembler Backend
//------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5TileOpExpand.h"
#include "LinxV5BaseInfo.h"
#include "LinxV5TileOpReader.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

namespace llvm {

static cl::opt<bool> EnableDimOpt("linxv5-enable-dim-opt",
                                  cl::desc("B.DIM Coding Optimization"),
                                  cl::init(true), cl::Hidden);

llvm::SmallVector<MCInst> getBIORFromInst(MCInst Inst, llvm::SmallVector<unsigned> inVec) {
  // The load local address pseudo-instruction "PS.B.IOR" is used in PC-relative
  // addressing of local symbols:
  //   PS.B.IOR [$reglistIn], [$reglistOut]
  // expands to
  //   B.IOR [$reglistIn], [$reglistOut]
  //   B.IOR [$reglistIn], [$reglistOut]
  //   ....
  llvm::SmallVector<MCInst> McVec;

  unsigned int InstNums =
      inVec.size() % 3 == 0 ? (inVec.size() / 3) : (inVec.size() / 3) + 1;
  unsigned int inID = 0;
  for (int i = 0; i < InstNums; ++i) {
    unsigned int RegSrc0 = LinxV5::R0;
    unsigned int RegSrc1 = LinxV5::R0;
    unsigned int RegSrc2 = LinxV5::R0;
    if (inID < inVec.size())
      RegSrc0 = inVec[inID++];
    if (inID < inVec.size())
      RegSrc1 = inVec[inID++];
    if (inID < inVec.size())
      RegSrc2 = inVec[inID++];
    McVec.push_back(MCInstBuilder(LinxV5::B_IO)
                        .addOperand(MCOperand::createReg(LinxV5::R0))
                        .addOperand(MCOperand::createReg(RegSrc0))
                        .addOperand(MCOperand::createReg(RegSrc1))
                        .addOperand(MCOperand::createReg(RegSrc2)));
  }
  return McVec;
}

llvm::SmallVector<MCInst> getBIORFromInstByIndex(MCInst Inst, int inIndex) {
  llvm::SmallVector<unsigned> inVec;
  for (unsigned i = 0; (inIndex + i) < Inst.getNumOperands(); i++) {
    unsigned Reg = Inst.getOperand(inIndex + i).getReg();
    if (Reg <= LinxV5::R23 && Reg >= LinxV5::R0)
      inVec.push_back(Reg);
  }
  return getBIORFromInst(Inst, inVec);
}

llvm::SmallVector<MCInst> getBARGFromInst(const MCInst &Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> MCVec;
  switch (Inst.getOpcode()) {
  default:
    break;
  case LinxV5::PseudoACCCVT_SizeI:
  case LinxV5::PseudoACCCVT_SizeR:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(8))
            .addOperand(Inst.getOperand(9))
            .addOperand(Inst.getOperand(6))
            .addOperand(MCOperand::createImm(LinxV5Op::PadValue::Null))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTMOV_SizeI:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(10))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(Inst.getOperand(11))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTLOAD_noDsrc_noDdst:
  case LinxV5::PseudoTLOAD_noDsrc_Ddst:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(9))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(Inst.getOperand(10))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTLOAD_Dsrc_noDdst:
  case LinxV5::PseudoTLOAD_Dsrc_Ddst:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(10))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(Inst.getOperand(11))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTSTORE_noDsrc_noDdst:
  case LinxV5::PseudoTSTORE_noDsrc_Ddst:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(8))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(MCOperand::createImm(LinxV5Op::PadValue::Null))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTSTORE_Dsrc_noDdst:
  case LinxV5::PseudoTSTORE_Dsrc_Ddst:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(9))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(MCOperand::createImm(LinxV5Op::PadValue::Null))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  }
  return MCVec;
}

void getPseudoCallBIOTBySrcDstNum(llvm::SmallVector<MCInst> &McVec,
                                  MCInst Inst, const MCInstrInfo &MII) {
  TileCallReader CallReader(Inst, MII);
  unsigned TileDstNum = CallReader.getTileDstNum();
  unsigned TileSrcNum = CallReader.getTileSrcNum();

  if (TileDstNum == 0 && TileSrcNum == 0) {
    if (CallReader.HasS())
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                          .addOperand(CallReader.getS())
                          .addOperand(MCOperand::createImm(1))
                          .addOperand(CallReader.getStackSize()));
    return;
  }

  auto getOpcode = [&](unsigned numSrc, bool hasDst) -> unsigned {
    switch (numSrc) {
    case 0:
      return LinxV5::B_IOT_NoSrc_Dst;
    case 1:
      return hasDst ? LinxV5::B_IOT_OneSrc_Dst : LinxV5::B_IOT_OneSrc_NoDst;
    case 2:
      return hasDst ? LinxV5::B_IOT_TwoSrc_Dst : LinxV5::B_IOT_TwoSrc_NoDst;
    default:
      llvm_unreachable("Invalid b.io src count");
    }
  };

  unsigned srcIdx = 0;
  unsigned dstIdx = 0;

  if (CallReader.HasS())
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(CallReader.getS())
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(CallReader.getStackSize()));

  // B.IO only can take at most 2 src, 1 dst
  while (srcIdx < TileSrcNum || dstIdx < TileDstNum) {
    unsigned remainingSrc = TileSrcNum - srcIdx;
    unsigned remainingDst = TileDstNum - dstIdx;
    unsigned thisSrcCount = std::min(2u, remainingSrc);
    bool hasDst = (remainingDst > 0);

    MCInstBuilder builder(getOpcode(thisSrcCount, hasDst));

    if (hasDst) {
      builder.addOperand(CallReader.getTileDst(dstIdx));
    }

    if (remainingSrc <= 2 && remainingDst <= 1) {
      // last Group
      builder.addOperand(MCOperand::createImm(1));
    } else {
      builder.addOperand(MCOperand::createImm(0));
    }
    if (hasDst) {
      builder.addOperand(CallReader.getTileSize(dstIdx));
      dstIdx++;
    }

    for (unsigned i = 0; i < thisSrcCount; i++)
      builder.addOperand(CallReader.getTileSrc(srcIdx++));

    McVec.push_back(builder);
  }
}

llvm::SmallVector<MCInst> getBATTRFromInst(MCInst Inst,
                                           const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;

  // MX matmul family:
  // If DataTypeA != DataTypeB, emit:
  //   B.ATTR DataTypeB
  // If DataTypeA == DataTypeB, no B.ATTR is needed.
  if (isMatmulPseudo(Inst.getOpcode())) {
    constexpr unsigned DataTypeAOpNo = 7;
    constexpr unsigned DataTypeBOpNo = 8;

    assert(Inst.getNumOperands() > DataTypeBOpNo &&
           "Unexpected operand layout for MX matmul pseudo");

    const MCOperand &DataTypeA = Inst.getOperand(DataTypeAOpNo);
    const MCOperand &DataTypeB = Inst.getOperand(DataTypeBOpNo);

    assert(DataTypeA.isImm() && DataTypeB.isImm() &&
           "DataType operands of MX matmul pseudo must be immediates");

    if (DataTypeA.getImm() == DataTypeB.getImm())
      return McVec;

    McVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(MCOperand::createImm(0))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(MCOperand::createImm(DataTypeB.getImm()))
            .addOperand(MCOperand::createImm(LinxV5Op::PadValue::Null))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    return McVec;
  }

  TileCallReader CallReader(Inst, MII);
  if (CallReader.HasDR() && CallReader.getDR().getImm() == LinxV5Op::DREnum::DR)
    McVec.push_back(
        MCInstBuilder(LinxV5::BCATR)
            .addOperand(MCOperand::createImm(0))
            .addOperand(MCOperand::createImm(LinxV5Op::DREnum::DR)));
  return McVec;
}

llvm::SmallVector<MCInst> getBIOTFromInst(MCInst Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;

  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    getPseudoCallBIOTBySrcDstNum(McVec, Inst, MII);
    return McVec;
  }
  switch (Inst.getOpcode()) {
  default:
    llvm_unreachable("Can not find BIO From Pseudo Inst!");
  case LinxV5::PseudoMAMULB_SizeI:
  case LinxV5::PseudoMAMULBAC_SizeI:
  case LinxV5::PseudoMAMULBACC_SizeI:
    if (Inst.getOpcode() == LinxV5::PseudoMAMULBAC_SizeI) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                          .addOperand(Inst.getOperand(0))
                          .addOperand(MCOperand::createImm(0))
                          .addOperand(Inst.getOperand(9))
                          .addOperand(Inst.getOperand(10))
                          .addOperand(Inst.getOperand(11)));
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
                          .addOperand(MCOperand::createImm(1))
                          .addOperand(Inst.getOperand(12)));
      break;
    }
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(9))
                        .addOperand(Inst.getOperand(10))
                        .addOperand(Inst.getOperand(11)));
    break;
  case LinxV5::PseudoMAMULBMX_SizeI:
  case LinxV5::PseudoMAMULBMXAC_SizeI:
  case LinxV5::PseudoMAMULBMXACC_SizeI:
    if (Inst.getOpcode() == LinxV5::PseudoMAMULBMXAC_SizeI) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                          .addOperand(MCOperand::createImm(0))
                          .addOperand(Inst.getOperand(10))
                          .addOperand(Inst.getOperand(11)));
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                          .addOperand(MCOperand::createImm(0))
                          .addOperand(Inst.getOperand(12))
                          .addOperand(Inst.getOperand(13)));
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                          .addOperand(Inst.getOperand(0))
                          .addOperand(MCOperand::createImm(1))
                          .addOperand(Inst.getOperand(9))
                          .addOperand(Inst.getOperand(14)));
      break;
    }
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(Inst.getOperand(10))
                        .addOperand(Inst.getOperand(11)));
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(9))
                        .addOperand(Inst.getOperand(12))
                        .addOperand(Inst.getOperand(13)));
    break;
  case LinxV5::PseudoMAMULBMXB_SizeI:
  case LinxV5::PseudoMAMULBMXBAC_SizeI:
  case LinxV5::PseudoMAMULBMXBACC_SizeI:
    if (Inst.getOpcode() == LinxV5::PseudoMAMULBMXBAC_SizeI) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                          .addOperand(MCOperand::createImm(0))
                          .addOperand(Inst.getOperand(10))
                          .addOperand(Inst.getOperand(11)));
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                          .addOperand(Inst.getOperand(0))
                          .addOperand(MCOperand::createImm(1))
                          .addOperand(Inst.getOperand(9))
                          .addOperand(Inst.getOperand(12))
                          .addOperand(Inst.getOperand(13)));
      break;
    }
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(Inst.getOperand(10))
                        .addOperand(Inst.getOperand(11)));
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(9))
                        .addOperand(Inst.getOperand(12)));
    break;
  case LinxV5::PseudoACCCVT_SizeI:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(7)));
    break;
  case LinxV5::PseudoESAVE:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(1)));
    break;
  case LinxV5::PseudoERCOV:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::PseudoEmptyTile:
  case LinxV5::PseudoEmptyTileASM:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(MCOperand::createImm(0)));
    break;
  case LinxV5::PseudoTMOV_SizeI:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(8))
                        .addOperand(Inst.getOperand(9)));
    break;
  case LinxV5::PseudoTCOPY:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(1))
                        .addOperand(Inst.getOperand(2)));
    break;
  case LinxV5::PseudoTSTORE_noDsrc_noDdst:
  case LinxV5::PseudoTSTORE_noDsrc_Ddst:
  case LinxV5::PseudoTSTORE_Dsrc_noDdst:
  case LinxV5::PseudoTSTORE_Dsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::PseudoTLOAD_noDsrc_noDdst:
  case LinxV5::PseudoTLOAD_noDsrc_Ddst:
  case LinxV5::PseudoTLOAD_Dsrc_noDdst:
  case LinxV5::PseudoTLOAD_Dsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(8)));
    break;
  }
  return McVec;
}

bool isZeroRegAndOneImm(MCOperand &RegOp, MCOperand &ImmOp) {
  if (!EnableDimOpt)
    return false;

  // Determine reg zero.
  bool isZeroReg = RegOp.isReg() && RegOp.getReg() == LinxV5::R0;

  // Determine imm 1.
  bool isZeroImm = ImmOp.isImm() && ImmOp.getImm() == 1;

  return (isZeroReg && isZeroImm);
}

bool isZeroRegAndOneImm(const MCInst &Inst, unsigned i) {
  if (i + 1 >= Inst.getNumOperands())
    return false;

  const MCOperand &RegOp = Inst.getOperand(i);
  const MCOperand &ImmOp = Inst.getOperand(i + 1);

  // Determine reg zero.
  bool isZeroReg = RegOp.isReg() && RegOp.getReg() == LinxV5::R0;

  // Determine imm 0.
  bool isZeroImm = ImmOp.isImm() && ImmOp.getImm() == 0;

  return (isZeroReg && isZeroImm);
}

llvm::SmallVector<MCInst> getBDIMFromInst(MCInst Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;
  using namespace LinxV5;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;

  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    TileCallReader CallReader(Inst, MII);
    auto RegM = CallReader.getDimMReg();
    auto ImmM = CallReader.getDimMImm();

    auto RegN = CallReader.getDimNReg();
    auto ImmN = CallReader.getDimNImm();

    auto RegK = CallReader.getDimKReg();
    auto ImmK = CallReader.getDimKImm();

    // ->lb0
    if (!isZeroRegAndOneImm(RegM, ImmM))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(0))
                          .addOperand(RegM)
                          .addOperand(ImmM));

    // ->lb1
    if (!isZeroRegAndOneImm(RegN, ImmN))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(1))
                          .addOperand(RegN)
                          .addOperand(ImmN));

    // ->lb2
    if (!isZeroRegAndOneImm(RegK, ImmK))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(2))
                          .addOperand(RegK)
                          .addOperand(ImmK));
    return McVec;
  }
  switch (Inst.getOpcode()) {
  case PseudoACCCVT_SizeI:
  case PseudoACCCVT_SizeR:
  case PseudoTMOV_SizeI: {
    // ->lb0
    McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(Inst.getOperand(1))
                        .addOperand(Inst.getOperand(2)));

    // ->lb1
    McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(3))
                        .addOperand(Inst.getOperand(4)));
    break;
  }
  case PseudoTLOAD_noDsrc_noDdst:
  case PseudoTSTORE_noDsrc_noDdst:
  case PseudoTLOAD_noDsrc_Ddst:
  case PseudoTSTORE_noDsrc_Ddst:
  case PseudoTLOAD_Dsrc_noDdst:
  case PseudoTSTORE_Dsrc_noDdst:
  case PseudoTLOAD_Dsrc_Ddst:
  case PseudoTSTORE_Dsrc_Ddst:
  case PseudoMAMULB_SizeI:
  case PseudoMAMULBAC_SizeI:
  case PseudoMAMULBACC_SizeI:
  case PseudoMAMULBMX_SizeI:
  case PseudoMAMULBMXB_SizeI:
  case PseudoMAMULBMXAC_SizeI:
  case PseudoMAMULBMXBAC_SizeI:
  case PseudoMAMULBMXACC_SizeI:
  case PseudoMAMULBMXBACC_SizeI: {
    // ->lb0
    if (!isZeroRegAndOneImm(Inst, 1))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(0))
                          .addOperand(Inst.getOperand(1))
                          .addOperand(Inst.getOperand(2)));

    // ->lb1
    if (!isZeroRegAndOneImm(Inst, 3))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(1))
                          .addOperand(Inst.getOperand(3))
                          .addOperand(Inst.getOperand(4)));
    // ->lb2
    if (!isZeroRegAndOneImm(Inst, 5))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(2))
                          .addOperand(Inst.getOperand(5))
                          .addOperand(Inst.getOperand(6)));
    break;
  }
  }
  return McVec;
}

llvm::SmallVector<MCInst> getBIORFromInst(MCInst Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;

  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    TileCallReader CallReader(Inst, MII);
    if (CallReader.HasRegInList()) {
      llvm::SmallVector<unsigned> GPRInList = CallReader.getGPRInList();
      McVec = getBIORFromInst(Inst, GPRInList);
    }
    return McVec;
  }
  switch (Inst.getOpcode()) {
  default:
    break;
  case LinxV5::PseudoTLOAD_Dsrc_noDdst:
  case LinxV5::PseudoTLOAD_Dsrc_Ddst:
  case LinxV5::PseudoTLOAD_noDsrc_noDdst:
  case LinxV5::PseudoTLOAD_noDsrc_Ddst:
  case LinxV5::PseudoTSTORE_Dsrc_noDdst:
  case LinxV5::PseudoTSTORE_Dsrc_Ddst:
  case LinxV5::PseudoTSTORE_noDsrc_noDdst:
  case LinxV5::PseudoTSTORE_noDsrc_Ddst:
    llvm::SmallVector<unsigned> GPRInList;
    for (int i = Inst.getNumOperands() - 1; i >= 0; i--) {
      auto CurMO = Inst.getOperand(i);
      if (!CurMO.isReg() ||
          !LinxV5MCRegisterClasses[LinxV5::GRRegClassID].contains(
              CurMO.getReg()))
        break;
      GPRInList.emplace_back(CurMO.getReg());
    }
    // need reverse
    std::reverse(GPRInList.begin(), GPRInList.end());
    McVec = getBIORFromInst(Inst, GPRInList);
    break;
  }
  return McVec;
}

llvm::SmallVector<MCInst> getBTEXTTFromInst(MCInst Inst, const MCInstrInfo &MII) {
  using namespace LinxV5;
  llvm::SmallVector<MCInst> McVec;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    TileCallReader CallReader(Inst, MII);
    McVec.push_back(
        MCInstBuilder(LinxV5::BTEXT).addOperand(CallReader.getCallee()));
  }
  return McVec;
}

llvm::SmallVector<MCInst> getBIODFromInst(MCInst Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;
  using namespace LinxV5;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    TileCallReader CallReader(Inst, MII);
    bool HasDepSrc = CallReader.getDepSrcNum() > 0;
    bool HasDepDst = CallReader.HasDepOutput();
    if (HasDepSrc && HasDepDst) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(CallReader.getDepSrc(0)));
      return McVec;
    }

    if (HasDepSrc && !HasDepDst) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Nodst)
                        .addOperand(CallReader.getDepSrc(0)));
      return McVec;
    }

    if (!HasDepSrc && HasDepDst) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(MCOperand::createReg(0)));
      return McVec;
    }

    return McVec;
  }

  switch (Inst.getOpcode()) {
  case PseudoTLOAD_noDsrc_Ddst:
  case PseudoTSTORE_noDsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(MCOperand::createReg(0)));

    break;
  case PseudoTSTORE_Dsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(Inst.getOperand(8)));
    break;
  case PseudoTLOAD_Dsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(Inst.getOperand(9)));
    break;

  case PseudoTSTORE_Dsrc_noDdst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Nodst)
                        .addOperand(Inst.getOperand(8)));
    break;
  case PseudoTLOAD_Dsrc_noDdst:
    McVec.push_back(
        MCInstBuilder(LinxV5::B_IOD_Nodst).addOperand(Inst.getOperand(9)));

    break;
  }
  return McVec;
}

unsigned getPseudoTILEOpcode(unsigned Opcode) {
  static const llvm::DenseMap<unsigned, unsigned> PseudoToOpc = {
      {LinxV5::PseudoMAMULB_SizeI, LinxV5Op::TileOPCUBE::MAMULB},
      {LinxV5::PseudoMAMULBAC_SizeI, LinxV5Op::TileOPCUBE::MAMULBAC},
      {LinxV5::PseudoMAMULBMX_SizeI, LinxV5Op::TileOPCUBE::MAMULBMX},
      {LinxV5::PseudoMAMULBMXB_SizeI, LinxV5Op::TileOPCUBE::MAMULBMX},
      {LinxV5::PseudoMAMULBMXAC_SizeI, LinxV5Op::TileOPCUBE::MAMULBMXAC},
      {LinxV5::PseudoMAMULBMXBAC_SizeI, LinxV5Op::TileOPCUBE::MAMULBMXAC},
      {LinxV5::PseudoMAMULBMXACC_SizeI, LinxV5Op::TileOPCUBE::MAMULBMX_ACC},
      {LinxV5::PseudoMAMULBMXBACC_SizeI, LinxV5Op::TileOPCUBE::MAMULBMX_ACC},
      {LinxV5::PseudoTLOAD_noDsrc_noDdst, LinxV5Op::TileOPTMA::TLOAD},
      {LinxV5::PseudoTSTORE_noDsrc_noDdst, LinxV5Op::TileOPTMA::TSTORE},
      {LinxV5::PseudoTLOAD_noDsrc_Ddst, LinxV5Op::TileOPTMA::TLOAD},
      {LinxV5::PseudoTSTORE_noDsrc_Ddst, LinxV5Op::TileOPTMA::TSTORE},
      {LinxV5::PseudoTLOAD_Dsrc_noDdst, LinxV5Op::TileOPTMA::TLOAD},
      {LinxV5::PseudoTSTORE_Dsrc_noDdst, LinxV5Op::TileOPTMA::TSTORE},
      {LinxV5::PseudoTLOAD_Dsrc_Ddst, LinxV5Op::TileOPTMA::TLOAD},
      {LinxV5::PseudoTSTORE_Dsrc_Ddst, LinxV5Op::TileOPTMA::TSTORE},
      {LinxV5::PseudoMAMULBACC_SizeI, LinxV5Op::TileOPCUBE::MAMULB_ACC},
      {LinxV5::PseudoACCCVT_SizeI, LinxV5Op::TileOPCUBE::ACCCVT},
      {LinxV5::PseudoACCCVT_SizeR, LinxV5Op::TileOPCUBE::ACCCVT},
      {LinxV5::PseudoESAVE, LinxV5Op::TileOPTEPL::ESAVE},
      {LinxV5::PseudoERCOV, LinxV5Op::TileOPTEPL::ERCOV}};

  return PseudoToOpc.lookup(Opcode);
}

bool isMatmulPseudo(unsigned Opcode) {
  switch (Opcode) {
  case LinxV5::PseudoMAMULB_SizeI:
  case LinxV5::PseudoMAMULBAC_SizeI:
  case LinxV5::PseudoMAMULBACC_SizeI:
  case LinxV5::PseudoMAMULBMX_SizeI:
  case LinxV5::PseudoMAMULBMXB_SizeI:
  case LinxV5::PseudoMAMULBMXAC_SizeI:
  case LinxV5::PseudoMAMULBMXBAC_SizeI:
  case LinxV5::PseudoMAMULBMXACC_SizeI:
  case LinxV5::PseudoMAMULBMXBACC_SizeI:
    return true;
  default:
    return false;
  }
}

} // namespace llvm