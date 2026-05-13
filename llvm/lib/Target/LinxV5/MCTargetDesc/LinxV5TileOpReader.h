//===- LinxV5TileOpReader.h - ----------------------- -----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===--------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_TILEOP_READER_H
#define LLVM_LIB_TARGET_LINXV5_TILEOP_READER_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"

namespace llvm {
class TileCallReader {
public:
  explicit TileCallReader(MCInst &MI, const MCInstrInfo &MII);

  unsigned getTileSrcNum();
  unsigned getTileDstNum();
  unsigned getDepSrcNum();

  MCOperand getS();
  MCOperand getStackSize();

  MCOperand getTileSize(unsigned index);
  MCOperand getDepSrc(unsigned index);
  MCOperand getTileSrc(unsigned index);
  MCOperand getTileDst(unsigned index);
  llvm::SmallVector<unsigned> getGPRInList();
  MCOperand getDR();
  MCOperand getCallee();
  MCOperand getDimMReg();
  MCOperand getDimMImm();
  MCOperand getDimNReg();
  MCOperand getDimNImm();
  MCOperand getDimKReg();
  MCOperand getDimKImm();

  bool HasRegInList();
  bool HasDepOutput();
  bool IsTileSizeReg();
  bool HasDR();
  bool HasS();

private:
  unsigned TileDstNum = 0;
  unsigned TileSrcNum = 0;
  unsigned DepSrcNum = 0;

  MCOperand CallLabel;
  MCOperand RegM;
  MCOperand ImmM;
  MCOperand RegN;
  MCOperand ImmN;
  MCOperand RegK;
  MCOperand ImmK;
  MCOperand ImmDR;
  MCOperand S;
  MCOperand StackSize;

  llvm::SmallVector<unsigned> GPRInList;
  llvm::SmallVector<MCOperand> DepSrcs = {};
  llvm::SmallVector<MCOperand> DstTiles = {};
  llvm::SmallVector<MCOperand> SrcTiles = {};
  llvm::SmallVector<MCOperand> DstTileSizes = {};

  bool hasGetList = false;
  // we assume TileSize only can be Imm
  bool isTileSizeReg = false;
  bool hasDepOutput = false;
  bool hasDR = false;
  bool hasS = false;
};

// TODO
class PseudoTCOPYReader {

};

} // namespace llvm
#endif