//===----------------------------------------------------------------------===//
//
// Copyright (C) 2023. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/TargetParser.h"

using namespace clang;
using namespace clang::targets;

static const char *const GCCRegNames[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23"};

ArrayRef<const char *> LinxV5TargetInfo::getGCCRegNames() const {
  return llvm::makeArrayRef(GCCRegNames);
}

static const TargetInfo::GCCRegAlias GCCRegAliases[] = {
    {{"zero"}, "r0"}, {{"sp"}, "r1"},  {{"a0"}, "r2"},  {{"a1"}, "r3"},
    {{"a2"}, "r4"},   {{"a3"}, "r5"},  {{"a4"}, "r6"},  {{"a5"}, "r7"},
    {{"a6"}, "r8"},   {{"a7"}, "r9"},  {{"ra"}, "r10"}, {{"s0", "fp"}, "r11"},
    {{"s1"}, "r12"},  {{"s2"}, "r13"}, {{"s3"}, "r14"}, {{"s4"}, "r15"},
    {{"s5"}, "r16"},  {{"s6"}, "r17"}, {{"s7"}, "r18"}, {{"s8"}, "r19"},
    {{"x0"}, "r20"},  {{"x1"}, "r21"}, {{"x2"}, "r22"}, {{"x3"}, "r23"}};

ArrayRef<TargetInfo::GCCRegAlias> LinxV5TargetInfo::getGCCRegAliases() const {
  return llvm::makeArrayRef(GCCRegAliases);
}

// TODO: add more check.
bool LinxV5TargetInfo::isValidCPUName(StringRef Name) const { return true; }

bool LinxV5TargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  switch (Name[0]) {
  case 'v':
  case 'T':
    if (Name[1] == 'r') {
      Info.setAllowsRegister();
      Name++;
      return true;
    }
    return false;
  default:
    break;
  }
  return false;
}

void LinxV5TargetInfo::getTargetDefines(const LangOptions &Opts,
                                        MacroBuilder &Builder) const {
  Builder.defineMacro("__ELF__");
  if (getTriple().getArch() == llvm::Triple::linx64v5be)
    Builder.defineMacro("__linxv5_be");
  Builder.defineMacro("__linx");
  Builder.defineMacro("__linxv5");
  Builder.defineMacro("__linx_v5");
  Builder.defineMacro("__linx_xlen", "64");
  StringRef CodeModel = getTargetOpts().CodeModel;
  if (CodeModel == "default")
    CodeModel = "small";

  if (CodeModel == "small")
    Builder.defineMacro("__linx_cmodel_medlow");
  else if (CodeModel == "medium")
    Builder.defineMacro("__linx_cmodel_medany");

  StringRef ABIName = getABI();
  if (ABIName == "ilp32f" || ABIName == "lp64f")
    Builder.defineMacro("__linx_float_abi_single");
  else if (ABIName == "ilp32d" || ABIName == "lp64d")
    Builder.defineMacro("__linx_float_abi_double");
  else
    Builder.defineMacro("__linx_float_abi_soft");

  if (HasM) {
    Builder.defineMacro("__linx_m", "2000000");
    Builder.defineMacro("__linx_mul");
    Builder.defineMacro("__linx_div");
    Builder.defineMacro("__linx_muldiv");
  }
}

const Builtin::Info LinxV5TargetInfo::BuiltinInfo[] = {
#define BUILTIN(ID, TYPE, ATTRS)                                               \
  {#ID, TYPE, ATTRS, nullptr, ALL_LANGUAGES, nullptr},
#define TARGET_BUILTIN(ID, TYPE, ATTRS, FEATURE)                               \
  {#ID, TYPE, ATTRS, nullptr, ALL_LANGUAGES, FEATURE},
#include "clang/Basic/BuiltinsLinxV5.def"
};

ArrayRef<Builtin::Info> LinxV5TargetInfo::getTargetBuiltins() const {
  return llvm::makeArrayRef(BuiltinInfo, clang::LinxV5::LastTSBuiltin -
                                             Builtin::FirstTSBuiltin);
}

/// Return true if has this feature, need to sync with handleTargetFeatures.
bool LinxV5TargetInfo::hasFeature(StringRef Feature) const {
  return llvm::StringSwitch<bool>(Feature)
      .Case("linx64v5", true)
      .Case("m", HasM)
      .Default(false);
}

/// Perform initialization based on the user configured set of features.
bool LinxV5TargetInfo::handleTargetFeatures(std::vector<std::string> &Features,
                                            DiagnosticsEngine &Diags) {
  for (const auto &Feature : Features) {
    if (Feature == "+m")
      HasM = true;
  }
  return true;
}
