//===- LinxISATargetTransformInfo.h - LinxISA specific TTI ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file defines a TargetTransformInfoImplBase conforming object specific
/// to the LinxISA target machine.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_LINXISATARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_LINXISA_LINXISATARGETTRANSFORMINFO_H

#include "LinxISASubtarget.h"
#include "LinxISATargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/IR/Function.h"

namespace llvm {

class LinxISATTIImpl final : public BasicTTIImplBase<LinxISATTIImpl> {
  using BaseT = BasicTTIImplBase<LinxISATTIImpl>;
  using TTI = TargetTransformInfo;

  friend BaseT;

  enum LinxRegisterClass { GPRRC, VRRC };

  const LinxISASubtarget *ST;
  const LinxISATargetLowering *TLI;

  const LinxISASubtarget *getST() const { return ST; }
  const LinxISATargetLowering *getTLI() const { return TLI; }

public:
  explicit LinxISATTIImpl(const LinxISATargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  void getUnrollingPreferences(Loop *L, ScalarEvolution &SE,
                               TTI::UnrollingPreferences &UP,
                               OptimizationRemarkEmitter *ORE) const override;

  unsigned getNumberOfRegisters(unsigned ClassID) const override {
    if (ClassID == VRRC)
      return 0;
    return 32;
  }

  unsigned getRegisterClassForType(bool Vector,
                                   Type *Ty = nullptr) const override {
    return Vector ? VRRC : GPRRC;
  }

  const char *getRegisterClassName(unsigned ClassID) const override {
    if (ClassID == VRRC)
      return "LinxVRRC";
    return "LinxGPRRC";
  }

  // Linx bring-up: avoid generic vectorization creating very wide fixed-width
  // vectors that the scalar pipeline does not lower. Tile blocks are expressed
  // via target-extension tile intrinsics and custom lowering.
  TypeSize getRegisterBitWidth(TTI::RegisterKind K) const override {
    switch (K) {
    case TTI::RGK_Scalar:
      return TypeSize::getFixed(64);
    case TTI::RGK_FixedWidthVector:
    case TTI::RGK_ScalableVector:
      return TypeSize::getFixed(0);
    }
    llvm_unreachable("Unknown register kind");
  }

  unsigned getMaximumVF(unsigned ElemWidth, unsigned Opcode) const override {
    (void)ElemWidth;
    (void)Opcode;
    return 1;
  }

  bool supportsScalableVectors() const override { return false; }
  bool enableScalableVectorization() const override { return false; }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LINXISA_LINXISATARGETTRANSFORMINFO_H
