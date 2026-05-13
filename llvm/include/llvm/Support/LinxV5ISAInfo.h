//===-- LinxV5ISAInfo.h - LinxV5 ISA Information ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_LINXV5ISAINFO_H
#define LLVM_SUPPORT_LINXV5ISAINFO_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"

#include <map>
#include <string>
#include <vector>

namespace llvm {
struct LinxV5ExtensionInfo {
  std::string ExtName;
  unsigned MajorVersion;
  unsigned MinorVersion;
};

class LinxV5ISAInfo {
public:
  LinxV5ISAInfo(const LinxV5ISAInfo &) = delete;
  LinxV5ISAInfo &operator=(const LinxV5ISAInfo &) = delete;
  LinxV5ISAInfo(unsigned XLen) : XLen(XLen) {}

  /// Helper class for OrderedExtensionMap.
  struct ExtensionComparator {
    bool operator()(const std::string &LHS, const std::string &RHS) const {
      return LHS < RHS;
    }
  };

  /// OrderedExtensionMap is std::map, it's specialized to keep entries
  /// in canonical order of extension.
  typedef std::map<std::string, LinxV5ExtensionInfo, ExtensionComparator>
      OrderedExtensionMap;

  /// Parse LinxV5 ISA info from arch string.
  static llvm::Expected<std::unique_ptr<LinxV5ISAInfo>>
  parseArchString(StringRef Arch, bool EnableExperimentalExtension,
                  bool ExperimentalExtensionVersionCheck = true);

  /// Convert LinxV5 ISA info to a feature vector.
  void toFeatures(std::vector<StringRef> &Features,
                  std::function<StringRef(const Twine &)> StrAlloc) const;

  const OrderedExtensionMap &getExtensions() const { return Exts; };

  bool hasExtension(StringRef Ext) const;
  StringRef computeDefaultABI() const;

  static bool isSupportedExtension(StringRef Ext);

private:
  unsigned XLen;

  OrderedExtensionMap Exts;

  void addExtension(StringRef ExtName, unsigned MajorVersion,
                    unsigned MinorVersion);

  static llvm::Expected<std::unique_ptr<LinxV5ISAInfo>>
  postProcessAndChecking(std::unique_ptr<LinxV5ISAInfo> &&ISAInfo);
};

} // namespace llvm

#endif
