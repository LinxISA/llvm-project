//===----------------------------------------------------------------------===//
//
// Copyright (C) 2023. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_ARCH_LINXV5_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_ARCH_LINXV5_H

#include "clang/Driver/Driver.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/Option.h"
#include <string>
#include <vector>

namespace clang {
namespace driver {
namespace tools {
namespace linxv5 {
void getLinxV5TargetFeatures(const Driver &D, const llvm::Triple &Triple,
                             const llvm::opt::ArgList &Args,
                             std::vector<llvm::StringRef> &Features);
StringRef getLinxV5ABI(const llvm::opt::ArgList &Args,
                       const llvm::Triple &Triple);
StringRef getLinxV5Arch(const llvm::opt::ArgList &Args,
                        const llvm::Triple &Triple);
} // end namespace linxv5
} // namespace tools
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_ARCH_LINXV5_H
