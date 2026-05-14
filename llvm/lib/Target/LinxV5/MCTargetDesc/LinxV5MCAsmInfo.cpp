//===-- LinxV5MCAsmInfo.cpp - LinxV5 Asm properties ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the LinxV5MCAsmInfo properties.
//
//===----------------------------------------------------------------------===//

#include "LinxV5MCAsmInfo.h"
#include "MCTargetDesc/LinxV5MCExpr.h"
#include "llvm/ADT/Triple.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/MC/MCStreamer.h"
using namespace llvm;

LinxV5MCAsmInfo::LinxV5MCAsmInfo(const Triple &TargetTriple) {
  if (TargetTriple.getArch() == Triple::linx64v5be)
    IsLittleEndian = false;
  CodePointerSize = CalleeSaveStackSlotSize =
      TargetTriple.isArch64Bit() ? 8 : 4;
  CommentString = "//";
  AlignmentIsInBytes = false;
  SupportsDebugInformation = true;
  ExceptionsType = ExceptionHandling::DwarfCFI;
  Data16bitsDirective = "\t.half\t";
  Data32bitsDirective = "\t.word\t";
  UseIntegratedAssembler = true;
}

const MCExpr *LinxV5MCAsmInfo::getExprForFDESymbol(const MCSymbol *Sym,
                                                   unsigned Encoding,
                                                   MCStreamer &Streamer) const {
  if (!(Encoding & dwarf::DW_EH_PE_pcrel))
    return MCAsmInfo::getExprForFDESymbol(Sym, Encoding, Streamer);

  // The default symbol subtraction results in an ADD/SUB relocation pair.
  // Processing this relocation pair is problematic when linker relaxation is
  // enabled, so we follow binutils in using the R_LinxV5_32_PCREL relocation
  // for the FDE initial location.
  MCContext &Ctx = Streamer.getContext();
  const MCExpr *ME =
      MCSymbolRefExpr::create(Sym, MCSymbolRefExpr::VK_None, Ctx);
  assert((Encoding & dwarf::DW_EH_PE_sdata4) && "Unexpected encoding");
  return LinxV5MCExpr::create(ME, LinxV5MCExpr::VK_LinxV5_32_TPCREL, Ctx);
}
