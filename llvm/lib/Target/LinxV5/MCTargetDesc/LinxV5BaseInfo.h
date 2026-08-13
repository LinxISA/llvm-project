//===-- LinxV5BaseInfo.h - Top level definitions for LinxV5 MC -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone enum definitions for the LinxV5 target
// useful for the compiler back-end and the MC libraries.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXBASEINFO_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXBASEINFO_H

#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Support/LinxV5ISAInfo.h"
#include "llvm/Support/MachineValueType.h"

namespace llvm {

// LinxV5II - This namespace holds all of the target specific flags that
// instruction info tracks. All definitions must match LinxV5InstrFormats.td.
namespace LinxV5II {
enum TII {
  // {0~7} For MC Fix-up
  InstFormatDefault = 0,
  InstFormat_ADDI = 1,
  InstFormat_StoreI_UnScaled = 2,
  InstFormat_LoadI_UnScaled = 3,
  InstFormat_SIMT_BRANCH = 4,
  InstFormat_SIMT_JUMP = 5,
  InstFormat_REDUCE = 6,
  InstFormat_Load_Symbol = 7,
  InstFormat_Store_Symbol = 8,
  InstFormat_BRANCH = 9,
  InstFormat_Load_Symbol_TARGET_42 = 10,
  InstFormat_Store_Symbol_TARGET_42 = 11,
  InstFormat_BRANCH_22 = 12,
  InstFormat_Load_Symbol_TARGET_29 = 13,
  InstFormat_Store_Symbol_TARGET_29 = 14,
  InstFormatFP = 15,
  InstFormat_STACK_SIZE = 16,

  InstFormat_BSTART_First = 32,
  InstFormat_BSTART_WITH_TARGET_12 = InstFormat_BSTART_First,
  InstFormat_BSTART_WITHOUT_TARGET_16 = InstFormat_BSTART_First + 1,
  InstFormat_BSTART_WITH_TARGET_17 = InstFormat_BSTART_First + 2,
  InstFormat_BSTART_WITH_TARGET_25 = InstFormat_BSTART_First + 3,
  InstFormat_BSTART_WITH_TARGET_29 = InstFormat_BSTART_First + 4,
  InstFormat_BSTART_WITH_TARGET_42 = InstFormat_BSTART_First + 5,
  InstFormat_BSTART_WITHOUT_TARGET_32 = InstFormat_BSTART_First + 6,
  InstFormat_BSTART_Last = InstFormat_BSTART_First + 32,

  InstFormatBlockModifier = 64,

  InstFormatMask = 255,

  // {8~15} For PE Type
  PETypeShift = 8,
  PET_STD = 0b00000001 << PETypeShift,
  PET_FP = 0b00000010 << PETypeShift,
  PET_SYS = 0b00000100 << PETypeShift,
  PET_FVEC = 0b00001000 << PETypeShift,
  PET_ALL = 255 << PETypeShift,
  PETypeMark = 255 << PETypeShift,

  // {16~19} For TileOp Property
  TOP_Not = 1ULL << 16, // not tile op
  TOP_VEC = 2ULL << 16,
  TOP_MTC = 3ULL << 16,
  TOP_CUBE = 4UL << 16,
  TOP_TEPL = 5UL << 16,
  TileOpPropertyMask = ((1ULL << 4) - 1) << 16,

  // {48~63} for constraint
  ConstraintShift = 47,
  IsHeaderOnlyMask = 1ULL << (ConstraintShift + 1),
  IsDisassembleOnlyMask = 1ULL << (ConstraintShift + 2),
  HasDepOutputMask = 1ULL << (ConstraintShift + 3),
};

// Helper functions to read TSFlags.

static inline bool isTileOpAtVEC(uint64_t TSFlags) {
  return (TSFlags & TileOpPropertyMask) == TOP_VEC;
}

static inline bool isTileOpAtMTC(uint64_t TSFlags) {
  return (TSFlags & TileOpPropertyMask) == TOP_MTC;
}

static inline bool isTileOpAtCUBE(uint64_t TSFlags) {
  return (TSFlags & TileOpPropertyMask) == TOP_CUBE;
}

static inline bool isTileOpAtTEPL(uint64_t TSFlags) {
  return (TSFlags & TileOpPropertyMask) == TOP_TEPL;
}

static inline bool isTileOp(uint64_t TSFlags) {
  return isTileOpAtVEC(TSFlags) || isTileOpAtMTC(TSFlags) ||
         isTileOpAtCUBE(TSFlags) || isTileOpAtTEPL(TSFlags);
}

static inline bool isHeaderOnly(uint64_t TSFlags) {
  return (TSFlags & IsHeaderOnlyMask);
}

/// \returns the format of the instruction.
static inline unsigned getFormat(uint64_t TSFlags) {
  return (TSFlags & InstFormatMask);
}

static inline bool isBSTART(uint64_t TSFlags) {
  unsigned Format = getFormat(TSFlags);
  return Format >= InstFormat_BSTART_First && Format <= InstFormat_BSTART_Last;
}

static inline bool isBlockModifier(uint64_t TSFlags) {
  return getFormat(TSFlags) == InstFormatBlockModifier ||
         getFormat(TSFlags) == InstFormat_STACK_SIZE;
}

static inline bool isMicroInstr(uint64_t TSFlags) {
  return !isHeaderOnly(TSFlags) && getFormat(TSFlags) < InstFormat_BSTART_First;
}

static inline bool isFPInstr(uint64_t TSFlags) {
  return getFormat(TSFlags) == InstFormatFP;
}

// Helper functions to get PE_Type.
static inline unsigned getPEMask(uint64_t TSFlags) {
  unsigned Mask = (TSFlags & PETypeMark);
  if (Mask == 0) {
    // llvm backend instructions
    Mask = PET_ALL;
  }
  return Mask;
}

static inline unsigned evalPETypeFromMask(unsigned Mask) {
  return 1 << countTrailingZeros(Mask);
}

static inline unsigned evalPEType(uint64_t TSFlags) {
  return evalPETypeFromMask(getPEMask(TSFlags));
}

// LinxV5 Specific Machine Operand Flags
enum TOF {
  MO_None = 0,
  MO_TPCREL = 1,
  MO_CALL = 2,
  MO_TPREL_LO = 3,
  MO_TPREL_HI = 4,
  MO_TPCREL_LO = 5,
  MO_TPCREL_HI = 6,
  MO_TPREL = 7,

  // Used to differentiate between target-specific "direct" flags and "bitmask"
  // flags. A machine operand can only have one "direct" flag, but can have
  // multiple "bitmask" flags.
  MO_DIRECT_FLAG_MASK = 31
};
} // namespace LinxV5II

namespace LinxV5TileCall {
struct TileCallEntry {
  int dNum;
  int uNum;
  int RegList;
  int SizeR;
  int MCall;
  int DSrc;
  int DDst;
  int StackIsReg;
  unsigned Insn;
};

#define GET_TileCallTable_DECL
#include "LinxV5GenSearchableTables.inc"
} // namespace LinxV5TileCall

namespace LinxV5Op {

enum SrcRType {
  NONE,
  SW,
  UW,
  NEG,
  NOT,
};

struct NONETraits {
  static constexpr const char *Asm = "";
  static constexpr const SrcRType Type = SrcRType::NONE;
};

struct SWTraits {
  static constexpr const char *Asm = ".sw";
  static constexpr const SrcRType Type = SrcRType::SW;
};

struct UWTraits {
  static constexpr const char *Asm = ".uw";
  static constexpr const SrcRType Type = SrcRType::UW;
};

struct NEGTraits {
  static constexpr const char *Asm = ".neg";
  static constexpr const SrcRType Type = SrcRType::NEG;
};

struct NOTTraits {
  static constexpr const char *Asm = ".not";
  static constexpr const SrcRType Type = SrcRType::NOT;
};

enum SIMTIntSrcRegType {
  SIMT_INT_SRC_REG_TYPE_UD = 0,
  SIMT_INT_SRC_REG_TYPE_UW = 1,
  SIMT_INT_SRC_REG_TYPE_UH = 2,
  SIMT_INT_SRC_REG_TYPE_UB = 3,
  SIMT_INT_SRC_REG_TYPE_SD = 4,
  SIMT_INT_SRC_REG_TYPE_SW = 5,
  SIMT_INT_SRC_REG_TYPE_SH = 6,
  SIMT_INT_SRC_REG_TYPE_SB = 7,
  SIMT_INT_SRC_REG_TYPE_NONE // default
};

enum SIMTFloatSrcRegType {
  SIMT_FLOAT_SRC_REG_TYPE_FD = 0,
  SIMT_FLOAT_SRC_REG_TYPE_FS = 1,
  SIMT_FLOAT_SRC_REG_TYPE_FH = 2,
  SIMT_FLOAT_SRC_REG_TYPE_FB = 3,
  SIMT_FLOAT_SRC_REG_TYPE_BF = 6,
  SIMT_FLOAT_SRC_REG_TYPE_FLB = 7,
  SIMT_FLOAT_SRC_REG_TYPE_NONE // default
};

enum SIMTDstRegType {
  SIMT_INT_DST_REG_TYPE_D = 0,
  SIMT_INT_DST_REG_TYPE_W = 1,
  SIMT_INT_DST_REG_TYPE_H = 2,
  SIMT_INT_DST_REG_TYPE_B = 3,
  SIMT_INT_DST_REG_TYPE_NONE // default
};

enum BranchType {
  EMPTY = 0,
  FALL,
  DIRECT,
  COND,
  CALL,
  IND,
  ICALL,
  RET,
};

enum AttrType {
  NONEATTR = 0,
  RL = 0b1,
  AQ = 0b10,
  AQRL = 0b11,
  ATOMIC = 0b100,
  FAR = 0b1000,
  TRAP = 0b10000
};

enum ArgFormat {
#define TRANS(NAME, CODE) NAME = CODE,
#include "LinxV5TileTrans.def"
#undef TRANS
};

enum Canon { NORMAL_CANON = 0, CANON = 1, EMPTY_Canon };

enum Sat { NOSAT = 0, SAT = 1, EMPTY_Sat };

enum ByteID { BYTE0 = 0, BYTE1 = 1, BYTE2 = 2, BYTE3 = 3, EMPTY_ByteID };

enum RMode {
#define RMODE(NAME, CODE) NAME = CODE,
#include "LinxV5TileRMode.def"
#undef RMODE
};

enum DataType {
  FP64 = 0,
  FP32 = 1,
  TF32 = 2,
  HF32 = 3,
  FP16 = 4,
  BF16 = 5,
  HiF8 = 6,
  e4m3 = 7,
  e5m2 = 8,
  e3m2 = 9,
  e2m3 = 10,
  e2m1x2 = 11,
  e1m2x2 = 12,
  e8m0 = 13,
  HiF4x2 = 14,
  S64 = 16,
  S32 = 17,
  S16 = 18,
  S8 = 19,
  S4x2 = 20,
  U64 = 24,
  U32 = 25,
  U16 = 26,
  U8 = 27,
  U4x2 = 28,
  EMPTY_DataType = 31
};

enum PadValue { Zero = 0, Max = 1, Min = 2, Null = 3, EMPTY_PadValue };

enum TileOPMode {
  VS8 = 0b00,
  VS16 = 0b01,
  VS32 = 0b10,
  VS64 = 0b11,
  EMPTY_TileOPMode
};

enum CmpMode { EQ = 0, NE = 1, LT = 2, GT = 3, LE = 4, GE = 5, EMPTY_CmpMode };
enum TEPLMode { gprs = 0b00, tile = 0b01, EMPTY_TEPLMode };

enum TileOPTMA {
  TLOAD = 0,
  TSTORE = 1,
  TMOV = 2,
  TPREFETCH = 3,
  MGATHER = 4,
  MSCATTER = 5,
  MGATHER_MASK = 6,
  MSCATTER_MASK = 7,
  // PTO v0.58 TLSU Function 8-14.
  MGATHER_CAS = 8,
  TMOV_L2S_INSERT = 9,
  TMOV_L2S_PUBLISH = 10,
  TMOV_S2L_BROADCAST = 11,
  TMOV_S2L_EXTRACT = 12,
  GMOV = 13,
  TSTORE_SPART = 14,
  EMPTY_TileOPTMA
};

enum DREnum { MR = 0, DR = 1, EMPTY_DREnum };

enum TileOPCUBE {
  // DavinciOO v5 active Matrix Functions (per
  // DavinciOO_intrinsic_changes_since_3b4fe5e.md):
  //   TMATMUL 0, TMATMUL.BIAS 1, TMATMUL.ACC 2,
  //   TMATMULMX 4, TMATMULMX.BIAS 5, TMATMULMX.ACC 6,
  //   TGEMV 16, TGEMV.BIAS 17, TGEMV.ACC 18,
  //   TGEMVMX 20, TGEMVMX.BIAS 21, TGEMVMX.ACC 22.
  // Function 3, 7, 8, 9-15, 19 are reserved/illegal. Function 8 was legacy
  // ACCCVT (removed); 9-14 were the deleted TMATMUL*_FIXP profile and must
  // never be remapped to a public operation.
  MAMULB = 0,
  MAMULBAC = 1,
  MAMULB_ACC = 2,
  MAMULBMX = 4,
  MAMULBMXAC = 5,
  MAMULBMX_ACC = 6,
  TGEMV = 16,
  TGEMV_BIAS = 17,
  TGEMV_ACC = 18,
  TGEMVMX = 20,
  TGEMVMX_BIAS = 21,
  TGEMVMX_ACC = 22,
  EMPTY_TileOPCUBE
};

enum TileOPTEPL { ESAVE = 0b1111110, ERCOV = 0b1111111, EMPTY_TileOPTEPL };

enum FenceFlag {
  FF_MEMW = 1 << 0,
  FF_MEMR = 1 << 1,
  FF_DEVO = 1 << 2,
  FF_DEVI = 1 << 3,
  FF_MASK = 15
};

enum WithTargetBranchType {
  DIRECT_TARGET = 1,
  CALL_TARGET = 1,
  COND_TARGET,
};

enum OperandType : unsigned {
  OPERAND_FIRST_LinxV5_IMM = MCOI::OPERAND_FIRST_TARGET,
  OPERAND_UIMM4 = OPERAND_FIRST_LinxV5_IMM,
  OPERAND_UIMM1,
  OPERAND_UIMM2,
  OPERAND_UIMM3,
  OPERAND_UIMM3_64,
  OPERAND_UIMM5,
  OPERAND_UIMM5_32,
  OPERAND_UIMM6,
  OPERAND_UIMM6_64,
  OPERAND_UIMM7,
  OPERAND_UIMM8,
  OPERAND_UIMM12,
  OPERAND_UIMM15,
  OPERAND_UIMM16,
  OPERAND_UIMM17,
  OPERAND_UIMM19,
  OPERAND_UIMM24,
  OPERAND_UIMM32,
  OPERAND_SIMM64,
  OPERAND_SIMM24,
  OPERAND_SIMM12,
  OPERAND_SIMM11,
  OPERAND_SIMM10,
  OPERAND_UIMM10,
  OPERAND_SIMM14,
  OPERAND_UIMM14,
  OPERAND_SIMM9,
  OPERAND_SIMM8,
  OPERAND_SIMM7,
  OPERAND_SIMM6,
  OPERAND_SIMM5,
  OPERAND_SIMM4,
  OPERAND_SIMM3,
  OPERAND_NOT_SIMM12,
  OPERAND_SIMM20,
  OPERAND_SIMM32,
  OPERAND_UIMM20,
  OPERAND_UIMMLOG2XLEN,
  OPERAND_LAST_LinxV5_IMM = OPERAND_UIMMLOG2XLEN
};
} // namespace LinxV5Op

namespace LinxV5ABI {

enum ABI { ABI_LP64, ABI_Unknown };

// Returns the target ABI, or else a StringError if the requested ABIName is
// not supported for the given TT and FeatureBits combination.
ABI computeTargetABI(const Triple &TT, FeatureBitset FeatureBits,
                     StringRef ABIName);

ABI getTargetABI(StringRef ABIName);

// Returns the register used to hold the stack pointer after realignment.
MCRegister getBPReg();

} // namespace LinxV5ABI

namespace LinxV5Features {

// Validates if the given combination of features are valid for the target
// triple. Exits with report_fatal_error if not.
void validate(const Triple &TT, const FeatureBitset &FeatureBits);

} // namespace LinxV5Features

} // namespace llvm

#endif
