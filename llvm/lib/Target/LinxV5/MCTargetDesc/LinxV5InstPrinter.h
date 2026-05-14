//===- LinxV5InstPrinter.h - Convert LinxV5 MCInst to asm syntax -*- C++ -*--//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints a LinxV5 MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV4INSTPRINTER_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV4INSTPRINTER_H

#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/MC/MCInstPrinter.h"

namespace llvm {

class LinxV5InstPrinter : public MCInstPrinter {
public:
  LinxV5InstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                    const MCRegisterInfo &MRI)
      : MCInstPrinter(MAI, MII, MRI) {}

  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &O) override;
  void printRegName(raw_ostream &O, unsigned RegNo) const override;

  void printOperand(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                    raw_ostream &O, const char *Modifier = nullptr);
  void printBareBlock(const MCInst *MI, unsigned OpNo,
                      const MCSubtargetInfo &STI, raw_ostream &O);
  void printBranchOperand(const MCInst *MI, uint64_t Address, unsigned OpNo,
                          const MCSubtargetInfo &STI, raw_ostream &O);
  void printLinxV5SrcRShamt(const MCInst *MI, unsigned OpNo,
                            const MCSubtargetInfo &STI, raw_ostream &O);
  void printLinxV5SrcRType(const MCInst *MI, unsigned OpNo,
                           const MCSubtargetInfo &STI, raw_ostream &O);
  void printRegWithSrcRTypeImpl(const MCInst *MI, unsigned OpNo,
                                const MCSubtargetInfo &STI, raw_ostream &O,
                                StringRef TypeName);
  template <class SrcRTypeTraits>
  void printRegWithSrcRType(const MCInst *MI, unsigned OpNo,
                            const MCSubtargetInfo &STI, raw_ostream &O) {
    printRegWithSrcRTypeImpl(MI, OpNo, STI, O, SrcRTypeTraits::Asm);
  }
  template <unsigned S>
  void printImmShifted(const MCInst *MI, unsigned OpNo,
                       const MCSubtargetInfo &STI, raw_ostream &O);
  void printShamtImm(const MCInst *MI, unsigned OpNo,
                     const MCSubtargetInfo &STI, raw_ostream &O);
  template <unsigned S>
  void printShamtImmPlus(const MCInst *MI, unsigned OpNo,
                         const MCSubtargetInfo &STI, raw_ostream &O);
  void printDstRWithArrow(const MCInst *MI, unsigned OpNo,
                          const MCSubtargetInfo &STI, raw_ostream &O);
  void printGroupOp(const MCInst *MI, unsigned OpNo,
                          const MCSubtargetInfo &STI, raw_ostream &O);
  void printSIMTSrcReg(const MCInst *MI, unsigned OpNo,
                          const MCSubtargetInfo &STI, raw_ostream &O);
  void printSIMTRegOp(const MCInst *MI, unsigned OpNo,
                          const MCSubtargetInfo &STI, raw_ostream &O);
  void printLoopBRegDstRWithArrow(const MCInst *MI, unsigned OpNo,
                    const MCSubtargetInfo &STI,raw_ostream &O);
  void printTileSrcReg(const MCInst *MI, unsigned OpNo,
                    const MCSubtargetInfo &STI,raw_ostream &O);
  void printDepSrcReg(const MCInst *MI, unsigned OpNo,
                    const MCSubtargetInfo &STI,raw_ostream &O);
  void printTileDstReg(const MCInst *MI, unsigned OpNo,
                    const MCSubtargetInfo &STI,raw_ostream &O);
  void printGPRWithBracket(const MCInst *MI, unsigned OpNo,
                    const MCSubtargetInfo &STI,raw_ostream &O);
  void printTileSizeWithBracket(const MCInst *MI, unsigned OpNo,
                    const MCSubtargetInfo &STI,raw_ostream &O);
  void printGPRPlusImm(const MCInst *MI, unsigned OpNo,
                       const MCSubtargetInfo &STI, raw_ostream &O);
  void printDRImm(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                  raw_ostream &O);
  void printPlusImm17(const MCInst *MI, unsigned OpNo,
                      const MCSubtargetInfo &STI, raw_ostream &O);
  void printBStartWithoutTargetBrType(const MCInst *MI, unsigned OpNo,
                                      const MCSubtargetInfo &STI,
                                      raw_ostream &O);
  void printBAttrType(const MCInst *MI, unsigned OpNo,
                      const MCSubtargetInfo &STI, raw_ostream &O);
  void printBArgFormat(const MCInst *MI, unsigned OpNo,
                       const MCSubtargetInfo &STI, raw_ostream &O);
  void printRMode(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                  raw_ostream &O);
  void printCanon(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                  raw_ostream &O);
  void printSat(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                raw_ostream &O);
  void printByteID(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                   raw_ostream &O);
  void printPadValue(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                  raw_ostream &O);
  void printCmpMode(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                    raw_ostream &O);
  void printBstartDataType(const MCInst *MI, unsigned OpNo,
                           const MCSubtargetInfo &STI, raw_ostream &O);
  void printTileOPTMA(const MCInst *MI, unsigned OpNo,
                      const MCSubtargetInfo &STI, raw_ostream &O);
  void printTileOPCUBE(const MCInst *MI, unsigned OpNo,
                       const MCSubtargetInfo &STI, raw_ostream &O);
  void printTileOPTEPL(const MCInst *MI, unsigned OpNo,
                       const MCSubtargetInfo &STI, raw_ostream &O);
  void printTileOPMode(const MCInst *MI, unsigned OpNo,
                       const MCSubtargetInfo &STI, raw_ostream &O);
  void printTEPLMode(const MCInst *MI, unsigned OpNo,
                     const MCSubtargetInfo &STI, raw_ostream &O);
  void printPseudoBIO(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                raw_ostream &O);
  void printGPRList(const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
                    raw_ostream &O);
  void printFenceFlag(const MCInst *MI, unsigned OpNo,
                      const MCSubtargetInfo &STI, raw_ostream &O);
  void printSIMTIntSrcRegType(const MCInst *MI, unsigned OpNo,
                              const MCSubtargetInfo &STI, raw_ostream &O);
  void printSIMTFloatSrcRegType(const MCInst *MI, unsigned OpNo,
                                const MCSubtargetInfo &STI, raw_ostream &O);
  void printSIMTDstRegType(const MCInst *MI, unsigned OpNo,
                           const MCSubtargetInfo &STI, raw_ostream &O);
  void printGPRBitMap(const MCInst *MI, unsigned OpNo,
                      const MCSubtargetInfo &STI, raw_ostream &O);
  // Autogenerated by tblgen.
  std::pair<const char *, uint64_t> getMnemonic(const MCInst *MI) override;
  void printInstruction(const MCInst *MI, uint64_t Address,
                        const MCSubtargetInfo &STI, raw_ostream &O);
  bool printAliasInstr(const MCInst *MI, uint64_t Address,
                       const MCSubtargetInfo &STI, raw_ostream &O);
  void printCustomAliasOperand(const MCInst *MI, uint64_t Address,
                               unsigned OpIdx, unsigned PrintMethodIdx,
                               const MCSubtargetInfo &STI, raw_ostream &O);
  static const char *getRegisterName(unsigned RegNo);
  static const char *getRegisterName(unsigned RegNo, unsigned AltIdx);
};
} // namespace llvm

#endif
