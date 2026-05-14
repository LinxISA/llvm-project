//===--- LinxV5Linux.h - linx-linux-musl ToolChain Implementations -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_LINXV5_LINUX_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_LINXV5_LINUX_H

#include "Linux.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {

class LLVM_LIBRARY_VISIBILITY LinxV5Linux : public Linux {
public:
  LinxV5Linux(const Driver &D, const llvm::Triple &Triple,
              const llvm::opt::ArgList &Args);

  RuntimeLibType GetDefaultRuntimeLibType() const override {
    if (!getTriple().isMusl())
      return Linux::GetDefaultRuntimeLibType();

    return ToolChain::RLT_CompilerRT;
  }

  CXXStdlibType GetDefaultCXXStdlibType() const override {
    if (!getTriple().isMusl())
      return Linux::GetDefaultCXXStdlibType();

    return ToolChain::CST_Libcxx;
  }

  void AddCXXStdlibLibArgs(const llvm::opt::ArgList &Args,
                           llvm::opt::ArgStringList &CmdArgs) const override;

  bool IsIntegratedAssemblerDefault() const override {
    if (!getTriple().isMusl())
      return Linux::IsIntegratedAssemblerDefault();

    return true;
  }

  const char *getDefaultLinker() const override {
    if (!getTriple().isMusl())
      return Linux::getDefaultLinker();

    return "ld.lld";
  }

  void addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                             llvm::opt::ArgStringList &CC1Args,
                             Action::OffloadKind) const override;

  void AddTargetPreprocessingOptions(
      const JobAction &JA, const llvm::opt::ArgList &DriverArgs,
      llvm::opt::ArgStringList &CC1Args) const override;
};

} // end namespace toolchains
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_LINXV5_LINUX_H
