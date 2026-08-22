//===-- LinxISA.cpp - LinxISA-specific CodeGen hooks ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"
#include "llvm/IR/DerivedTypes.h"

using namespace clang;
using namespace clang::CodeGen;

namespace {

class LinxISATargetCodeGenInfo final : public TargetCodeGenInfo {
public:
  explicit LinxISATargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<DefaultABIInfo>(CGT)) {}

  llvm::Type *adjustInlineAsmType(CodeGenFunction &CGF, StringRef Constraint,
                                  llvm::Type *Ty) const override {
    Constraint.consume_front("&");
    if (Constraint == "Sr" || Constraint == "^Sr")
      return Ty->isIntegerTy(64) ? Ty : nullptr;
    if (Constraint != "Tr" && Constraint != "^Tr")
      return Ty;

    auto *VT = dyn_cast<llvm::FixedVectorType>(Ty);
    if (!VT || VT->getPrimitiveSizeInBits() != 32768)
      return nullptr;

    return llvm::FixedVectorType::get(CGF.Int32Ty, 1024);
  }
};

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createLinxISATargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<LinxISATargetCodeGenInfo>(CGM.getTypes());
}
