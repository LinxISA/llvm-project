//===-- LinxAttributes.h - Linx Attributes --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//
//
// This file contains enumerations for Linx attributes as defined in Linx
// ELF psABI specification.
//
// Linx ELF psABI specification
//
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_SUPPORT_LINXATTRIBUTES_H
#define LLVM_SUPPORT_LINXATTRIBUTES_H

namespace llvm {
namespace LinxAttrs {
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

} // namespace LinxAttrs
} // namespace llvm

#endif
