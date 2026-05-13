//===-- LinxV5Attributes.h - LinxV5 Attributes -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//
//
// This file contains enumerations for LinxV5 attributes as defined in LinxV5
// ELF psABI specification.
//
// LinxV5 ELF psABI specification
//
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_SUPPORT_LINXV5ATTRIBUTES_H
#define LLVM_SUPPORT_LINXV5ATTRIBUTES_H

#include "llvm/Support/ELFAttributes.h"

namespace llvm {
namespace LinxV5Attrs {
enum AttrType : unsigned {
  // Attribute types in ELF/.linx.attributes.
  STACK_ALIGN = 4,
  ARCH = 5,
  UNALIGNED_ACCESS = 6,
  PRIV_SPEC = 8,
  PRIV_SPEC_MINOR = 10,
  PRIV_SPEC_REVISION = 12,
};

enum StackAlign { ALIGN_4 = 4, ALIGN_16 = 16 };

} // namespace LinxV5Attrs
} // namespace llvm

#endif
