//===--- LinxISA.cpp - LinxISA Helpers for Tools ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "clang/Options/Options.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

namespace {

enum class LinxProfile {
  Linx32,
  Linx64,
};

struct ParsedLinxMarch {
  LinxProfile Profile;
  bool HasS32 = true;
  bool HasS64 = false;
  bool HasC = false;
  bool HasF = false;
  bool HasA = false;
  bool HasSys = false;
  bool HasV = false;
  bool HasM = false;
  bool HasPtoV0591Scoped = false;
};

static LinxProfile profileFromTriple(const llvm::Triple &Triple) {
  return Triple.getArch() == llvm::Triple::linx64 ? LinxProfile::Linx64
                                                   : LinxProfile::Linx32;
}

static bool parseProfileToken(StringRef Profile, LinxProfile &OutProfile) {
  if (Profile == "linx32") {
    OutProfile = LinxProfile::Linx32;
    return true;
  }
  if (Profile == "linx64") {
    OutProfile = LinxProfile::Linx64;
    return true;
  }
  return false;
}

static bool enableExtensionToken(StringRef Token, ParsedLinxMarch &Out) {
  if (Token == "lnx-s32") {
    Out.HasS32 = true;
    return true;
  }
  if (Token == "lnx-s64") {
    Out.HasS64 = true;
    return true;
  }
  if (Token == "lnx-c") {
    Out.HasC = true;
    return true;
  }
  if (Token == "lnx-f") {
    Out.HasF = true;
    return true;
  }
  if (Token == "lnx-a") {
    Out.HasA = true;
    return true;
  }
  if (Token == "lnx-sys") {
    Out.HasSys = true;
    return true;
  }
  if (Token == "lnx-v") {
    Out.HasV = true;
    return true;
  }
  if (Token == "lnx-m") {
    Out.HasM = true;
    return true;
  }
  if (Token == "pto-v0591-scoped") {
    Out.HasPtoV0591Scoped = true;
    return true;
  }
  return false;
}

static bool parseLinxMarch(const Driver &D, const llvm::Triple &Triple,
                           const ArgList &Args, const Arg *A,
                           ParsedLinxMarch &Out) {
  StringRef March = A->getValue();
  SmallVector<StringRef, 8> Parts;
  March.split(Parts, '+');
  if (Parts.empty() || Parts[0].empty()) {
    D.Diag(diag::err_drv_invalid_arch_name) << A->getAsString(Args);
    return false;
  }

  LinxProfile ParsedProfile = profileFromTriple(Triple);
  if (!parseProfileToken(Parts.front(), ParsedProfile)) {
    D.Diag(diag::err_drv_invalid_arch_name) << A->getAsString(Args);
    return false;
  }

  const LinxProfile TripleProfile = profileFromTriple(Triple);
  if (ParsedProfile != TripleProfile) {
    D.Diag(diag::err_drv_unsupported_option_argument)
        << A->getSpelling() << March;
    return false;
  }

  Out = ParsedLinxMarch{};
  Out.Profile = ParsedProfile;
  Out.HasS64 = ParsedProfile == LinxProfile::Linx64;

  for (unsigned I = 1; I < Parts.size(); ++I) {
    StringRef Token = Parts[I];
    if (Token.empty()) {
      D.Diag(diag::err_drv_invalid_arch_name) << A->getAsString(Args);
      return false;
    }
    if (!enableExtensionToken(Token, Out)) {
      D.Diag(diag::err_drv_unsupported_option_argument)
          << A->getSpelling() << Token;
      return false;
    }
    if (ParsedProfile == LinxProfile::Linx32 && Token == "lnx-s64") {
      D.Diag(diag::err_drv_unsupported_option_argument)
          << A->getSpelling() << Token;
      return false;
    }
  }

  // Keep extension dependencies explicit in emitted feature sets.
  if (Out.HasS64 || Out.HasC || Out.HasF || Out.HasA || Out.HasSys || Out.HasV ||
      Out.HasM) {
    Out.HasS32 = true;
  }
  return true;
}

} // namespace

std::string linxisa::getLinxISATargetCPU(const Driver &D,
                                         const llvm::Triple &Triple,
                                         const ArgList &Args) {
  (void)D;
  (void)Triple;
  if (const Arg *A = Args.getLastArg(options::OPT_mcpu_EQ)) {
    StringRef CPU = A->getValue();
    if (CPU == "native")
      return "";
    return std::string(CPU);
  }
  return "";
}

void linxisa::getLinxISATargetFeatures(const Driver &D,
                                       const llvm::Triple &Triple,
                                       const ArgList &Args,
                                       std::vector<StringRef> &Features) {
  ParsedLinxMarch MarchConfig;
  MarchConfig.Profile = profileFromTriple(Triple);
  MarchConfig.HasS64 = MarchConfig.Profile == LinxProfile::Linx64;

  if (const Arg *A = Args.getLastArg(options::OPT_march_EQ)) {
    if (!parseLinxMarch(D, Triple, Args, A, MarchConfig))
      return;
  }

  if (MarchConfig.HasS32)
    Features.push_back(Args.MakeArgString("+lnx-s32"));
  if (MarchConfig.HasS64)
    Features.push_back(Args.MakeArgString("+lnx-s64"));
  if (MarchConfig.HasC)
    Features.push_back(Args.MakeArgString("+lnx-c"));
  if (MarchConfig.HasF)
    Features.push_back(Args.MakeArgString("+lnx-f"));
  if (MarchConfig.HasA)
    Features.push_back(Args.MakeArgString("+lnx-a"));
  if (MarchConfig.HasSys)
    Features.push_back(Args.MakeArgString("+lnx-sys"));
  if (MarchConfig.HasV)
    Features.push_back(Args.MakeArgString("+lnx-v"));
  if (MarchConfig.HasM)
    Features.push_back(Args.MakeArgString("+lnx-m"));
  if (MarchConfig.HasPtoV0591Scoped)
    Features.push_back(Args.MakeArgString("+pto-v0591-scoped"));
}
