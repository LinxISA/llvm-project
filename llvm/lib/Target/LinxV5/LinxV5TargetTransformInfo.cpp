//===- LinxV5TargetTransformInfo.cpp - LinxV5 specific TTI pass -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// \file
// This file implements a TargetTransformInfo analysis pass specific to the
// LinxV5 target machine. It uses the target's detailed information to provide
// more precise answers to certain TTI queries, while letting the target
// independent and default TTI implementations handle the rest.
//
//===----------------------------------------------------------------------===//

#include "LinxV5TargetTransformInfo.h"
#include "LinxV5BaseInfo.h"
#include "LinxV5TargetMachine.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/CostTable.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/IntrinsicsLinx.h"

using namespace llvm;

/// \returns true if the result of the value could potentially be
/// different across lanes in a group
bool LinxV5VecTTIImpl::isSourceOfDivergence(const Value *V) const {
  if (const Argument *A = dyn_cast<Argument>(V))
    return false;

  if (const Instruction *I = dyn_cast<Instruction>(V)) {
    // For now, vector call can only load data from the local tile register
    // which is definitely divergent
    if (const LoadInst *LI = dyn_cast<LoadInst>(I))
      return true;

    if (const IntrinsicInst *Intrinsic = dyn_cast<IntrinsicInst>(V))
      return LinxV5::isIntrinsicSourceOfDivergence(Intrinsic->getIntrinsicID());
    // TODO: add function call here
  }

  return false;
}

bool LinxV5VecTTIImpl::isAlwaysUniform(const Value *V) const {
  if (const Argument *A = dyn_cast<Argument>(V))
    return true;

  if (const IntrinsicInst *Intrinsic = dyn_cast<IntrinsicInst>(V)) {
    switch (Intrinsic->getIntrinsicID()) {
    default:
      return false;
    case Intrinsic::blkv_get_tile_ptr:
    case Intrinsic::blkv_if:
    case Intrinsic::blkv_flow:
    case Intrinsic::blkv_if_break:
    case Intrinsic::blkv_loop:
    case Intrinsic::blkv_end_cf:
    case Intrinsic::linx_blkv_rdmax:
    case Intrinsic::linx_blkv_rdmin:
    case Intrinsic::linx_blkv_rdadd:
    case Intrinsic::linx_blkv_rdor:
      return true;
    }
  }

  // TODO: add support for inline asm
  //if (CI->isInlineAsm())
  //  return !isInlineAsmSourceOfDivergence(CI, ExtValue->getIndices());

  return false;
}
