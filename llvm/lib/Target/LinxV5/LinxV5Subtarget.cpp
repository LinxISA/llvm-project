//===-- LinxV5Subtarget.cpp - LinxV5 Subtarget Information ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the LinxV5 specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "LinxV5Subtarget.h"
#include "LinxV5.h"
#include "LinxV5FrameLowering.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "LinxV5GenSubtargetInfo.inc"

void LinxV5Subtarget::anchor() {}

LinxV5Subtarget &LinxV5Subtarget::initializeSubtargetDependencies(
    const Triple &TT, StringRef CPU, StringRef TuneCPU, StringRef FS,
    StringRef ABIName) {
  // Determine default and user-specified characteristics
  bool Is64Bit = TT.isArch64Bit();
  std::string CPUName = std::string(CPU);
  std::string TuneCPUName = std::string(TuneCPU);
  if (CPUName.empty())
    CPUName = "janus";
  if (TuneCPUName.empty())
    TuneCPUName = CPUName;

  if (CPUName == "janus") {
    IsGeneric = true;
    IsSIMT = false;
  } else if (CPUName == "simt") {
    IsGeneric = false;
    IsSIMT = true;
  }

  ParseSubtargetFeatures(CPUName, TuneCPUName, FS);
  if (Is64Bit) { // TODO: Delete all 64bits code.
    XLenVT = MVT::i64;
    XLen = 64;
    Has64 = true;
  }

  TargetABI = LinxV5ABI::computeTargetABI(TT, getFeatureBits(), ABIName);
  LinxV5Features::validate(TT, getFeatureBits());
  return *this;
}

static cl::opt<cl::boolOrDefault>
    EnableCSEL("linxv5-enable-csel", cl::Hidden,
               cl::desc("Enable hardware CSEL Instruction."));

static cl::opt<cl::boolOrDefault> EnableFInstrSelection(
    "linxv5-enable-finstr", cl::Hidden,
    cl::desc("Enable LinxV5 floating-point instructions."));

static cl::opt<cl::boolOrDefault> EnableLegacyISel("linxv5-enable-legacy-isel",
                                                   cl::desc("Enable DAG ISel"),
                                                   cl::Hidden);

static cl::opt<bool> EnableContinuousMemOpt("linxv5-enable-continuous-mem-opt",
                                            cl::init(true), cl::Hidden);

static bool boolVal(cl::boolOrDefault Val) { return Val == cl::BOU_TRUE; }

bool LinxV5Subtarget::enableLegacyISel() const {
  if (EnableLegacyISel == cl::BOU_UNSET)
    return isWireless();
  else
    return boolVal(EnableLegacyISel);
}

bool LinxV5Subtarget::hasFloat() const {
  if (EnableFInstrSelection == cl::BOU_UNSET)
    return !isWireless();
  else
    return boolVal(EnableFInstrSelection);
}

bool LinxV5Subtarget::hasCSel() const {
  if (EnableCSEL == cl::BOU_UNSET)
    return !isWireless();
  else
    return boolVal(EnableCSEL);
}

bool LinxV5Subtarget::enableContinuousMemOpt() const {
  return EnableContinuousMemOpt;
}

bool LinxV5Subtarget::enableFoldCmpArith() const { return !enableLegacyISel(); }

LinxV5Subtarget::LinxV5Subtarget(const Triple &TT, StringRef CPU,
                                 StringRef TuneCPU, StringRef FS,
                                 StringRef ABIName, const TargetMachine &TM)
    : LinxV5GenSubtargetInfo(TT, CPU, TuneCPU, FS),
      UserReservedRegister(LinxV5::NUM_TARGET_REGS),
      FrameLowering(
          initializeSubtargetDependencies(TT, CPU, TuneCPU, FS, ABIName)),
      InstrInfo(*this), RegInfo(getHwMode(), *this), TLInfo(TM, *this) {}
