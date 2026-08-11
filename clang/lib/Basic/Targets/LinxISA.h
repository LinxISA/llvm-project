//===--- LinxISA.h - Declare LinxISA target feature support -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares LinxISA TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_LINXISA_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_LINXISA_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"
#include <vector>

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY LinxISATargetInfo : public TargetInfo {
protected:
  bool HasExtS32;
  bool HasExtS64;
  bool HasExtC;
  bool HasExtF;
  bool HasExtA;
  bool HasExtSys;
  bool HasExtV;
  bool HasExtM;

public:
  LinxISATargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    HasExtS32 = true;
    HasExtS64 = Triple.isArch64Bit();
    HasExtC = false;
    HasExtF = false;
    HasExtA = false;
    HasExtSys = false;
    HasExtV = false;
    HasExtM = false;
    WCharType = SignedInt;
    WIntType = UnsignedInt;
    // Bring-up toolchain convention: `long double` is the same as `double`.
    // This avoids requiring fp128/libquadmath support in freestanding builds.
    LongDoubleWidth = 64;
    LongDoubleAlign = 64;
    LongDoubleFormat = &llvm::APFloat::IEEEdouble();
    BFloat16Width = 16;
    BFloat16Align = 16;
    BFloat16Format = &llvm::APFloat::BFloat();
    HasFloat16 = true;
    SuitableAlign = 64;
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override;

  std::string_view getClobbers() const override { return ""; }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override;

  bool hasFeature(StringRef Feature) const override;
  bool handleTargetFeatures(std::vector<std::string> &Features,
                            DiagnosticsEngine &Diags) override;

  int getEHDataRegisterNumber(unsigned RegNo) const override {
    if (RegNo == 0)
      return 2; // a0
    if (RegNo == 1)
      return 3; // a1
    return -1;
  }

  bool hasBitIntType() const override { return true; }

  bool hasBFloat16Type() const override { return true; }
};

class LLVM_LIBRARY_VISIBILITY Linx32TargetInfo : public LinxISATargetInfo {
public:
  Linx32TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : LinxISATargetInfo(Triple, Opts) {
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    SizeType = UnsignedInt;
    resetDataLayout("e-m:e-p:32:32-i64:64-i128:128-n32-S128");
  }

  void setMaxAtomicWidth() override {
    MaxAtomicPromoteWidth = MaxAtomicInlineWidth = 32;
  }
};

class LLVM_LIBRARY_VISIBILITY Linx64TargetInfo : public LinxISATargetInfo {
public:
  Linx64TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : LinxISATargetInfo(Triple, Opts) {
    LongWidth = LongAlign = PointerWidth = PointerAlign = 64;
    IntMaxType = Int64Type = SignedLong;
    resetDataLayout("e-m:e-p:64:64-i64:64-i128:128-n32:64-S128");
  }

  void setMaxAtomicWidth() override {
    MaxAtomicPromoteWidth = 128;
    MaxAtomicInlineWidth = 64;
  }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_LINXISA_H
