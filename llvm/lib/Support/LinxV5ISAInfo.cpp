//===-- LinxV5ISAInfo.cpp - LinxV5 Arch String Parser --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/LinxV5ISAInfo.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include <array>
#include <string>
#include <vector>

using namespace llvm;

namespace {
/// Represents the major and version number components of a LINX extension
struct LinxV5ExtensionVersion {
  unsigned Major;
  unsigned Minor;
};

struct LinxV5SupportedExtension {
  const char *Name;
  /// Supported version.
  LinxV5ExtensionVersion Version;
};

} // end anonymous namespace

static constexpr StringLiteral AllStdExts = "mafdqlcbkjtpvn";

static const LinxV5SupportedExtension SupportedExtensions[] = {
    {"i", LinxV5ExtensionVersion{2, 0}},
    {"m", LinxV5ExtensionVersion{2, 0}},
};

namespace {
struct FindByName {
  explicit FindByName(StringRef Ext) : Ext(Ext){};
  StringRef Ext;
  bool operator()(const LinxV5SupportedExtension &ExtInfo) {
    return ExtInfo.Name == Ext;
  }
};
} // namespace

static Optional<LinxV5ExtensionVersion> findDefaultVersion(StringRef ExtName) {
  // Find default version of an extension.
  // TODO: We might set default version based on profile or ISA spec.
  for (auto &ExtInfo : {makeArrayRef(SupportedExtensions)}) {
    auto ExtensionInfoIterator = llvm::find_if(ExtInfo, FindByName(ExtName));
    if (ExtensionInfoIterator == ExtInfo.end()) {
      continue;
    }
    return ExtensionInfoIterator->Version;
  }
  return None;
}

void LinxV5ISAInfo::addExtension(StringRef ExtName, unsigned MajorVersion,
                                 unsigned MinorVersion) {
  LinxV5ExtensionInfo Ext;
  Ext.ExtName = ExtName.str();
  Ext.MajorVersion = MajorVersion;
  Ext.MinorVersion = MinorVersion;
  Exts[ExtName.str()] = Ext;
}

bool LinxV5ISAInfo::isSupportedExtension(StringRef Ext) {
  return llvm::any_of(SupportedExtensions, FindByName(Ext));
}

bool LinxV5ISAInfo::hasExtension(StringRef Ext) const {
  if (!isSupportedExtension(Ext))
    return false;

  return Exts.count(Ext.str()) != 0;
}

void LinxV5ISAInfo::toFeatures(
    std::vector<StringRef> &Features,
    std::function<StringRef(const Twine &)> StrAlloc) const {
  for (auto const &Ext : Exts) {
    StringRef ExtName = Ext.first;

    if (ExtName == "i")
      continue;

    Features.push_back(StrAlloc("+" + ExtName));
  }
}

llvm::Expected<std::unique_ptr<LinxV5ISAInfo>>
LinxV5ISAInfo::parseArchString(StringRef Arch, bool EnableExperimentalExtension,
                               bool ExperimentalExtensionVersionCheck) {
  // LinxV5 ISA strings must be lowercase.
  if (llvm::any_of(Arch, isupper)) {
    return createStringError(errc::invalid_argument,
                             "string must be lowercase");
  }

  // ISA string must begin with linx64
  // TODO: Add extension here if we need.
  if (!Arch.startswith("linx64v5")) {
    return createStringError(errc::invalid_argument,
                             "string must begin with linx64v5");
  }

  unsigned XLen = 64;
  std::unique_ptr<LinxV5ISAInfo> ISAInfo =
      std::make_unique<LinxV5ISAInfo>(XLen);

  return LinxV5ISAInfo::postProcessAndChecking(std::move(ISAInfo));
}

llvm::Expected<std::unique_ptr<LinxV5ISAInfo>>
LinxV5ISAInfo::postProcessAndChecking(
    std::unique_ptr<LinxV5ISAInfo> &&ISAInfo) {
  return std::move(ISAInfo);
}

StringRef LinxV5ISAInfo::computeDefaultABI() const { return "lp64"; }
