//===- LinxV5CompressInst.h - ----------------------- -----*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===--------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_COMPRESS_INST_H
#define LLVM_LIB_TARGET_LINXV5_COMPRESS_INST_H

#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"

namespace llvm {
namespace LinxV5 {

bool tryCompressInst(MCInst &OutInst, const MCInst &MI,
                     const MCSubtargetInfo &STI, MCContext &Context);

bool unCompressInst(MCInst &OutInst, const MCInst &MI,
                    const MCRegisterInfo &MRI, const MCSubtargetInfo &STI);

} // namespace LinxV5
} // namespace llvm
#endif
