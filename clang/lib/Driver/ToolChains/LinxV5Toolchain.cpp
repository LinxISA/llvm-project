//===----------------------------------------------------------------------===//
//
// Copyright (C) 2023. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//

#include "LinxV5Toolchain.h"
#include "Arch/LinxV5.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

static void addMultilibsFilePaths(const Driver &D, const MultilibSet &Multilibs,
                                  const Multilib &Multilib,
                                  StringRef InstallPath,
                                  ToolChain::path_list &Paths) {
  if (const auto &PathsCallback = Multilibs.filePathsCallback())
    for (const auto &Path : PathsCallback(Multilib))
      addPathIfExists(D, InstallPath + Path, Paths);
}

// This function tests whether a gcc installation is present either
// through gcc-toolchain argument or in the same prefix where clang
// is installed. This helps decide whether to instantiate this toolchain
// or Baremetal toolchain.
bool LinxV5ToolChain::hasGCCToolchain(const Driver &D,
                                      const llvm::opt::ArgList &Args) {
  if (Args.getLastArg(options::OPT_gcc_toolchain))
    return true;

  SmallString<128> GCCDir;
  llvm::sys::path::append(GCCDir, D.Dir, "..", D.getTargetTriple(),
                          "lib/crt0.o");
  return llvm::sys::fs::exists(GCCDir);
}

/// LINX Toolchain
LinxV5ToolChain::LinxV5ToolChain(const Driver &D, const llvm::Triple &Triple,
                                 const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  GCCInstallation.init(Triple, Args);
  if (GCCInstallation.isValid()) {
    Multilibs = GCCInstallation.getMultilibs();
    SelectedMultilib = GCCInstallation.getMultilib();
    path_list &Paths = getFilePaths();
    // Add toolchain/multilib specific file paths.
    addMultilibsFilePaths(D, Multilibs, SelectedMultilib,
                          GCCInstallation.getInstallPath(), Paths);
    getFilePaths().push_back(GCCInstallation.getInstallPath().str());
    ToolChain::path_list &PPaths = getProgramPaths();
    // Multilib cross-compiler GCC installations put ld in a triple-prefixed
    // directory off of the parent of the GCC installation.
    PPaths.push_back(Twine(GCCInstallation.getParentLibPath() + "/../" +
                           GCCInstallation.getTriple().str() + "/bin")
                         .str());
    PPaths.push_back((GCCInstallation.getParentLibPath() + "/../bin").str());
  } else {
    getProgramPaths().push_back(D.Dir);
  }
  getFilePaths().push_back(computeSysRoot() + "/lib");
}

Tool *LinxV5ToolChain::buildAssembler() const {
  return new tools::LinxV5::Assembler(*this);
}

Tool *LinxV5ToolChain::buildLinker() const {
  return new tools::LinxV5::Linker(*this);
}

ToolChain::RuntimeLibType LinxV5ToolChain::GetDefaultRuntimeLibType() const {
  return GCCInstallation.isValid() ?
    ToolChain::RLT_Libgcc : ToolChain::RLT_CompilerRT;
}

ToolChain::UnwindLibType
LinxV5ToolChain::GetUnwindLibType(const llvm::opt::ArgList &Args) const {
  return ToolChain::UNW_None;
}

void LinxV5ToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs, llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind) const {
  CC1Args.push_back("-nostdsysteminc");
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
  if (!DriverArgs.hasArg(options::OPT_O_Group)) {
    CC1Args.push_back("-O2");
  }
}

void LinxV5ToolChain::AddTargetPreprocessingOptions(
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

void LinxV5ToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                                ArgStringList &CC1Args) const {
  const Driver &D = getDriver();

  if (DriverArgs.hasArg(options::OPT_nostdinc))
    return;

  if (!DriverArgs.hasArg(options::OPT_nostdlibinc)) {
    std::string HeaderPath = D.InstalledDir + "/../";
    addSystemInclude(DriverArgs, CC1Args,
                     HeaderPath + "linx64-unknown-elf/include");
  }
}

void LinxV5ToolChain::addLibStdCxxIncludePaths(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  const GCCVersion &Version = GCCInstallation.getVersion();
  StringRef TripleStr = GCCInstallation.getTriple().str();
  const Multilib &Multilib = GCCInstallation.getMultilib();
  addLibStdCXXIncludePaths(computeSysRoot() + "/include/c++/" + Version.Text,
      TripleStr, Multilib.includeSuffix(), DriverArgs, CC1Args);
}

std::string LinxV5ToolChain::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;

  SmallString<128> SysRootDir;
  if (GCCInstallation.isValid()) {
    StringRef LibDir = GCCInstallation.getParentLibPath();
    StringRef TripleStr = GCCInstallation.getTriple().str();
    llvm::sys::path::append(SysRootDir, LibDir, "..", TripleStr);
  } else {
    // Use the triple as provided to the driver. Unlike the parsed triple
    // this has not been normalized to always contain every field.
    llvm::sys::path::append(SysRootDir, getDriver().Dir, "..",
                            getDriver().getTargetTriple());
  }

  if (!llvm::sys::fs::exists(SysRootDir))
    return std::string();

  return std::string(SysRootDir.str());
}

void LinxV5::Assembler::ConstructJob(Compilation &C, const JobAction &JA,
                                     const InputInfo &Output,
                                     const InputInfoList &Inputs,
                                     const ArgList &Args,
                                     const char *LinkingOutput) const {}

void LinxV5::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                  const InputInfo &Output,
                                  const InputInfoList &Inputs,
                                  const ArgList &Args,
                                  const char *LinkingOutput) const {
  const ToolChain &ToolChain = getToolChain();
  const Driver &D = ToolChain.getDriver();
  ArgStringList CmdArgs;

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));
  else
    CmdArgs.push_back(
        Args.MakeArgString("--sysroot=" + D.InstalledDir + "/../sysroot"));

  bool IsBigEndian = getToolChain().getArch() == llvm::Triple::linx64v5be;
  if (IsBigEndian) {
    CmdArgs.push_back("-EB");
  }

  CmdArgs.push_back("-m");
  CmdArgs.push_back(IsBigEndian ? "elf64blinx" : "elf64llinx");

  const char *NameStr = Args.MakeArgString("linx64");
  std::string CPUName(NameStr);
  std::string Linker =
      D.InstalledDir + "/ld.lld";

  bool WantCRTs =
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles);

  if (WantCRTs) {
    CmdArgs.push_back(Args.MakeArgString(
        D.InstalledDir + "/../linx64-unknown-elf/lib/crt0.o"));
  }

  CmdArgs.push_back(Args.MakeArgString("-L" + D.InstalledDir +
                                       "/../linx64-unknown-elf/lib"));

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);
  Args.AddAllArgs(CmdArgs,
                  {options::OPT_T_Group, options::OPT_e, options::OPT_s,
                   options::OPT_t, options::OPT_Z_Flag, options::OPT_r});

  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  if (!Args.hasArg(options::OPT_nostdlib) &&
      !Args.hasArg(options::OPT_nodefaultlibs)) {
    if (ToolChain.ShouldLinkCXXStdlib(Args))
      ToolChain.AddCXXStdlibLibArgs(Args, CmdArgs);
    CmdArgs.push_back("--start-group");

    CmdArgs.push_back("-lm");
    CmdArgs.push_back("-lc");

    CmdArgs.push_back("-lgloss");
    CmdArgs.push_back("-lclang_rt.builtins-linx64");

    CmdArgs.push_back("--end-group");
  }

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());
  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(), Args.MakeArgString(Linker),
      CmdArgs, Inputs, Output));
}
// LINXV4 tools end.
