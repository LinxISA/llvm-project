//===-- LinxV5MCTargetDesc.h - LinxV5 Target Descriptions ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides LinxV5 specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LinxV5_MCTARGETDESC_LinxV5MCTARGETDESC_H
#define LLVM_LIB_TARGET_LinxV5_MCTARGETDESC_LinxV5MCTARGETDESC_H

#include "llvm/Config/config.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class Target;

MCCodeEmitter *createLinxV5MCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx);

MCAsmBackend *createLinxV5AsmBackend(const Target &T,
                                     const MCSubtargetInfo &STI,
                                     const MCRegisterInfo &MRI,
                                     const MCTargetOptions &Options);

MCAsmBackend *createLinxV5beAsmBackend(const Target &T,
                                       const MCSubtargetInfo &STI,
                                       const MCRegisterInfo &MRI,
                                       const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter> createLinxV5ELFObjectWriter(uint8_t OSABI,
                                                                  bool Is64Bit);

} // namespace llvm

// Defines symbolic names
#define GET_REGINFO_ENUM
#include "LinxV5GenRegisterInfo.inc"

// Defines symbolic names
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "LinxV5GenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "LinxV5GenSubtargetInfo.inc"

#endif
