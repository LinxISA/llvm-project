//===- LinxV5TileOpExpand.h - ----------------------- -----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===--------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_TILE_OP_EXPAND_H
#define LLVM_LIB_TARGET_LINXV5_TILE_OP_EXPAND_H

#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/MC/MCInstrInfo.h"

namespace llvm {

llvm::SmallVector<MCInst> getBARGFromInst(const MCInst &Inst, const MCInstrInfo &MII);

llvm::SmallVector<MCInst> getBIORFromInstByIndex(MCInst Inst, int inIndex);

llvm::SmallVector<MCInst> getBIORFromInst(MCInst Inst, llvm::SmallVector<unsigned> inVec);

llvm::SmallVector<MCInst> getBIOTFromInst(MCInst Inst, const MCInstrInfo &MII);

llvm::SmallVector<MCInst> getBATTRFromInst(MCInst Inst, const MCInstrInfo &MII);

llvm::SmallVector<MCInst> getBDIMFromInst(MCInst Inst, const MCInstrInfo &MII);

llvm::SmallVector<MCInst> getBIODFromInst(MCInst Inst, const MCInstrInfo &MII);

llvm::SmallVector<MCInst> getBIORFromInst(MCInst Inst, const MCInstrInfo &MII);

llvm::SmallVector<MCInst> getBTEXTTFromInst(MCInst Inst, const MCInstrInfo &MII);

unsigned getPseudoTILEOpcode(unsigned Opcode);

bool isActiveMatrixPseudo(unsigned Opcode);

unsigned getRepresentMCallOpcode(unsigned Opcode);

void getPseudoCallBIOTBySrcDstNum(llvm::SmallVector<MCInst> &McVec,
                                  MCInst Inst, const MCInstrInfo &MII);

} // namespace llvm
#endif
