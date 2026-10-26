#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5FPATR_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5FPATR_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {
namespace LinxV5FPATR {

inline ArrayRef<StringRef> getFieldNames() {
  static const StringRef Names[] = {
      "PreQuantMode", "ReluMode", "GroupN", "RowMaxEn", "GroupMaxEn",
      "RowMaxInit", "MaxAbsEn", "TransA", "TransB", "CScaleEn"};
  return Names;
}

inline ArrayRef<StringRef> getModeNames(unsigned Operand) {
  static const StringRef PreQuantNames[] = {
      "NoQuant",        "F322F16",        "VREQ8",          "REQ8",
      "VDEQF16",        "DEQF16",         "",               "",
      "",               "",               "",               "",
      "VSHIFTS322S16",  "SHIFTS322S16",   "",               "",
      "F322BF16",       "REQ4",           "VREQ4",          "DEQS16",
      "VDEQS16",        "",               "",               "VQF322B8_PRE",
      "QF322B8_PRE",    "QF322HIF8_PRE",  "QF322FP8_PRE",   "QF322F32_PRE",
      "VQF322HIF8_PRE", "",               "",               "",
      "QF322F16_PRE",   "VQF322F16_PRE",  "QF322BF16_PRE",  "QS322BF16_PRE",
      "VQF322BF16_PRE", "VQF322FP8_PRE",  "VQF322F32_PRE",  "VQS322BF16_PRE"};
  static const StringRef ReluNames[] = {"None", "ReLU", "ScalarLReLU",
                                      "VectorPReLU"};
  if (Operand == 0)
    return PreQuantNames;
  if (Operand == 1)
    return ReluNames;
  return {};
}

inline ArrayRef<unsigned> getGroupWidths() {
  static const unsigned Widths[] = {0, 8, 16, 32, 48, 64, 80, 96, 112, 128};
  return Widths;
}

}
}

#endif
