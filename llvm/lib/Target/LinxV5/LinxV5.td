//===-- LinxV5.td - Describe the LinxV5 Target Machine -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/TableGen/SearchableTable.td"
include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// LinxV5 subtarget features and instruction predicates.
//===----------------------------------------------------------------------===//

// TODO: Delect 64bits extension cause LinxV5 do not need support 32bits.
// Note: Delect this need consider XLEN register define and its ISel code.
def Feature64Bit
    : SubtargetFeature<"64bit", "Has64", "true", "Implements 64Bits">;

def FeatureGeneric
    : SubtargetFeature<"janus", "IsGeneric", "true",
                       "'janus' (v0.5 generic)">;

def FeatureSIMT
    : SubtargetFeature<"simt", "IsSIMT", "true",
                       "'SIMT' (LinxV5 SIMT Extend)">;

// This extension can help LinxV5 control relax
def FeatureRelax
    : SubtargetFeature<"relax", "EnableLinkerRelax", "true",
                       "Enable Linker relaxation.">;

def isScalar : Predicate<"Subtarget->isScalar()">,
                          AssemblerPredicate<(all_of Feature64Bit),
                          "'Scalar' (LinxV5 All Scalar)">;

def isSIMT : Predicate<"Subtarget->isSIMT()">,
                       AssemblerPredicate<(all_of FeatureSIMT),
                       "'SIMT' (LinxV5 SIMT Extend)">;

def hasFloat : Predicate<"Subtarget->hasFloat()">; // TODO: need define assembler predicate
def hasCSel : Predicate<"Subtarget->hasCSel()">;

def useVBXISel : Predicate<"Subtarget->enableLegacyISel()">;
def useDAGISel : Predicate<"!Subtarget->enableLegacyISel()">;

defvar RV32 = DefaultMode;
def RV64           : HwMode<"+64bit">;

//===----------------------------------------------------------------------===//
// Registers, calling conventions, instruction descriptions.
//===----------------------------------------------------------------------===//

include "LinxV5Schedule.td"
include "LinxV5RegisterInfo.td"
include "LinxV5RegisterInfoSIMT.td"
include "LinxV5CallingConv.td"
include "LinxV5VBXInstrInfo.td"
include "LinxV5InstrInfo.td"
include "LinxV5InstrInfoSIMT.td"
include "LinxV5SchedSIMT.td"
include "LinxV5SearchableTables.td"

//===----------------------------------------------------------------------===//
// LinxV5 processors supported.
//===----------------------------------------------------------------------===//

def : ProcessorModel<"janus", DefaultOoOModel, [FeatureGeneric]>;

def : ProcessorModel<"simt", SIMTSchedModel, [FeatureSIMT]>;

//===----------------------------------------------------------------------===//
// Define the LinxV5 target.
//===----------------------------------------------------------------------===//

defvar Linx64 = DefaultMode;

def LinxV5InstrInfo : InstrInfo {
  let guessInstructionProperties = 0;
}

def LinxV5AsmParser : AsmParser {
  let AllowDuplicateRegisterNames = 1;
}

def LinxV5AsmWriter : AsmWriter {
  int PassSubtarget = 1;
}

def LinxV5 : Target {
  let InstructionSet = LinxV5InstrInfo;
  let AssemblyParsers = [LinxV5AsmParser];
  let AssemblyWriters = [LinxV5AsmWriter];
  let AllowRegisterRenaming = 1;
}
