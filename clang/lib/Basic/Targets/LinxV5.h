//===----------------------------------------------------------------------===//
//
// Copyright (C) 2023. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_LINXV5_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_LINXV5_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/LinxV5ISAInfo.h"

namespace clang {
namespace targets {

class LinxV5TargetInfo : public TargetInfo {
public:
  LinxV5TargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    LongDoubleWidth = 128;
    LongDoubleAlign = 128;
    LongDoubleFormat = &llvm::APFloat::IEEEquad();
    MaxVectorAlign = 256;
    SuitableAlign = 128;
    WCharType = SignedInt;
    WIntType = UnsignedInt;
  }

  bool setCPU(const std::string &Name) override {
    if (!isValidCPUName(Name))
      return false;
    CPU = Name;
    return true;
  }

  bool isValidCPUName(StringRef Name) const override;
  StringRef getABI() const override { return ABI; }
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  ArrayRef<Builtin::Info> getTargetBuiltins() const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  const char *getClobbers() const override { return ""; }

  ArrayRef<const char *> getGCCRegNames() const override;

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  std::string convertConstraint(const char *&Constraint) const override {
    std::string R;
    switch (*Constraint) {
    case 'v':
    case 'T':
      if (Constraint[1] == 'r')
        R = std::string("@2") + std::string(Constraint, 2);
      Constraint += 1;
      break;
    default:
      R = TargetInfo::convertConstraint(Constraint);
      break;
    }
    return R;
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override;

  bool hasFeature(StringRef Feature) const override;

  bool handleTargetFeatures(std::vector<std::string> &Features,
                            DiagnosticsEngine &Diags) override;

  int getEHDataRegisterNumber(unsigned RegNo) const override {
    if (RegNo == 0)
      return 2; // a0
    else if (RegNo == 1)
      return 3; // a1
    return -1;
  }

protected:
  std::string ABI, CPU;
  bool HasM = false;

  static const Builtin::Info BuiltinInfo[];
};

class LLVM_LIBRARY_VISIBILITY Linx64V5TargetInfo : public LinxV5TargetInfo {
public:
  Linx64V5TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : LinxV5TargetInfo(Triple, Opts) {
    LongWidth = LongAlign = PointerWidth = PointerAlign = 64;
    IntMaxType = Int64Type = SignedLong;
    MinGlobalAlign = 64;

    BFloat16Width = BFloat16Align = 16;
    BFloat16Format = &llvm::APFloat::BFloat();

    resetDataLayout(
        (Twine(Triple.isLittleEndian() ? "e" : "E") +
         "-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128")
            .str());
  }

  bool setABI(const std::string &Name) override {
    if (Name == "lp64") {
      ABI = Name;
      return true;
    }
    return false;
  }

  void setMaxAtomicWidth() override {
    //TODO: The V4 atomic command is temporarily disabled and will be enabled later.
    MaxAtomicPromoteWidth = 128;
    MaxAtomicInlineWidth = 64;
  }

  bool hasBFloat16Type() const override {
    return true;
  }

  const char *getBFloat16Mangling() const override { return "u6__bf16"; }
};
} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_LINXV5_H
