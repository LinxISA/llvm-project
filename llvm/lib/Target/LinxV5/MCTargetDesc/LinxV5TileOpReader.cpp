//===-- LinxV5TileOpReader.cpp - LinxV5 Assembler Backend
//------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5TileOpReader.h"
#include "LinxV5BaseInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "LinxV5GenRegisterInfo.inc"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCInst.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/MC/MCInstrInfo.h"

namespace llvm {

/* ----------------------- TileCallReader----------------------------
 * --------Operands---------OpType--------Number----------(RegTy)------
 * {DstTile0， DstTile1},     Reg     getTileDstNum()
 * Label,                     Expr           1
 * RegM,                      Reg            1
 * ImmM,                      Imm            1
 * RegN,                      Reg            1
 * ImmN,                      Imm            1
 * RegK,                      Reg            1
 * ImmK,                      Imm            1
 * {TileSize0, TileSize1}     Reg/Imm getTileDstNum()
 * {SrcTile0, SrcTile1}       Reg     getTileSrcNum()    TILE_SRCReg
 * {DepSrc},                  Reg     getDepSrcNum()     Dep_SRCReg
 * {DR},                      Imm     getDR()
 * {S}                        Reg            1
 * {StackSize}                Imm/Expr    getStackSize()
    ...
 * {RegInList},               RegList     HasRegInList()       (------always at the end-----)
 */
TileCallReader::TileCallReader(MCInst &MI, const MCInstrInfo &MII) {
  hasDepOutput = MII.get(MI.getOpcode()).TSFlags &
      llvm::LinxV5II::HasDepOutputMask;
  for (unsigned i = 0, e = MI.getNumOperands(); i != e; ++i) {
    if (!MI.getOperand(i).isReg()) {
      TileDstNum = i;
      break;
    }
  }
  unsigned BaseIndex = 0;
  for (unsigned i = 0; i < TileDstNum; i++) {
    DstTiles.emplace_back(MI.getOperand(i));
    ++BaseIndex;
  }

  CallLabel = MI.getOperand(BaseIndex);
  RegM = MI.getOperand(++BaseIndex);
  ImmM = MI.getOperand(++BaseIndex);
  RegN = MI.getOperand(++BaseIndex);
  ImmN = MI.getOperand(++BaseIndex);
  RegK = MI.getOperand(++BaseIndex);
  ImmK = MI.getOperand(++BaseIndex);

  for (unsigned i = 0; i < TileDstNum; i++) {
    auto MO = MI.getOperand(++BaseIndex);
    DstTileSizes.emplace_back(MO);
  }

  if (BaseIndex >= MI.getNumOperands() - 1)
    return;
  ++BaseIndex;

  MCOperand CurMO;
  while (BaseIndex < MI.getNumOperands()) {
    CurMO = MI.getOperand(BaseIndex);
    if (!CurMO.isReg() ||
        !LinxV5MCRegisterClasses[LinxV5::TILE_SRCRegClassID].contains(
            CurMO.getReg()))
      break;
    SrcTiles.emplace_back(CurMO);
    ++TileSrcNum;
    ++BaseIndex;
  }

  while (BaseIndex < MI.getNumOperands()) {
    CurMO = MI.getOperand(BaseIndex);
    if (!CurMO.isReg() ||
        !LinxV5MCRegisterClasses[LinxV5::Dep_SRCRegClassID].contains(
            CurMO.getReg())) {
      break;
    }
    DepSrcs.emplace_back(CurMO);
    ++DepSrcNum;
    ++BaseIndex;
  }

  assert(MI.getOperand(BaseIndex).isImm() &&
         "TileCallReader read DR error, Please check");
  ImmDR = MI.getOperand(BaseIndex);
  hasDR = true;
  ++BaseIndex;

  if (BaseIndex < MI.getNumOperands() && MI.getOperand(BaseIndex).isReg() &&
      LinxV5MCRegisterClasses[LinxV5::TILE_DSTSRegClassID].contains(
          MI.getOperand(BaseIndex).getReg())) {
    S = MI.getOperand(BaseIndex++);
    StackSize = MI.getOperand(BaseIndex++);
    hasS = true;
  }

  while (BaseIndex < MI.getNumOperands()) {
    CurMO = MI.getOperand(BaseIndex);
    if (!CurMO.isReg() ||
        !LinxV5MCRegisterClasses[LinxV5::GRRegClassID].contains(
            CurMO.getReg())) {
      break;
    }
    hasGetList = true;
    GPRInList.emplace_back(CurMO.getReg());
    ++BaseIndex;
  }

  assert(BaseIndex == MI.getNumOperands() &&
         "TileCallReader read num error, Please check");
}

unsigned TileCallReader::getTileSrcNum() { return TileSrcNum; }

unsigned TileCallReader::getTileDstNum() { return TileDstNum; }

unsigned TileCallReader::getDepSrcNum() { return DepSrcNum; }

MCOperand TileCallReader::getTileSize(unsigned index) {
  assert(index < TileDstNum && "invalid getTileSize: index > TileDstNum");
  return DstTileSizes[index];
}

MCOperand TileCallReader::getTileSrc(unsigned index) {
  assert(index < TileSrcNum && "invalid getTileSrc: index > TileSrcNum");
  return SrcTiles[index];
}

MCOperand TileCallReader::getTileDst(unsigned index) {
  assert(index < TileDstNum && "invalid getTileDst: index > TileDstNum");
  return DstTiles[index];
}

MCOperand TileCallReader::getS() { return S; }
MCOperand TileCallReader::getStackSize() { return StackSize; }

MCOperand TileCallReader::getDepSrc(unsigned index) {
  assert(index < DepSrcNum && "invalid getDepSrc: index > DepSrcNum");
  return DepSrcs[index];
}

llvm::SmallVector<unsigned> TileCallReader::getGPRInList() { return GPRInList; }

MCOperand TileCallReader::getDR() { return ImmDR; }

MCOperand TileCallReader::getCallee() { return CallLabel; }

MCOperand TileCallReader::getDimMReg() { return RegM; }

MCOperand TileCallReader::getDimMImm() { return ImmM; }

MCOperand TileCallReader::getDimNReg() { return RegN; }

MCOperand TileCallReader::getDimNImm() { return ImmN; }

MCOperand TileCallReader::getDimKReg() { return RegK; }

MCOperand TileCallReader::getDimKImm() { return ImmK; }

bool TileCallReader::IsTileSizeReg() { return isTileSizeReg; }

bool TileCallReader::HasRegInList() { return hasGetList; }

bool TileCallReader::HasDR() { return hasDR; }

bool TileCallReader::HasS() { return hasS; }

bool TileCallReader::HasDepOutput() { return hasDepOutput; }

} // namespace llvm end