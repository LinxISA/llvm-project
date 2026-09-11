//===-- LinxV5Disassembler.cpp - Disassembler for LinxV5 ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the LinxV5Disassembler class.
//
//===----------------------------------------------------------------------===//
#include "MCTargetDesc/LinxV5InstPrinter.h"

#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "MCTargetDesc/LinxV5TileMacroCatalog.h"
#include "TargetInfo/LinxV5TargetInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {
class LinxV5Disassembler : public MCDisassembler {
  std::unique_ptr<MCInstrInfo const> const MCII;

public:
  LinxV5Disassembler(const MCSubtargetInfo &STI, MCContext &Ctx,
                     MCInstrInfo const *MCII)
      : MCDisassembler(STI, Ctx), MCII(MCII) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};
} // end anonymous namespace

static MCDisassembler *createLinxV5Disassembler(const Target &T,
                                                const MCSubtargetInfo &STI,
                                                MCContext &Ctx) {
  return new LinxV5Disassembler(STI, Ctx, T.createMCInstrInfo());
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLinxV5Disassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheLinx64V5Target(),
                                         createLinxV5Disassembler);
  TargetRegistry::RegisterMCDisassembler(getTheLinx64V5beTarget(),
                                         createLinxV5Disassembler);
}

static DecodeStatus DecodeLoopRegRegisterClass(MCInst &Inst, uint64_t RegNo,
                                               uint64_t Address,
                                               const MCDisassembler *Decoder) {
  uint64_t Enc = RegNo & 0x1f;
  assert(((RegNo >> 5) | 2) == 2 && "invalid LoopReg Ext encoding!");
  MCRegister Reg = MCRegister::NoRegister;
  switch (Enc)
  {
  case 0b00000:
    Reg = LinxV5::SIMT_LC0;
    break;
  case 0b00001:
    Reg = LinxV5::SIMT_LB0;
    break;
  case 0b00100:
    Reg = LinxV5::SIMT_LC1;
    break;
  case 0b00101:
    Reg = LinxV5::SIMT_LB1;
    break;
  case 0b01000:
    Reg = LinxV5::SIMT_LC2;
    break;
  case 0b01001:
    Reg = LinxV5::SIMT_LB2;
    break;
  default: return MCDisassembler::Fail;
  }
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeMCSrcRegisterClass(MCInst &Inst, uint64_t RegNo,
                                             uint64_t Address,
                                             const MCDisassembler *Decoder) {
  if (RegNo >= 32)
    return MCDisassembler::Fail;

  MCRegister Reg;
  if (RegNo <= 23)
    Reg = LinxV5::R0 + RegNo;
  else if (RegNo <= 27)
    Reg = LinxV5::TOS1 + RegNo - 24;
  else
    Reg = LinxV5::UOS1 + RegNo - 28;

  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static MCRegister DecodeMCDst(uint64_t RegNo) {
  if (RegNo >= 32)
    return MCRegister::NoRegister;

  MCRegister Reg;
  if (RegNo <= 23)
    Reg = LinxV5::R0 + RegNo;
  else if (RegNo == 24)
    Reg = LinxV5::TX2;
  else if (RegNo == 25)
    Reg = LinxV5::UX2;
  else if (RegNo == 26)
    Reg = LinxV5::TX4;
  else if (RegNo == 27)
    Reg = LinxV5::UX4;
  else if (RegNo == 30)
    Reg = LinxV5::U;
  else if (RegNo == 31)
    Reg = LinxV5::T;
  else
    return MCRegister::NoRegister;
  return Reg;
}

static DecodeStatus DecodeMCDstRegisterClass(MCInst &Inst, uint64_t RegNo,
                                             uint64_t Address,
                                             const MCDisassembler *Decoder) {
  MCRegister Reg = DecodeMCDst(RegNo);
  if (Reg == MCRegister::NoRegister)
    return MCDisassembler::Fail;

  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus
DecodeMCDstNoRARegisterClass(MCInst &Inst, uint64_t RegNo, uint64_t Address,
                             const MCDisassembler *Decoder) {
  MCRegister Reg = DecodeMCDst(RegNo);
  if (Reg == MCRegister::NoRegister || Reg == LinxV5::R10)
    return MCDisassembler::Fail;

  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeSIMT_SRCRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  MCRegister Reg;
  if (RegNo >= 0 && RegNo <= 3)
    Reg = LinxV5::SIMT_OSVT1 + RegNo;
  else if (RegNo <= 15)
    Reg = LinxV5::SIMT_OSVU1 + RegNo - 8;
  else if (RegNo <= 23)
    Reg = LinxV5::SIMT_OSVM1 + RegNo - 16;
  else if (RegNo <= 31)
    Reg = LinxV5::SIMT_OSVN1 + RegNo - 24;
  else if (RegNo >= 32 && RegNo <= 43)
    Reg = LinxV5::SIMT_RI0 + RegNo - 32;
  else if (RegNo >= 56 && RegNo <= 59)
    Reg = LinxV5::SIMT_OST1 + RegNo - 56;
  else if (RegNo >= 60 && RegNo <= 63)
    Reg = LinxV5::SIMT_OSU1 + RegNo - 60;
  else if (RegNo == 64)
    Reg = LinxV5::SIMT_LC0;
  else if (RegNo == 65)
    Reg = LinxV5::SIMT_LB0;
  else if (RegNo == 68)
    Reg = LinxV5::SIMT_LC1;
  else if (RegNo == 69)
    Reg = LinxV5::SIMT_LB1;
  else if (RegNo == 72)
    Reg = LinxV5::SIMT_LC2;
  else if (RegNo == 73)
    Reg = LinxV5::SIMT_LB2;
  else if (RegNo >= 80 && RegNo <= 87)
    Reg = LinxV5::SIMT_TA + RegNo - 80;
  else if (RegNo == 88)
    Reg = LinxV5::SIMT_TO;
  else if (RegNo >= 89 && RegNo <= 91)
    Reg = LinxV5::SIMT_TO1 + RegNo - 89;
  else if (RegNo == 92)
    Reg = LinxV5::SIMT_P;
  else if (RegNo == 95)
    Reg = LinxV5::R0;
  // v5: SIMT source reuse registers (extenc=11, RegNo 96-123) removed.
  else
    return MCDisassembler::Fail;

  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeSIMT_DSTRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  MCRegister Reg;
  if (RegNo == 0)
    Reg = LinxV5::SIMT_VT;
  else if (RegNo == 1)
    Reg = LinxV5::SIMT_VU;
  else if (RegNo == 2)
    Reg = LinxV5::SIMT_VM;
  else if (RegNo == 3)
    Reg = LinxV5::SIMT_VN;
  else if (RegNo >= 32 && RegNo <= 43)
    Reg = LinxV5::SIMT_RO0 + RegNo - 32;
  else if (RegNo == 62)
    Reg = LinxV5::SIMT_U;
  else if (RegNo == 63)
    Reg = LinxV5::SIMT_T;
  else if (RegNo == 92)
    Reg = LinxV5::SIMT_P;
  else
    return MCDisassembler::Fail;

  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeSIMT_SRC_VecRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  return DecodeSIMT_SRCRegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus DecodeSIMT_SRC_ScalarRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  return DecodeSIMT_SRCRegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus DecodeSIMT_DST_ScalarRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  return DecodeSIMT_DSTRegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus DecodeSIMT_DST_VecRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  return DecodeSIMT_DSTRegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus
DecodeSIMT_DST_CMP_ScalarRegisterClass(MCInst &Inst, uint64_t RegNo,
                                       uint64_t Address,
                                       const MCDisassembler *Decoder) {
  return DecodeSIMT_DSTRegisterClass(Inst, RegNo, Address, Decoder);
}

static DecodeStatus DecodeTILE_SRCRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  if (RegNo >= 128)
    return MCDisassembler::Fail;
  // v5: source reuse bit{6} removed. Ignore the former reuse bit.
  bool isKill = RegNo >= 0b1000000;
  if (isKill) {
    RegNo -= 0b1000000;
  }
  MCRegister Reg;
  if (RegNo <= 15) {
    // t#{1-16} start from 0
    Reg = RegNo + LinxV5::Tile_TOS1;
  } else if (RegNo <= 31) {
    // u#{1-16} start from 0b010000
    Reg = RegNo - 16 + LinxV5::Tile_UOS1;
  } else if (RegNo <= 47) {
    // m#{1-16} start from 0b100000
    Reg = RegNo - 32 + LinxV5::Tile_MOS1;
  } else {
    // n#{1-16} start from 0b110000
    Reg = RegNo - 48 + LinxV5::Tile_NOS1;
  }
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeTILE_DSTRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  MCRegister Reg;
  if (RegNo == 0)
    Reg = LinxV5::Tile_T;
  else if (RegNo == 1)
    Reg = LinxV5::Tile_U;
  else if (RegNo == 2)
    Reg = LinxV5::Tile_M;
  else if (RegNo == 3)
    Reg = LinxV5::Tile_N;
  else if (RegNo == 4)
    Reg = LinxV5::Tile_ACC;
  else if (RegNo == 5)
    Reg = LinxV5::Tile_S;
  else
    return MCDisassembler::Fail;

  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeDep_SRCRegisterClass(MCInst &Inst, uint64_t RegNo,
                                                uint64_t Address,
                                                const MCDisassembler *Decoder) {
  MCRegister Reg;
  if (RegNo >= 9)
    return MCDisassembler::Fail;
  Reg = LinxV5::Dep_DOS0 + RegNo;
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeSImmOperandAndLsl1(MCInst &Inst, uint64_t Imm,
                                             int64_t Address,
                                             const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  // Sign-extend the number in the bottom N bits of Imm after accounting for
  // the fact that the N bit immediate is stored in N-1 bits (the LSB is
  // always zero)
  Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm << 1)));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeSImmOperandAndLsl3(MCInst &Inst, uint64_t Imm,
                                             int64_t Address,
                                             const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  // Sign-extend the number in the bottom N bits of Imm after accounting for
  // the fact that the N bit immediate is stored in N-1 bits (the LSB is
  // always zero)
  Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm << 3)));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeUImmOperandAndLsl1(MCInst &Inst, uint64_t Imm,
                                             int64_t Address,
                                             const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  // Sign-extend the number in the bottom N bits of Imm after accounting for
  // the fact that the N bit immediate is stored in N-1 bits (the LSB is
  // always zero)
  Inst.addOperand(MCOperand::createImm(Imm << 1));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeUImmOperandAndPlus1(MCInst &Inst, uint64_t Imm,
                                             int64_t Address,
                                             const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  // Plus 1 to the number
  Inst.addOperand(MCOperand::createImm(Imm + 1));
  return MCDisassembler::Success;
}

// Extract bits v[begin:end], where range is inclusive, and begin must be < 63.
static uint32_t extractBits(uint64_t v, uint32_t begin, uint32_t end) {
  return (v & ((1ULL << (begin + 1)) - 1)) >> end;
}

template <unsigned N>
static DecodeStatus decodeUImmOperand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address,
                                      const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeSImmOperand(MCInst &Inst, uint64_t Imm,
                                      int64_t Address,
                                      const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  // Sign-extend the number in the bottom N bits of Imm
  Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm)));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeSImmShiftN(MCInst &Inst, uint64_t Imm,
                                     int64_t Address,
                                     const MCDisassembler *Decoder) {
  assert(isUInt<N + 5>(Imm) && "Invalid immediate");
  uint64_t shift = Imm & 0x1f;
  uint64_t base = Imm >> 5;
  Inst.addOperand(MCOperand::createImm(SignExtend64<N>(base) << shift));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeUImmShiftN(MCInst &Inst, uint64_t Imm,
                                     int64_t Address,
                                     const MCDisassembler *Decoder) {
  assert(isUInt<N + 5>(Imm) && "Invalid immediate");
  uint64_t shift = Imm & 0x1f;
  uint64_t base = Imm >> 5;
  Inst.addOperand(MCOperand::createImm((base) << shift));
  return MCDisassembler::Success;
}

template <class InsnType>
static DecodeStatus decodeFail(MCInst &Inst, const InsnType &insn,
                               int64_t Address, const MCDisassembler *Decoder) {
  return MCDisassembler::Fail;
}

// B.DATR/tile layout transport operand decoder. Receives the already
// extracted 5-bit layout code; only the active ISA assigned set decodes,
// and reserved codes (2,5,7,12..16,19) fail closed so a raw word with
// a reserved layout prints <unknown>.
template <typename InsnType>
static DecodeStatus decodeBArgFormat(MCInst &Inst, const InsnType &insn,
                                     int64_t Address,
                                     const MCDisassembler *Decoder) {
  uint64_t Code = static_cast<uint64_t>(insn) & 0x1f;
  switch (Code) {
  case LinxV5Op::ArgFormat::OHWI2NK:
  case LinxV5Op::ArgFormat::OIHW2NK:
  case LinxV5Op::ArgFormat::NORM:
  case LinxV5Op::ArgFormat::ND2DN:
  case LinxV5Op::ArgFormat::ND2ZN:
  case LinxV5Op::ArgFormat::ND2NZ:
  case LinxV5Op::ArgFormat::DN2ND:
  case LinxV5Op::ArgFormat::DN2ZN:
  case LinxV5Op::ArgFormat::DN2NZ:
  case LinxV5Op::ArgFormat::ZN2ND:
  case LinxV5Op::ArgFormat::ZN2DN:
  case LinxV5Op::ArgFormat::ZN2NZ:
  case LinxV5Op::ArgFormat::ND2M32:
  case LinxV5Op::ArgFormat::ND2M16:
  case LinxV5Op::ArgFormat::ND2N8:
  case LinxV5Op::ArgFormat::M322ND:
  case LinxV5Op::ArgFormat::M162ND:
  case LinxV5Op::ArgFormat::N82ND:
  case LinxV5Op::ArgFormat::NZ2ND:
  case LinxV5Op::ArgFormat::NZ2DN:
  case LinxV5Op::ArgFormat::CUBE_M32:
  case LinxV5Op::ArgFormat::NZ2ZN:
  case LinxV5Op::ArgFormat::CUBE_M16:
    Inst.addOperand(MCOperand::createImm(Code));
    return MCDisassembler::Success;
  default:
    return MCDisassembler::Fail; // reserved layout code
  }
}

// B.IOS SizeCode operand decoder. The operand decoder receives the
// already-extracted 4-bit field value (fieldFromInstruction(insn, 15, 4) in
// the generated table), not the full instruction word. SizeCode=0 is the
// source form; 1..12 are destination capacities; 13..15 are reserved and
// rejected.
template <typename InsnType>
static DecodeStatus decodeBIOSDstTSize(MCInst &Inst, const InsnType &insn,
                                       int64_t Address,
                                       const MCDisassembler *Decoder) {
  uint64_t SizeCode = static_cast<uint64_t>(insn) & 0xf;
  if (SizeCode > 12)
    return MCDisassembler::Fail; // reserved 13..15
  Inst.addOperand(MCOperand::createImm(SizeCode));
  return MCDisassembler::Success;
}

// B.IOT destination SizeCode operand decoder (Local SizeCode contract
// 1..12). A size code of 0 encodes the source-only form, and 13..15 are
// reserved, so an instruction word that lands on a B_IOT_*_Dst form with
// any of these must fail to decode as a destination (the decoder then falls
// through to the source-only form for SizeCode=0).
template <typename InsnType>
static DecodeStatus decodeBIOTDstSizeCode(MCInst &Inst, const InsnType &insn,
                                          int64_t Address,
                                          const MCDisassembler *Decoder) {
  uint64_t SizeCode = static_cast<uint64_t>(insn) & 0xf;
  if (SizeCode < 1 || SizeCode > 12)
    return MCDisassembler::Fail; // source form (0) or reserved (13..15)
  Inst.addOperand(MCOperand::createImm(SizeCode));
  return MCDisassembler::Success;
}

// B.SUBVIEW SubviewSizeCode operand decoder (ADR-0098): receives the
// already-extracted 4-bit field value; only 1..12 decode, 0/13..15 fail.
template <typename InsnType>
static DecodeStatus decodeSUBVIEWSizeCode(MCInst &Inst, const InsnType &insn,
                                          int64_t Address,
                                          const MCDisassembler *Decoder) {
  uint64_t SizeCode = static_cast<uint64_t>(insn) & 0xf;
  if (SizeCode < 1 || SizeCode > 12)
    return MCDisassembler::Fail; // reserved 0, 13..15
  Inst.addOperand(MCOperand::createImm(SizeCode));
  return MCDisassembler::Success;
}

// B.ASSEMBLE ParentSizeCode operand decoder (ADR-0098): 0..12 decode;
// 13..15 reserved. (The INIT<->non-INIT size-0 association is checked by
// the assembler predicate; the decoder only enforces the raw range.)
template <typename InsnType>
static DecodeStatus decodeParentSizeCode(MCInst &Inst, const InsnType &insn,
                                         int64_t Address,
                                         const MCDisassembler *Decoder) {
  uint64_t SizeCode = static_cast<uint64_t>(insn) & 0xf;
  if (SizeCode > 12)
    return MCDisassembler::Fail; // reserved 13..15
  Inst.addOperand(MCOperand::createImm(SizeCode));
  return MCDisassembler::Success;
}

// PEMode operand decoder: receives the already-extracted 3-bit mode value
// (fieldFromInstruction(insn, 9, 3) in the generated table) and expands it
// back to the 4-bit semantic mask that the printer renders as mask=NNNN.
template <typename InsnType>
static DecodeStatus decodePEMode(MCInst &Inst, const InsnType &insn,
                                 int64_t Address,
                                 const MCDisassembler *Decoder) {
  uint64_t Mode = static_cast<uint64_t>(insn) & 0x7;
  Inst.addOperand(MCOperand::createImm(LinxV5PEMode::maskForMode(Mode)));
  return MCDisassembler::Success;
}

#include "LinxV5GenDisassemblerTables.inc"

enum DecoderNS {
  STD = 0b00,
  SYS = 0b01,
  FP = 0b10,
  Last = FP,
};

static bool isShareSpace(ArrayRef<uint8_t> Bytes) {
  if ((Bytes[0] & 0x7) == 0x3 || (Bytes[0] & 0x7) == 0x5)
    return true;
  return false;
}

// PTO-ISA 0.58.4 ADR-0098 range modifiers: fail-closed validation of a
// decoded B.SUBVIEW/B.ASSEMBLE word. The field-level operand decoders
// enforce raw ranges, but the following cross-field contracts can only be
// checked against the full word:
//   B.SUBVIEW: SubviewSizeCode must be 1..12; RegSrc 0..23
//   B.ASSEMBLE: INIT=1 requires ParentSizeCode 1..12; INIT=0 requires 0;
//               RegSrc 0..23
// Any violation makes the word fail to decode (<unknown>) instead of
// printing a semantically-illegal instruction.
static bool subviewWordIllegal(uint64_t Insn) {
  uint64_t SubviewSizeCode = (Insn >> 7) & 0xf;
  if (SubviewSizeCode < 1 || SubviewSizeCode > 12)
    return true;
  uint64_t RegSrc = (Insn >> 15) & 0x1f;
  if (RegSrc > 23)
    return true;
  return false;
}

static bool assembleWordIllegal(uint64_t Insn) {
  uint64_t Init = (Insn >> 31) & 0x1;
  uint64_t ParentSizeCode = (Insn >> 7) & 0xf;
  if (ParentSizeCode > 12)
    return true; // reserved 13..15
  if (Init == 1 && ParentSizeCode == 0)
    return true; // INIT requires ParentSizeCode 1..12
  if (Init == 0 && ParentSizeCode != 0)
    return true; // non-INIT requires ParentSizeCode 0
  uint64_t RegSrc = (Insn >> 15) & 0x1f;
  if (RegSrc > 23)
    return true;
  return false;
}

static DecodeStatus decodeOneLinxV5Instruction(MCInst &MI, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               const MCDisassembler *Decoder,
                                               const MCSubtargetInfo &STI) {
  uint64_t Insn;
  DecodeStatus Result;

  if (Bytes.size() > 4 && (Bytes[0] & 0xf) == 0xf) {
    if (Bytes.size() < 8) {
      Size = 0;
      return MCDisassembler::Fail;
    }
    Size = 8;
    Insn = support::endian::read64le(Bytes.data());
    Result = decodeInstruction(DecoderTable64, MI, Insn, Address, Decoder, STI);
  } else {
    if (Bytes.size() > 4 && (Bytes[0] & 0xf) == 0xe) { // HL.BSTART.STD/SYS/FP
      if (Bytes.size() < 6) {
        Size = 0;
        return MCDisassembler::Fail;
      }
      Size = 6;
      Insn = support::endian::read64le(Bytes.data()) & 0xffffffffffffULL;
      Result =
          decodeInstruction(DecoderTable48, MI, Insn, Address, Decoder, STI);
    } else if ((Bytes[0] & 0x1) == 0x1 && (Bytes[0] & 0xf) != 0xf) {
      if (Bytes.size() < 4) {
        Size = 0;
        return MCDisassembler::Fail;
      }
      Size = 4;
      Insn = support::endian::read32le(Bytes.data());
      // PTO-ISA 0.58.4 ADR-0098 range modifiers: reject words that hit the
      // B.SUBVIEW/B.ASSEMBLE match but carry an illegal cross-field
      // combination, before the table decoder can print them. Size stays 4
      // so the disassembler skips the whole word and re-aligns (<unknown>
      // is printed for the illegal word, then decoding continues).
      if (((Insn & 0x787f) == 0x0053 && subviewWordIllegal(Insn)) ||
          ((Insn & 0x707f) == 0x1053 && assembleWordIllegal(Insn))) {
        return MCDisassembler::Fail;
      }
      Result =
          decodeInstruction(DecoderTable32, MI, Insn, Address, Decoder, STI);
    } else {
      if (Bytes.size() < 2) {
        Size = 0;
        return MCDisassembler::Fail;
      }
      Size = 2;
      Insn = support::endian::read16le(Bytes.data());
      Result =
          decodeInstruction(DecoderTable16, MI, Insn, Address, Decoder, STI);
    }
  }

  return Result;
}

static bool tryDecodeTileMacro(MCInst &MI, uint64_t &Size,
                               ArrayRef<uint8_t> Bytes, uint64_t Address,
                               const MCDisassembler *Decoder,
                               const MCSubtargetInfo &STI,
                               const MCInstrInfo &MII) {
  MCInst Header;
  uint64_t HeaderSize = 0;
  if (decodeOneLinxV5Instruction(Header, HeaderSize, Bytes, Address, Decoder,
                                 STI) != MCDisassembler::Success)
    return false;

  TileMacroHeaderKind HeaderKind;
  unsigned Selector = 0;
  unsigned DataType = 0;
  if (Header.getOpcode() == LinxV5::BSTART_TEPL_NoMode &&
      Header.getNumOperands() == 2) {
    HeaderKind = TileMacroHeaderKind::TEPL;
    DataType = Header.getOperand(0).getImm();
    Selector = Header.getOperand(1).getImm();
  } else if (Header.getOpcode() == LinxV5::BSTART_TMA &&
             Header.getNumOperands() == 2) {
    HeaderKind = TileMacroHeaderKind::TLSU;
    DataType = Header.getOperand(0).getImm();
    Selector = Header.getOperand(1).getImm();
  } else if (Header.getOpcode() == LinxV5::BSTART_CUBE &&
             Header.getNumOperands() == 2) {
    HeaderKind = TileMacroHeaderKind::CUBE;
    DataType = Header.getOperand(0).getImm();
    Selector = Header.getOperand(1).getImm();
  } else if (Header.getOpcode() == LinxV5::BSTART_GMOV &&
             Header.getNumOperands() == 1) {
    HeaderKind = TileMacroHeaderKind::GMOV;
    DataType = Header.getOperand(0).getImm();
    Selector = 13;
  } else {
    return false;
  }

  const TileMacroOperationDesc *Operation = nullptr;
  for (unsigned I = 0; I != TileMacroOperationCount; ++I) {
    if (TileMacroOperations[I].HeaderKind == HeaderKind &&
        TileMacroOperations[I].Selector == Selector) {
      Operation = &TileMacroOperations[I];
      break;
    }
  }
  if (!Operation)
    return false;

  uint64_t Offset = HeaderSize;
  SmallVector<MCInst, 16> Commands;
  enum class TileMacroBoundary : unsigned {
    ExplicitBSTOP,
    NextBSTART,
    EndOfSection,
    NextNonModifier,
    DecodeFailure,
    Limit,
  };
  TileMacroBoundary Boundary = TileMacroBoundary::EndOfSection;
  while (true) {
    if (Offset >= Bytes.size()) {
      Boundary = TileMacroBoundary::EndOfSection;
      break;
    }
    MCInst Command;
    uint64_t CommandSize = 0;
    if (decodeOneLinxV5Instruction(Command, CommandSize, Bytes.slice(Offset),
                                   Address + Offset, Decoder,
                                   STI) != MCDisassembler::Success) {
      Boundary = TileMacroBoundary::DecodeFailure;
      break;
    }
    uint64_t TSFlags = MII.get(Command.getOpcode()).TSFlags;
    if (Command.getOpcode() == LinxV5::BSTOP ||
        Command.getOpcode() == LinxV5::BSTOP_C) {
      Commands.push_back(Command);
      Offset += CommandSize;
      Boundary = TileMacroBoundary::ExplicitBSTOP;
      break;
    }
    if (LinxV5II::isBSTART(TSFlags) && !LinxV5II::isBlockModifier(TSFlags)) {
      Boundary = TileMacroBoundary::NextBSTART;
      break;
    }
    if (!LinxV5II::isBlockModifier(TSFlags)) {
      Boundary = TileMacroBoundary::NextNonModifier;
      break;
    }
    Commands.push_back(Command);
    Offset += CommandSize;
    if (Commands.size() == 32) {
      Boundary = TileMacroBoundary::Limit;
      break;
    }
  }
  if (Commands.empty())
    return false;

  ArrayRef<MCInst> Body = Commands;
  if (!Body.empty() && (Body.back().getOpcode() == LinxV5::BSTOP ||
                        Body.back().getOpcode() == LinxV5::BSTOP_C))
    Body = Body.drop_back();
  bool NonCanonicalBoundary = Boundary == TileMacroBoundary::DecodeFailure ||
                              Boundary == TileMacroBoundary::Limit;

  struct IOTShape {
    unsigned Sources = 0;
    bool Destination = false;
    unsigned Mask = 0;
    bool Last = false;
  };
  auto decodeIOTShape = [&](const MCInst &Inst, IOTShape &Shape) {
    switch (Inst.getOpcode()) {
    case LinxV5::B_IOT_NoSrc_Dst:
      Shape.Destination = true;
      Shape.Mask = Inst.getOperand(1).getImm();
      Shape.Last = Inst.getOperand(3).getImm();
      return true;
    case LinxV5::B_IOT_OneSrc_Dst:
      Shape.Sources = 1;
      Shape.Destination = true;
      Shape.Mask = Inst.getOperand(1).getImm();
      Shape.Last = Inst.getOperand(3).getImm();
      return true;
    case LinxV5::B_IOT_TwoSrc_Dst:
      Shape.Sources = 2;
      Shape.Destination = true;
      Shape.Mask = Inst.getOperand(1).getImm();
      Shape.Last = Inst.getOperand(3).getImm();
      return true;
    case LinxV5::B_IOT_OneSrc_NoDst:
      Shape.Sources = 1;
      Shape.Mask = Inst.getOperand(0).getImm();
      Shape.Last = Inst.getOperand(1).getImm();
      return true;
    case LinxV5::B_IOT_TwoSrc_NoDst:
      Shape.Sources = 2;
      Shape.Mask = Inst.getOperand(0).getImm();
      Shape.Last = Inst.getOperand(1).getImm();
      return true;
    default:
      return false;
    }
  };
  auto bindingForMember =
      [&](const TileMacroFormDesc &Form,
          unsigned MemberIndex) -> const TileMacroBindingDesc * {
    for (unsigned I = 0; I != Form.NumBindings; ++I) {
      const auto &Binding = TileMacroBindings[Form.FirstBinding + I];
      if (MemberIndex >= Binding.FirstMember &&
          MemberIndex < Binding.FirstMember + Binding.NumMembers)
        return &Binding;
    }
    return nullptr;
  };
  auto isLocalDestination = [&](TileMacroBindingKind Kind) {
    return Kind == TileMacroBindingKind::LocalDestination ||
           Kind == TileMacroBindingKind::PredicateTileDestination ||
           Kind == TileMacroBindingKind::PredicateCellDestination;
  };
  auto hasTarget = [&](const TileMacroFormDesc &Form, TileMacroTargetKind Kind,
                       TileMacroTargetSlot Slot, int Group) {
    for (unsigned I = 0; I != Form.NumConfigs; ++I) {
      const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
      for (unsigned T = 0; T != Config.NumTargets; ++T) {
        const auto &Target = TileMacroConfigTargets[Config.FirstTarget + T];
        if (Target.Kind == Kind && Target.Slot == Slot &&
            (Group < 0 || Target.Group == Group))
          return true;
      }
    }
    return false;
  };
  auto targetConfig =
      [&](const TileMacroFormDesc &Form, TileMacroTargetKind Kind,
          TileMacroTargetSlot Slot) -> const TileMacroConfigDesc * {
    for (unsigned I = 0; I != Form.NumConfigs; ++I) {
      const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
      for (unsigned T = 0; T != Config.NumTargets; ++T) {
        const auto &Target = TileMacroConfigTargets[Config.FirstTarget + T];
        if (Target.Kind == Kind && Target.Slot == Slot)
          return &Config;
      }
    }
    return nullptr;
  };
  auto conditionEnabled = [&](StringRef Condition, const MCInst *FPATR) {
    if (Condition.empty())
      return true;
    if (Condition == "DataType=U8")
      return DataType == LinxV5Op::DataType::U8;
    if (Condition == "AType requires MX scale")
      return LinxV5Op::matrixMXTypeNeedsScale(DataType);
    if (Condition == "BType requires MX scale") {
      unsigned Type = DataType;
      if (!Body.empty() && Body.front().getOpcode() == LinxV5::BDATR)
        Type = Body.front().getOperand(2).getImm();
      return LinxV5Op::matrixMXTypeNeedsScale(Type);
    }
    if (!FPATR || FPATR->getNumOperands() != 10)
      return false;
    if (Condition == "RowMax&&RowMaxInit")
      return FPATR->getOperand(3).getImm() && FPATR->getOperand(5).getImm();
    if (Condition == "PreMode=vector")
      return FPATR->getOperand(0).getImm() &&
             FPATR->getOperand(0).getImm() != 1;
    if (Condition == "PreMode=scalar")
      return FPATR->getOperand(0).getImm() == 1;
    if (Condition == "PostMode=vector")
      return FPATR->getOperand(1).getImm() == 2;
    if (Condition == "PostMode=scalar")
      return FPATR->getOperand(1).getImm() == 1;
    if (Condition == "RowMax")
      return FPATR->getOperand(3).getImm() != 0;
    if (Condition == "GroupMax")
      return FPATR->getOperand(4).getImm() != 0;
    if (Condition == "CScale")
      return FPATR->getOperand(9).getImm() != 0;
    return false;
  };

  auto matchesForm = [&](const TileMacroFormDesc &Form) {
    if (!Form.UniqueWithoutRuntimeState)
      return false;
    const TileMacroConfigDesc *HeaderConfig = targetConfig(
        Form, TileMacroTargetKind::Header, TileMacroTargetSlot::DataType);
    if (!HeaderConfig)
      return false;
    StringRef HeaderField = HeaderConfig->Field;
    if ((HeaderField == "U8" && DataType != LinxV5Op::DataType::U8) ||
        (HeaderField == "U32" && DataType != LinxV5Op::DataType::U32) ||
        (HeaderField == "DTYPE_NONE" &&
         DataType != LinxV5Op::DataType::EMPTY_DataType))
      return false;

    unsigned Pos = 0;
    const MCInst *DATR = nullptr;
    const MCInst *FPATR = nullptr;
    if (Pos < Body.size() && Body[Pos].getOpcode() == LinxV5::BDATR)
      DATR = &Body[Pos++];
    if (Pos < Body.size() && Body[Pos].getOpcode() == LinxV5::B_FPATR)
      FPATR = &Body[Pos++];

    bool AllowsDATR = false;
    bool RequiresDATR = false;
    bool AllowsFPATR = false;
    bool RequiresFPATR = false;
    for (unsigned I = 0; I != Form.NumConfigs; ++I) {
      const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
      for (unsigned T = 0; T != Config.NumTargets; ++T) {
        const auto &Target = TileMacroConfigTargets[Config.FirstTarget + T];
        if (Target.Kind == TileMacroTargetKind::DATR) {
          AllowsDATR = true;
          RequiresDATR |= !Config.Optional && !Config.Default;
        }
        if (Target.Kind == TileMacroTargetKind::FPATR) {
          AllowsFPATR = true;
          RequiresFPATR |= !Config.Optional;
        }
      }
    }
    if ((DATR && !AllowsDATR) || (!DATR && RequiresDATR) ||
        (FPATR && !AllowsFPATR) || (!FPATR && RequiresFPATR))
      return false;
    if (DATR) {
      if (DATR->getNumOperands() != 8)
        return false;
      unsigned Layout = DATR->getOperand(0).getImm();
      unsigned Canon = DATR->getOperand(1).getImm();
      unsigned DstType = DATR->getOperand(2).getImm();
      unsigned Pad = DATR->getOperand(3).getImm();
      unsigned Cmp = DATR->getOperand(4).getImm();
      unsigned Round = DATR->getOperand(5).getImm();
      unsigned Sat = DATR->getOperand(6).getImm();
      unsigned Byte = DATR->getOperand(7).getImm();
      if (!hasTarget(Form, TileMacroTargetKind::DATR,
                     TileMacroTargetSlot::Layout, -1) &&
          Layout != 0)
        return false;
      const auto *LayoutConfig = targetConfig(Form, TileMacroTargetKind::DATR,
                                              TileMacroTargetSlot::Layout);
      if (LayoutConfig && StringRef(LayoutConfig->Field) == "WeightLayout" &&
          !LinxV5Op::isWeightLayout(Layout))
        return false;
      if (LayoutConfig && StringRef(LayoutConfig->Field) == "CubeLayout") {
        if ((StringRef(Form.Spelling).startswith("TLOAD") &&
             !LinxV5Op::isCubeLoadConversion(Layout)) ||
            (StringRef(Form.Spelling).startswith("TSTORE") &&
             !LinxV5Op::isCubeStoreConversion(Layout)))
          return false;
      }
      if (LayoutConfig && StringRef(LayoutConfig->Field) == "Layout") {
        if ((StringRef(Form.Spelling) == "TLOAD" &&
             LinxV5Op::isCubeLoadConversion(Layout)) ||
            (StringRef(Form.Spelling) == "TSTORE" &&
             LinxV5Op::isCubeStoreConversion(Layout)))
          return false;
      }
      if (!hasTarget(Form, TileMacroTargetKind::DATR,
                     TileMacroTargetSlot::Canonicalize, -1) &&
          Canon != 0)
        return false;
      if (!hasTarget(Form, TileMacroTargetKind::DATR,
                     TileMacroTargetSlot::DataType, -1) &&
          DstType != LinxV5Op::DataType::EMPTY_DataType)
        return false;
      if (!hasTarget(Form, TileMacroTargetKind::DATR,
                     TileMacroTargetSlot::CMode, -1) &&
          Cmp != 0)
        return false;
      if (!hasTarget(Form, TileMacroTargetKind::DATR,
                     TileMacroTargetSlot::RMode, -1) &&
          Round != 0)
        return false;
      if (!hasTarget(Form, TileMacroTargetKind::DATR, TileMacroTargetSlot::Sat,
                     -1) &&
          Sat != 0)
        return false;
      const auto *PadConfig =
          targetConfig(Form, TileMacroTargetKind::DATR,
                       TileMacroTargetSlot::PadValueOrByteId);
      if (!PadConfig) {
        unsigned DefaultPad = Operation->HeaderKind == TileMacroHeaderKind::CUBE
                                  ? 0
                                  : LinxV5Op::PadValue::Null;
        if (Pad != DefaultPad || Byte != 0)
          return false;
      }
      if (PadConfig && StringRef(PadConfig->Field) == "PadValue" && Byte != 0)
        return false;
      if (PadConfig && StringRef(PadConfig->Field) == "PadValueOrByteId" &&
          Pad != LinxV5Op::PadValue::Null && Byte != 0)
        return false;

    }

    bool SeenDim[3] = {false, false, false};
    int LastDim = -1;
    while (Pos < Body.size() && (Body[Pos].getOpcode() == LinxV5::B_DIM ||
                                 Body[Pos].getOpcode() == LinxV5::C_B_DIMI)) {
      unsigned Slot = Body[Pos].getOperand(0).getImm();
      if (Slot >= 3 || SeenDim[Slot] || int(Slot) <= LastDim)
        return false;
      TileMacroTargetSlot TargetSlot = Slot == 0   ? TileMacroTargetSlot::LB0
                                       : Slot == 1 ? TileMacroTargetSlot::LB1
                                                   : TileMacroTargetSlot::LB2;
      if (!hasTarget(Form, TileMacroTargetKind::DIM, TargetSlot, -1))
        return false;
      SeenDim[Slot] = true;
      LastDim = Slot;
      ++Pos;
    }
    // A missing B.DIM command denotes the architectural per-block default of
    // 1. It can therefore satisfy a form whose source-level dimension is
    // required even though no physical dimension command was encoded.

    const unsigned OperandStart = Pos;
    SmallVector<bool, 32> Consumed(Body.size(), false);
    for (unsigned I = 0; I != OperandStart; ++I)
      Consumed[I] = true;
    auto commandMatchesKind = [&](unsigned Position,
                                  TileMacroCommandKind Kind) {
      if (Position >= Body.size() || Consumed[Position])
        return false;
      if (Kind == TileMacroCommandKind::IOS)
        return Body[Position].getOpcode() == LinxV5::B_IOS;
      if (Kind == TileMacroCommandKind::IOR)
        return Body[Position].getOpcode() == LinxV5::B_IO;
      IOTShape Shape;
      return decodeIOTShape(Body[Position], Shape);
    };
    auto findCommand = [&](TileMacroCommandKind Kind) {
      for (unsigned I = OperandStart; I != Body.size(); ++I)
        if (commandMatchesKind(I, Kind))
          return I;
      return unsigned(Body.size());
    };
    auto markConsumed = [&](unsigned Begin, unsigned End) {
      for (unsigned I = Begin; I != End; ++I)
        Consumed[I] = true;
    };

    auto consumeRangeModifiers = [&](ArrayRef<unsigned> SourceMembers,
                                     int DestinationMember,
                                     unsigned &Position) {
      bool SourceSeen[2] = {false, false};
      bool DestinationSeen = false;
      while (Position < Body.size()) {
        const MCInst &Modifier = Body[Position];
        if (Modifier.getOpcode() == LinxV5::B_SUBVIEW) {
          if (Modifier.getNumOperands() != 4)
            return false;
          unsigned SourceSelect = Modifier.getOperand(0).getImm();
          if (SourceSelect >= SourceMembers.size() || SourceSelect >= 2 ||
              SourceSeen[SourceSelect])
            return false;
          unsigned Member = SourceMembers[SourceSelect];
          if (!(TileMacroBindingMembers[Member].ModifierFlags & 1) ||
              Modifier.getOperand(3).getImm() != 1)
            return false;
          SourceSeen[SourceSelect] = true;
          ++Position;
          continue;
        }
        if (Modifier.getOpcode() == LinxV5::B_ASSEMBLE) {
          if (Modifier.getNumOperands() != 5 || DestinationMember < 0 ||
              DestinationSeen ||
              !(TileMacroBindingMembers[DestinationMember].ModifierFlags & 2) ||
              Modifier.getOperand(0).getImm() != 1 ||
              Modifier.getOperand(1).getImm() != 1 ||
              Modifier.getOperand(4).getImm() != 1)
            return false;
          DestinationSeen = true;
          ++Position;
          continue;
        }
        break;
      }
      return true;
    };

    SmallVector<unsigned, 16> IOTPositions;
    bool SawMask = false;
    unsigned CommonMask = 0;
    for (unsigned C = 0; C != Form.NumCommands; ++C) {
      const auto &Expected = TileMacroCommands[Form.FirstCommand + C];
      SmallVector<unsigned, 8> Members;
      bool HasRequired = false;
      for (unsigned M = 0; M != Expected.NumMembers; ++M) {
        unsigned Member =
            TileMacroCommandMembers[Expected.FirstMember + M].BindingMember;
        const auto &Desc = TileMacroBindingMembers[Member];
        bool Enabled =
            !Desc.Condition || conditionEnabled(Desc.Condition, FPATR);
        if (!Enabled)
          continue;
        Members.push_back(Member);
        HasRequired |= !Desc.Optional && !Desc.Default;
        HasRequired |= Desc.Condition != nullptr;
      }
      if (Members.empty())
        continue;
      Pos = findCommand(Expected.Kind);
      if (Expected.Kind == TileMacroCommandKind::IOT) {
        if (Pos >= Body.size()) {
          if (HasRequired)
            return false;
          continue;
        }
        unsigned RequiredSources = 0, MaxSources = 0;
        bool RequiredDestination = false, AllowsDestination = false;
        for (unsigned Member : Members) {
          const auto *Binding = bindingForMember(Form, Member);
          bool Destination = Binding && isLocalDestination(Binding->Kind);
          if (Destination) {
            AllowsDestination = true;
            RequiredDestination |= !TileMacroBindingMembers[Member].Optional;
          } else {
            ++MaxSources;
            RequiredSources += !TileMacroBindingMembers[Member].Optional;
          }
        }
        SmallVector<unsigned, 2> SourceMembers;
        int DestinationMember = -1;
        for (unsigned Member : Members) {
          const auto *Binding = bindingForMember(Form, Member);
          if (Binding && isLocalDestination(Binding->Kind))
            DestinationMember = Member;
          else
            SourceMembers.push_back(Member);
        }
        unsigned SeenSources = 0;
        bool SeenDestination = false;
        do {
          Pos = findCommand(TileMacroCommandKind::IOT);
          if (Pos >= Body.size())
            return false;
          IOTShape Actual;
          if (!decodeIOTShape(Body[Pos], Actual) ||
              SeenSources + Actual.Sources > MaxSources ||
              (Actual.Destination &&
               (!AllowsDestination || SeenDestination)))
            return false;
          if (!SawMask) {
            SawMask = true;
            CommonMask = Actual.Mask;
          } else if (CommonMask != Actual.Mask)
            return false;
          SmallVector<unsigned, 2> RangeSources;
          for (unsigned I = 0; I != Actual.Sources; ++I)
            RangeSources.push_back(SourceMembers[SeenSources + I]);
          unsigned CommandStart = Pos;
          IOTPositions.push_back(Pos++);
          if (!consumeRangeModifiers(
                  RangeSources,
                  Actual.Destination ? DestinationMember : -1, Pos))
            return false;
          markConsumed(CommandStart, Pos);
          SeenSources += Actual.Sources;
          SeenDestination |= Actual.Destination;
        } while (SeenSources < RequiredSources ||
                 (RequiredDestination && !SeenDestination));
      } else if (Expected.Kind == TileMacroCommandKind::IOS) {
        for (unsigned Member : Members) {
          const auto *Binding = bindingForMember(Form, Member);
          bool Required = !TileMacroBindingMembers[Member].Optional ||
                          TileMacroBindingMembers[Member].Condition;
          if (Pos >= Body.size() || Body[Pos].getOpcode() != LinxV5::B_IOS) {
            if (Required)
              return false;
            continue;
          }
          bool Destination =
              Binding &&
              Binding->Kind == TileMacroBindingKind::SharedDestination;
          unsigned SizeCode = Body[Pos].getOperand(2).getImm();
          if (Destination != (SizeCode != 0))
            return false;
          unsigned Mask = Body[Pos].getOperand(1).getImm();
          if (!SawMask) {
            SawMask = true;
            CommonMask = Mask;
          } else if (CommonMask != Mask)
            return false;
          ++Pos;
          SmallVector<unsigned, 1> RangeSources;
          int RangeDestination = -1;
          if (Destination)
            RangeDestination = Member;
          else
            RangeSources.push_back(Member);
          unsigned CommandStart = Pos - 1;
          if (!consumeRangeModifiers(RangeSources, RangeDestination, Pos))
            return false;
          markConsumed(CommandStart, Pos);
        }
      } else {
        bool Present =
            Pos < Body.size() && Body[Pos].getOpcode() == LinxV5::B_IO;
        if (!Present) {
          if (HasRequired)
            return false;
          continue;
        }
        bool Allowed[5] = {false, false, false, false, false};
        bool Required[5] = {false, false, false, false, false};
        for (unsigned Member : Members) {
          const auto &Desc = TileMacroBindingMembers[Member];
          unsigned Slot = Desc.Slot == TileMacroMemberSlot::RegDst    ? 0
                          : Desc.Slot == TileMacroMemberSlot::RegSrc0 ? 1
                          : Desc.Slot == TileMacroMemberSlot::RegSrc1 ? 2
                          : Desc.Slot == TileMacroMemberSlot::RegSrc2 ? 3
                                                                      : 4;
          Allowed[Slot] = true;
          Required[Slot] = !Desc.Optional && !Desc.Default;
          Required[Slot] |= Desc.Condition != nullptr;
        }
        for (unsigned Slot = 0; Slot != 4; ++Slot) {
          bool Nonzero = Body[Pos].getOperand(Slot).getReg() != LinxV5::R0;
          if ((Nonzero && !Allowed[Slot]) || (!Nonzero && Required[Slot]))
            return false;
        }
        unsigned CommandStart = Pos++;
        if (Allowed[4]) {
          if (Pos >= Body.size() || Body[Pos].getOpcode() != LinxV5::B_IO)
            return !Required[4] && Pos == Body.size();
          if (Body[Pos].getOperand(0).getReg() != LinxV5::R0 ||
              Body[Pos].getOperand(2).getReg() != LinxV5::R0 ||
              Body[Pos].getOperand(3).getReg() != LinxV5::R0 ||
              (Body[Pos].getOperand(1).getReg() == LinxV5::R0 && Required[4]))
            return false;
          ++Pos;
        }
        markConsumed(CommandStart, Pos);
      }
    }
    for (unsigned I = OperandStart; I != Body.size(); ++I)
      if (!Consumed[I])
        return false;
    for (unsigned I = 0; I != IOTPositions.size(); ++I) {
      IOTShape Shape;
      decodeIOTShape(Body[IOTPositions[I]], Shape);
      if (Shape.Last != (I + 1 == IOTPositions.size()))
        return false;
    }
    for (unsigned I = 0; I != Form.NumConfigs; ++I) {
      const auto &Config = TileMacroConfigs[Form.FirstConfig + I];
      if (Config.Constraint && StringRef(Config.Constraint) == "AllPE" &&
          SawMask && CommonMask != 0xf)
        return false;
    }
    return true;
  };

  const TileMacroFormDesc *MatchedForm = nullptr;
  unsigned FormIndex = 0;
  for (unsigned I = 0; I != Operation->NumForms; ++I) {
    unsigned CandidateIndex = Operation->FirstForm + I;
    if (NonCanonicalBoundary || !matchesForm(TileMacroForms[CandidateIndex]))
      continue;
    if (MatchedForm)
      return false;
    MatchedForm = &TileMacroForms[CandidateIndex];
    FormIndex = CandidateIndex;
  }
  if (!MatchedForm)
    return false;
  MI.clear();
  MI.setOpcode(LinxV5::PseudoTileMacroDisasm);
  MI.addOperand(MCOperand::createImm(FormIndex));
  MI.addOperand(MCOperand::createImm(DataType));
  MI.addOperand(MCOperand::createImm(Commands.size()));
  for (const MCInst &Command : Commands) {
    MI.addOperand(MCOperand::createImm(Command.getOpcode()));
    MI.addOperand(MCOperand::createImm(Command.getNumOperands()));
    for (const MCOperand &Operand : Command) {
      if (Operand.isReg()) {
        MI.addOperand(MCOperand::createImm(1));
        MI.addOperand(MCOperand::createImm(Operand.getReg()));
      } else if (Operand.isImm()) {
        MI.addOperand(MCOperand::createImm(0));
        MI.addOperand(MCOperand::createImm(Operand.getImm()));
      } else {
        return false;
      }
    }
  }
  Size = Offset;
  return true;
}

DecodeStatus LinxV5Disassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                                ArrayRef<uint8_t> Bytes,
                                                uint64_t Address,
                                                raw_ostream &CS) const {
  if (useLinxV5TileMacroAliases() &&
      tryDecodeTileMacro(MI, Size, Bytes, Address, this, STI, *MCII))
    return MCDisassembler::Success;
  return decodeOneLinxV5Instruction(MI, Size, Bytes, Address, this, STI);
}
