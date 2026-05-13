//===----------------------------------------------------------------------===//
//
// Copyright (C) 2023. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_LINXV5TOOLCHAIN_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_LINXV5TOOLCHAIN_H

#include "Gnu.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {

class LLVM_LIBRARY_VISIBILITY LinxV5ToolChain : public Generic_ELF {
public:
  LinxV5ToolChain(const Driver &D, const llvm::Triple &Triple,
                  const llvm::opt::ArgList &Args);

  static bool hasGCCToolchain(const Driver &D, const llvm::opt::ArgList &Args);
  bool IsIntegratedAssemblerDefault() const override { return true; }
  void addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                             llvm::opt::ArgStringList &CC1Args,
                             Action::OffloadKind) const override;
  RuntimeLibType GetDefaultRuntimeLibType() const override;
  UnwindLibType
  GetUnwindLibType(const llvm::opt::ArgList &Args) const override;
  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void
  addLibStdCxxIncludePaths(const llvm::opt::ArgList &DriverArgs,
                           llvm::opt::ArgStringList &CC1Args) const override;
  void AddTargetPreprocessingOptions(
      const JobAction &JA, const llvm::opt::ArgList &DriverArgs,
      llvm::opt::ArgStringList &CC1Args) const override;

protected:
  Tool *buildAssembler() const override;
  Tool *buildLinker() const override;

private:
  std::string computeSysRoot() const override;
};

} // end namespace toolchains

namespace tools {
namespace LinxV5 {
class LLVM_LIBRARY_VISIBILITY Assembler : public Tool {
public:
  explicit Assembler(const ToolChain &TC)
      : Tool("LinxV5::Assembler", "as", TC) {}
  bool hasIntegratedCPP() const override { return false; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};

class LLVM_LIBRARY_VISIBILITY Linker : public Tool {
public:
  explicit Linker(const ToolChain &TC) : Tool("LinxV5::Linker", "ld", TC) {}
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;
};
} // namespace LinxV5
} // end namespace tools

} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_LINXV5TOOLCHAIN_H
