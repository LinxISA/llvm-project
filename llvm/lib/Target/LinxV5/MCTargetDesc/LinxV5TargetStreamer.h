//===-- LinxV5TargetStreamer.h - LinxV5 Target Streamer -------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_LINXV5TARGETSTREAMER_H
#define LLVM_LIB_TARGET_LINXV5_LINXV5TARGETSTREAMER_H

#include "LinxV5.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"

namespace llvm {

class formatted_raw_ostream;

class LinxV5TargetStreamer : public MCTargetStreamer {
  LinxV5ABI::ABI TargetABI = LinxV5ABI::ABI_Unknown;

public:
  LinxV5TargetStreamer(MCStreamer &S);
  void finish() override;
  virtual void reset();

  virtual void emitDirectiveOptionRelax();
  virtual void emitDirectiveOptionNoRelax();
  virtual void emitAttribute(unsigned Attribute, unsigned Value);
  virtual void finishAttributeSection();
  virtual void emitTextAttribute(unsigned Attribute, StringRef String);
  virtual void emitRawText(StringRef Str);

  void setTargetABI(LinxV5ABI::ABI ABI);
  LinxV5ABI::ABI getTargetABI() const { return TargetABI; }
};

// This part is for ascii assembly output
class LinxV5TargetAsmStreamer : public LinxV5TargetStreamer {
  formatted_raw_ostream &OS;

  void emitAttribute(unsigned Attribute, unsigned Value) override;
  void finishAttributeSection() override;
  void emitTextAttribute(unsigned Attribute, StringRef String) override;

public:
  LinxV5TargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS);
  void emitDirectiveOptionRelax() override;
  void emitDirectiveOptionNoRelax() override;
  void emitRawText(StringRef Str) override;
  ~LinxV5TargetAsmStreamer() override = default;
};
}
#endif
