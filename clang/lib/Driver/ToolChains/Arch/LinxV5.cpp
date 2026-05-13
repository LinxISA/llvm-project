//===--- LinxV5.cpp - LINXV4 Helpers for Tools -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "../Clang.h"
#include "ToolChains/CommonArgs.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "llvm/ADT/Optional.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/LinxV5ISAInfo.h"
#include "llvm/Support/TargetParser.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

// Returns false if an error is diagnosed.
static bool getArchFeatures(const Driver &D, StringRef Arch,
                            std::vector<StringRef> &Features,
                            const ArgList &Args) {
  bool EnableExperimentalExtensions =
      Args.hasArg(options::OPT_menable_experimental_extensions);
  auto ISAInfo =
      llvm::LinxV5ISAInfo::parseArchString(Arch, EnableExperimentalExtensions);
  if (!ISAInfo) {
    handleAllErrors(ISAInfo.takeError(), [&](llvm::StringError &ErrMsg) {
      D.Diag(diag::err_drv_invalid_linx_arch_name)
          << Arch << ErrMsg.getMessage();
    });

    return false;
  }

  (*ISAInfo)->toFeatures(
      Features, [&Args](const Twine &Str) { return Args.MakeArgString(Str); });
  return true;
}

void linxv5::getLinxV5TargetFeatures(const Driver &D,
                                     const llvm::Triple &Triple,
                                     const ArgList &Args,
                                     std::vector<StringRef> &Features) {
  StringRef MArch = getLinxV5Arch(Args, Triple);

  if (!getArchFeatures(D, MArch, Features, Args))
    return;

  // -mrelax is default, unless -mno-relax is specified.
  if (Args.hasFlag(options::OPT_mrelax, options::OPT_mno_relax, true)) {
    Features.push_back("+relax");
    // -gsplit-dwarf -mrelax requires DW_AT_high_pc/DW_AT_ranges/... indexing
    // into .debug_addr, which is currently not implemented.
    Arg *A;
    if (getDebugFissionKind(D, Args, A) != DwarfFissionKind::None)
      D.Diag(clang::diag::err_drv_riscv_unsupported_with_linker_relaxation)
          << A->getAsString(Args);
  } else {
    Features.push_back("-relax");
  }
}

StringRef linxv5::getLinxV5ABI(const ArgList &Args,
                               const llvm::Triple &Triple) {
  StringRef Arch = getLinxV5Arch(Args, Triple);

  auto ParseResult = llvm::LinxV5ISAInfo::parseArchString(
      Arch, /* EnableExperimentalExtension */ true);
  if (!ParseResult)
    consumeError(ParseResult.takeError());
  else
    return (*ParseResult)->computeDefaultABI();
}

StringRef linxv5::getLinxV5Arch(const llvm::opt::ArgList &Args,
                                const llvm::Triple &Triple) {
  return "linx64v5";
}
