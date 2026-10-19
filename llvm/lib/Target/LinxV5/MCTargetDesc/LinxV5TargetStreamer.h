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
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"

namespace llvm {

class formatted_raw_ostream;

class LinxV5TargetStreamer : public MCTargetStreamer {
  LinxV5ABI::ABI TargetABI = LinxV5ABI::ABI_Unknown;
  unsigned TilePEDomain = 15;
  bool CompilerOwnedTilePEDomain = false;

public:
  LinxV5TargetStreamer(MCStreamer &S);
  void finish() override;
  virtual void reset();

  virtual void emitDirectiveOptionRelax();
  virtual void emitDirectiveOptionNoRelax();
  virtual void emitDirectiveTilePEDomain(unsigned Mask);
  virtual void emitAttribute(unsigned Attribute, unsigned Value);
  virtual void finishAttributeSection();
  virtual void emitTextAttribute(unsigned Attribute, StringRef String);
  virtual void emitRawText(StringRef Str);

  void setTargetABI(LinxV5ABI::ABI ABI);
  LinxV5ABI::ABI getTargetABI() const { return TargetABI; }
  unsigned getTilePEDomain() const { return TilePEDomain; }
  bool isCompilerOwnedTilePEDomain() const {
    return CompilerOwnedTilePEDomain;
  }
  void enterCompilerTilePEDomain(unsigned Mask);
  void leaveCompilerTilePEDomain();
};

namespace LinxV5TilePEDomain {
constexpr unsigned Default = 15;
constexpr unsigned FlagValid = 1u << 4;
constexpr unsigned FlagMask = 0xfu;

inline bool isNativeMask(unsigned Mask) {
  return Mask == 1 || Mask == 2 || Mask == 4 || Mask == 8 || Mask == 12 ||
         Mask == 14 || Mask == 15;
}

inline unsigned encodeFlags(unsigned Mask) { return FlagValid | Mask; }
inline unsigned decodeFlags(unsigned Flags) {
  return (Flags & FlagValid) ? Flags & FlagMask : Default;
}
inline void stamp(MCInst &Inst, unsigned Mask) {
  Inst.setFlags((Inst.getFlags() & ~(FlagValid | FlagMask)) |
                encodeFlags(Mask));
}
inline void inherit(MCInst &Child, const MCInst &Parent) {
  stamp(Child, decodeFlags(Parent.getFlags()));
}
} // namespace LinxV5TilePEDomain

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
  void emitDirectiveTilePEDomain(unsigned Mask) override;
  void emitRawText(StringRef Str) override;
  ~LinxV5TargetAsmStreamer() override = default;
};
}
#endif
