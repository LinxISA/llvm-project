//===--- LinxISA.cpp - Implement LinxISA target feature support -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements LinxISA TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;
using namespace clang::targets;

static constexpr int NumBuiltins =
    clang::LinxISA::LastTSBuiltin - Builtin::FirstTSBuiltin;

#define GET_BUILTIN_STR_TABLE
#include "clang/Basic/BuiltinsLinxISA.inc"
#undef GET_BUILTIN_STR_TABLE

static constexpr Builtin::Info BuiltinInfos[] = {
#define GET_BUILTIN_INFOS
#include "clang/Basic/BuiltinsLinxISA.inc"
#undef GET_BUILTIN_INFOS
};
static_assert(std::size(BuiltinInfos) == NumBuiltins);

llvm::SmallVector<Builtin::InfosShard>
LinxISATargetInfo::getTargetBuiltins() const {
  return {{&BuiltinStrings, BuiltinInfos}};
}

ArrayRef<const char *> LinxISATargetInfo::getGCCRegNames() const {
  static const char *const GCCRegNames[] = {
      "zero", "sp", "a0",  "a1",  "a2",  "a3",  "a4",  "a5",  "a6", "a7", "ra",
      "s0",   "s1", "s2",  "s3",  "s4",  "s5",  "s6",  "s7",  "s8", "x0", "x1",
      "x2",   "x3", "t#1", "t#2", "t#3", "t#4", "u#1", "u#2", "u",  "t",
  };
  return llvm::ArrayRef(GCCRegNames);
}

ArrayRef<TargetInfo::GCCRegAlias> LinxISATargetInfo::getGCCRegAliases() const {
  static const TargetInfo::GCCRegAlias GCCRegAliases[] = {
      {{"r0"}, "zero"}, {{"r1"}, "sp"},  {{"r2"}, "a0"},  {{"r3"}, "a1"},
      {{"r4"}, "a2"},   {{"r5"}, "a3"},  {{"r6"}, "a4"},  {{"r7"}, "a5"},
      {{"r8"}, "a6"},   {{"r9"}, "a7"},  {{"r10"}, "ra"}, {{"r11"}, "s0"},
      {{"r12"}, "s1"},  {{"r13"}, "s2"}, {{"r14"}, "s3"}, {{"r15"}, "s4"},
      {{"r16"}, "s5"},  {{"r17"}, "s6"}, {{"r18"}, "s7"}, {{"r19"}, "s8"},
      {{"r20"}, "x0"},  {{"r21"}, "x1"}, {{"r22"}, "x2"}, {{"r23"}, "x3"},
  };
  return llvm::ArrayRef(GCCRegAliases);
}

bool LinxISATargetInfo::validateAsmConstraint(
    const char *&Name, TargetInfo::ConstraintInfo &Info) const {
  // LinxISA inline assembly constraints
  // r - any general purpose register
  // d - destination register (GPR)
  // s - source register (GPR)
  // I - immediate (12-bit signed)
  // J - immediate (20-bit for LUI)
  // K - immediate (5-bit unsigned)
  // n - immediate (32-bit signed)
  // e - even register (for paired registers)
  // z - zero register (r0)
  // Z - first special register (ra/r10)

  switch (Name[0]) {
  case 'T': {
    if (Name[1] != 'r')
      return false;
    ++Name;
    Info.setAllowsRegister();
    return true;
  }
  case 'v': {
    if (Name[1] != 'r')
      return false;
    ++Name;
    Info.setAllowsRegister();
    return true;
  }
  case 'r': {
    // General purpose register
    Info.setAllowsRegister();
    return true;
  }
  case 'S': {
    // Compiler-allocated core-private Shared tile register S0..S63.
    if (Name[1] == 'r')
      ++Name;
    Info.setAllowsRegister();
    return true;
  }
  case 'd':
  case 's': {
    // Destination/source register
    Info.setAllowsRegister();
    return true;
  }
  case 'I': {
    // 12-bit signed immediate
    Info.setRequiresImmediate();
    return true;
  }
  case 'J': {
    // 20-bit immediate (for LUI)
    Info.setRequiresImmediate();
    return true;
  }
  case 'K': {
    // 5-bit unsigned immediate
    Info.setRequiresImmediate();
    return true;
  }
  case 'n': {
    // 32-bit signed immediate
    Info.setRequiresImmediate();
    return true;
  }
  case 'z': {
    // Zero register constraint
    Info.setAllowsRegister();
    return true;
  }
  case 'e': {
    // Even register (for paired register access)
    Info.setAllowsRegister();
    return true;
  }
  case 'Z': {
    // Special register (ra for return address)
    Info.setAllowsRegister();
    return true;
  }
  case 'i': {
    // Any immediate value
    Info.setRequiresImmediate();
    return true;
  }
  case 'm': {
    // Memory operand
    Info.setAllowsMemory();
    return true;
  }
  case 'p': {
    // Memory operand with base register
    Info.setAllowsMemory();
    return true;
  }
  default:
    return false;
  }
}

std::string
LinxISATargetInfo::convertConstraint(const char *&Constraint) const {
  if ((Constraint[0] == 'T' || Constraint[0] == 'S' || Constraint[0] == 'v') &&
      Constraint[1] == 'r') {
    std::string Result = std::string("^") + std::string(Constraint, 2);
    ++Constraint;
    return Result;
  }
  return TargetInfo::convertConstraint(Constraint);
}

bool LinxISATargetInfo::hasFeature(StringRef Feature) const {
  const bool Is64Bit = getTriple().isArch64Bit();
  return llvm::StringSwitch<bool>(Feature)
      .Case("linx32", !Is64Bit)
      .Case("linx64", Is64Bit)
      .Case("lnx-s32", HasExtS32)
      .Case("lnx-s64", HasExtS64)
      .Case("lnx-c", HasExtC)
      .Case("lnx-f", HasExtF)
      .Case("lnx-a", HasExtA)
      .Case("lnx-sys", HasExtSys)
      .Case("lnx-v", HasExtV)
      .Case("lnx-m", HasExtM)
      .Default(false);
}

bool LinxISATargetInfo::handleTargetFeatures(std::vector<std::string> &Features,
                                             DiagnosticsEngine &Diags) {
  (void)Diags;
  for (const auto &Feature : Features) {
    if (Feature == "+lnx-s32")
      HasExtS32 = true;
    else if (Feature == "-lnx-s32")
      HasExtS32 = false;
    else if (Feature == "+lnx-s64")
      HasExtS64 = true;
    else if (Feature == "-lnx-s64")
      HasExtS64 = false;
    else if (Feature == "+lnx-c")
      HasExtC = true;
    else if (Feature == "-lnx-c")
      HasExtC = false;
    else if (Feature == "+lnx-f")
      HasExtF = true;
    else if (Feature == "-lnx-f")
      HasExtF = false;
    else if (Feature == "+lnx-a")
      HasExtA = true;
    else if (Feature == "-lnx-a")
      HasExtA = false;
    else if (Feature == "+lnx-sys")
      HasExtSys = true;
    else if (Feature == "-lnx-sys")
      HasExtSys = false;
    else if (Feature == "+lnx-v")
      HasExtV = true;
    else if (Feature == "-lnx-v")
      HasExtV = false;
    else if (Feature == "+lnx-m")
      HasExtM = true;
    else if (Feature == "-lnx-m")
      HasExtM = false;
  }

  if (HasExtS64 || HasExtC || HasExtF || HasExtA || HasExtSys || HasExtV ||
      HasExtM)
    HasExtS32 = true;

  return true;
}

void LinxISATargetInfo::getTargetDefines(const LangOptions &Opts,
                                         MacroBuilder &Builder) const {
  (void)Opts;
  Builder.defineMacro("__LINX__");
  Builder.defineMacro("__linx__");
  Builder.defineMacro("__LINXISA__");
  Builder.defineMacro("__linxisa__");
  if (getTriple().isArch64Bit()) {
    Builder.defineMacro("__LINX64__");
    Builder.defineMacro("__linx64__");
  } else {
    Builder.defineMacro("__LINX32__");
    Builder.defineMacro("__linx32__");
  }

  if (HasExtS32)
    Builder.defineMacro("__LINX_EXT_S32__");
  if (HasExtS64)
    Builder.defineMacro("__LINX_EXT_S64__");
  if (HasExtC)
    Builder.defineMacro("__LINX_EXT_C__");
  if (HasExtF)
    Builder.defineMacro("__LINX_EXT_F__");
  if (HasExtA)
    Builder.defineMacro("__LINX_EXT_A__");
  if (HasExtSys)
    Builder.defineMacro("__LINX_EXT_SYS__");
  if (HasExtV)
    Builder.defineMacro("__LINX_EXT_V__");
  if (HasExtM)
    Builder.defineMacro("__LINX_EXT_M__");

  Builder.defineMacro("__ELF__");
}
