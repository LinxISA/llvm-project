//===--- LinxV5Linux.c - linx-linux-musl ToolChain Implementations -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//

#include "LinxV5Linux.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Options.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang::driver::toolchains;
using namespace clang;
using namespace llvm::opt;

LinxV5Linux::LinxV5Linux(const Driver &D, const llvm::Triple &Triple,
                         const llvm::opt::ArgList &Args)
    : Linux(D, Triple, Args) {

  getProgramPaths().push_back(getDriver().getInstalledDir());
  if (getDriver().getInstalledDir() != getDriver().Dir)
    getProgramPaths().push_back(getDriver().Dir);
}

void LinxV5Linux::AddCXXStdlibLibArgs(const ArgList &Args,
                                      ArgStringList &CmdArgs) const {
  if (!getTriple().isMusl())
    return Linux::AddCXXStdlibLibArgs(Args, CmdArgs);

  assert((GetCXXStdlibType(Args) == ToolChain::CST_Libcxx) &&
         "Only -lc++ (aka libxx) is supported in this toolchain.");

  CmdArgs.push_back("-lc++");
  if (Args.hasArg(options::OPT_fexperimental_library))
    CmdArgs.push_back("-lc++experimental");
  CmdArgs.push_back("-lc++abi");
  CmdArgs.push_back("-lunwind");
}

void LinxV5Linux::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs, llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind DeviceOffloadKind) const {
  Linux::addClangTargetOptions(DriverArgs, CC1Args, DeviceOffloadKind);
  if (DriverArgs.hasArg(options::OPT_mlxbc)) {
    CC1Args.push_back("-mlxbc");
    CC1Args.push_back("-mllvm");
    CC1Args.push_back("-enable-all-vector-as-tilereg");
    CC1Args.push_back("-fenable-matrix");
    CC1Args.push_back("-mllvm");
    CC1Args.push_back("-vectorize-loops=false");
    CC1Args.push_back("-mllvm");
    CC1Args.push_back("-vectorize-slp=false");
  }
  if (!DriverArgs.hasArg(options::OPT_O)) {
    CC1Args.push_back("-O2");
  }
}

void LinxV5Linux::AddTargetPreprocessingOptions(
    const JobAction &JA, const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_mlxbc)) {
    CC1Args.push_back("-include");
    CC1Args.push_back("linx_blkc.h");
    CC1Args.push_back("-I");
    std::string APIPath(getDriver().ResourceDir + "/include/tileop-api");
    CC1Args.push_back(DriverArgs.MakeArgString(APIPath));
  }
}
