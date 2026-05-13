//===- LinxV5BaseInfo.cpp - LinxV5 Base encoding information --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5BaseInfo.h"
#include "LinxV5.h"
#include "llvm/IR/Intrinsics.h"
namespace llvm {
namespace LinxV5 {

namespace {

struct SourceOfDivergence {
  unsigned Intr;
};

const SourceOfDivergence *lookupSourceOfDivergence(unsigned Intr);

#define GET_SourcesOfDivergence_IMPL
#include "LinxV5GenSearchableTables.inc"

} // end anonymous namespace

bool isIntrinsicSourceOfDivergence(unsigned IntrID) {
  return lookupSourceOfDivergence(IntrID);
}

} // end LinxV5 namespace

} // namespace llvm
