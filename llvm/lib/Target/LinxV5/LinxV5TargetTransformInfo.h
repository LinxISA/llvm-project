//===- LinxV5TargetTransformInfo.h - LinxV5 specific TTI --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This file a TargetTransformInfo::Concept conforming object specific to the
/// LinxV5 target machine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_LINXV5TARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_LINXV5_LINXV5TARGETTRANSFORMINFO_H

#include "LinxV5.h"
#include "LinxV5TargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

namespace llvm {

class LinxV5VecTTIImpl final : public BasicTTIImplBase<LinxV5VecTTIImpl> {
  using BaseT = BasicTTIImplBase<LinxV5VecTTIImpl>;
  using TTI = TargetTransformInfo;

  friend BaseT;

  Triple TargetTriple;

  const TargetSubtargetInfo *ST;
  const TargetLoweringBase *TLI;

  const TargetSubtargetInfo *getST() const { return ST; }
  const TargetLoweringBase *getTLI() const { return TLI; }

public:
  explicit LinxV5VecTTIImpl(const LinxV5TargetMachine *TM, const Function &F)
    : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  bool hasBranchDivergence() { return true; }

  // force using AMDGPU's DA
  bool useGPUDivergenceAnalysis() { return false; }

  /// \returns true if the result of the value could potentially be
  /// different across lanes in a group
  bool isSourceOfDivergence(const Value *V) const;


  bool isAlwaysUniform(const Value *V) const;
};

class LinxV5TTIImpl final : public BasicTTIImplBase<LinxV5TTIImpl> {
  using BaseT = BasicTTIImplBase<LinxV5TTIImpl>;
  using TTI = TargetTransformInfo;

  friend BaseT;

  Triple TargetTriple;

  const TargetSubtargetInfo *ST;
  const TargetLoweringBase *TLI;

  const TargetSubtargetInfo *getST() const { return ST; }
  const TargetLoweringBase *getTLI() const { return TLI; }

public:
  explicit LinxV5TTIImpl(const LinxV5TargetMachine *TM, const Function &F)
    : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}
};

} // end namespace llvm

#endif
