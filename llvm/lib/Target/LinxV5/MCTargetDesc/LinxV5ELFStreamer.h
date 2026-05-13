//===-- LinxV5ELFStreamer.h - LinxV5 ELF Target Streamer ------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LinxV5_MCTARGETDESC_LinxV5ELFSTREAMER_H
#define LLVM_LIB_TARGET_LinxV5_MCTARGETDESC_LinxV5ELFSTREAMER_H

#include "LinxV5TargetStreamer.h"
#include "llvm/MC/MCELFStreamer.h"

namespace llvm {

class LinxV5TargetELFStreamer : public LinxV5TargetStreamer {
private:
  enum class AttributeType { Hidden, Numeric, Text, NumericAndText };

  struct AttributeItem {
    AttributeType Type;
    unsigned Tag;
    unsigned IntValue;
    std::string StringValue;
  };

  StringRef CurrentVendor;
  SmallVector<AttributeItem, 64> Contents;

  MCSection *AttributeSection = nullptr;
  const MCSubtargetInfo &STI;

  AttributeItem *getAttributeItem(unsigned Attribute) {
    for (size_t i = 0; i < Contents.size(); ++i)
      if (Contents[i].Tag == Attribute)
        return &Contents[i];
    return nullptr;
  }

  void setAttributeItem(unsigned Attribute, unsigned Value,
                        bool OverwriteExisting) {
    // Look for existing attribute item.
    if (AttributeItem *Item = getAttributeItem(Attribute)) {
      if (!OverwriteExisting)
        return;
      Item->Type = AttributeType::Numeric;
      Item->IntValue = Value;
      return;
    }

    // Create new attribute item.
    Contents.push_back({AttributeType::Numeric, Attribute, Value, ""});
  }

  void setAttributeItem(unsigned Attribute, StringRef Value,
                        bool OverwriteExisting) {
    // Look for existing attribute item.
    if (AttributeItem *Item = getAttributeItem(Attribute)) {
      if (!OverwriteExisting)
        return;
      Item->Type = AttributeType::Text;
      Item->StringValue = std::string(Value);
      return;
    }

    // Create new attribute item.
    Contents.push_back({AttributeType::Text, Attribute, 0, std::string(Value)});
  }

  void setAttributeItems(unsigned Attribute, unsigned IntValue,
                         StringRef StringValue, bool OverwriteExisting) {
    // Look for existing attribute item.
    if (AttributeItem *Item = getAttributeItem(Attribute)) {
      if (!OverwriteExisting)
        return;
      Item->Type = AttributeType::NumericAndText;
      Item->IntValue = IntValue;
      Item->StringValue = std::string(StringValue);
      return;
    }

    // Create new attribute item.
    Contents.push_back({AttributeType::NumericAndText, Attribute, IntValue,
                        std::string(StringValue)});
  }

  void emitAttribute(unsigned Attribute, unsigned Value) override;
  void emitTextAttribute(unsigned Attribute, StringRef String) override;
  void finishAttributeSection() override;

  void reset() override;

public:
  MCELFStreamer &getStreamer();
  LinxV5TargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);

  void emitDirectiveOptionRelax() override;
  void emitDirectiveOptionNoRelax() override;
  void emitRawText(StringRef Str) override;
  void finish() override;
};

MCELFStreamer *createLinxV5ELFStreamer(MCContext &C,
                                       std::unique_ptr<MCAsmBackend> MAB,
                                       std::unique_ptr<MCObjectWriter> MOW,
                                       std::unique_ptr<MCCodeEmitter> MCE,
                                       bool RelaxAll);

} // namespace llvm
#endif
