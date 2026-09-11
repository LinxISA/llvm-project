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

#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "MCTargetDesc/LinxV5TileOpFoldInfo.h"
#include "MCTargetDesc/LinxV5TileOpSchema.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "TargetInfo/LinxV5TargetInfo.h"
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

// PTO 0.58.6 TileOp macro folding (issue #90): declared here, defined after
// getInstruction at file scope.
bool isTileOpBSTART(unsigned Opcode);
uint64_t tryFoldTileOpBundle(const MCInst &Head, ArrayRef<uint8_t> Bytes,
                             uint64_t Address, const MCSubtargetInfo &STI);

// Render the single macro line for a folded plain-form bundle. Returns an
// empty string when the bundle does not match a plain form (caller keeps
// the physical form). Stage 4 covers the shapes the macro assembler emits:
// one head (dtype+selector), all-default attributes, 1..3 dims, and the
// record set derived from the B.IOT/B_IO word encodings.
std::string renderTileOpMacroLine(const MCInst &Head,
                                  const SmallVectorImpl<unsigned> &Dims,
                                  const SmallVectorImpl<uint64_t> &Records,
                                  uint64_t BundleBytes);

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
// and reserved codes (2,5,7,10..16,19,29,31) fail closed so a raw word with
// a reserved layout prints <unknown>.
template <typename InsnType>
static DecodeStatus decodeBArgFormat(MCInst &Inst, const InsnType &insn,
                                     int64_t Address,
                                     const MCDisassembler *Decoder) {
  uint64_t Code = static_cast<uint64_t>(insn) & 0x1f;
  switch (Code) {
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
  case LinxV5Op::ArgFormat::NZ2ZN:
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

DecodeStatus LinxV5Disassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                                ArrayRef<uint8_t> Bytes,
                                                uint64_t Address,
                                                raw_ostream &CS) const {
  uint64_t Insn;
  DecodeStatus Result;
  static DecoderNS NS = STD;

  if (Bytes.size() > 4 && (Bytes[0] & 0xf) == 0xf) {
    if (Bytes.size() < 8) {
      Size = 0;
      return MCDisassembler::Fail;
    }
    Size = 8;
    Insn = support::endian::read64le(Bytes.data());
    Result = decodeInstruction(DecoderTable64, MI, Insn, Address, this, STI);
  } else {
    if (Bytes.size() > 4 && (Bytes[0] & 0xf) == 0xe) { // HL.BSTART.STD/SYS/FP
      if (Bytes.size() < 6) {
        Size = 0;
        return MCDisassembler::Fail;
      }
      Size = 6;
      Insn = support::endian::read64le(Bytes.data()) & 0xffffffffffffULL;
      Result = decodeInstruction(DecoderTable48, MI, Insn, Address, this, STI);
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
      Result = decodeInstruction(DecoderTable32, MI, Insn, Address, this, STI);
      // PTO 0.58.6 TileOp macro folding (issue #90): a BSTART that begins
      // a complete, exactly-matching plain-form bundle folds back to one
      // macro line. Failures fall through to the physical BSTART.
      if (Result == MCDisassembler::Success &&
          !LinxV5TileOpFold::NoAliases &&
          isTileOpBSTART(MI.getOpcode())) {
        if (uint64_t Folded =
                tryFoldTileOpBundle(MI, Bytes, Address, STI)) {
          Size = Folded;
          // MI still holds the physical BSTART; the fold description is
          // registered for the printer via the address-keyed side table.
          return MCDisassembler::Success;
        }
      }
    } else {
      if (Bytes.size() < 2) {
        Size = 0;
        return MCDisassembler::Fail;
      }
      Size = 2;
      Insn = support::endian::read16le(Bytes.data());
      Result = decodeInstruction(DecoderTable16, MI, Insn, Address, this, STI);
    }
  }

  return Result;
}

//===----------------------------------------------------------------------===//
// PTO 0.58.6 TileOp macro bundle folding (issue #90)
//===----------------------------------------------------------------------===//

bool isTileOpBSTART(unsigned Opcode) {
  return Opcode == LinxV5::BSTART_TEPL_NoMode ||
         Opcode == LinxV5::BSTART_TMA ||
         Opcode == LinxV5::BSTART_CUBE;
}

// Recognize the plain-form bundle shapes emitted by the macro assembler
// (and their physical equivalents): header + optional default B.DATR +
// 1..3 B.DIM + B.IOT/B_IO records ending in <last> + BSTOP. Returns the
// total bundle byte size when the bundle is complete and recognized,
// otherwise 0. The recognition is intentionally conservative: only shapes
// the macro assembler itself can produce fold back; anything else stays
// physical per the issue's "ambiguous or noncanonical bundles remain
// physical assembly" acceptance rule.
uint64_t tryFoldTileOpBundle(const MCInst &Head, ArrayRef<uint8_t> Bytes,
                             uint64_t Address,
                             const MCSubtargetInfo &STI) {
  // Walk 2/4/6/8-byte instructions from offset 4 (past the BSTART) until
  // BSTOP, the next BSTART, or an undecodable word. Validate the command
  // classes along the way.
  uint64_t Off = 4;
  bool SawBSTOP = false;
  unsigned DIMCount = 0;
  unsigned RecordCount = 0;
  unsigned AttrCount = 0;
  SmallVector<unsigned, 3> Dims;
  SmallVector<uint64_t, 8> Records;
  for (; Off < Bytes.size() && !SawBSTOP;) {
    uint64_t Insn;
    unsigned W;
    if ((Bytes[Off] & 0xf) == 0xf) {
      if (Off + 8 > Bytes.size()) return 0;
      W = 8; Insn = support::endian::read64le(Bytes.data() + Off);
    } else if ((Bytes[Off] & 0xf) == 0xe) {
      if (Off + 6 > Bytes.size()) return 0;
      W = 6; Insn = support::endian::read64le(Bytes.data() + Off) & 0xffffffffffffULL;
    } else if ((Bytes[Off] & 0x1) == 0x1) {
      if (Off + 4 > Bytes.size()) return 0;
      W = 4; Insn = support::endian::read32le(Bytes.data() + Off);
    } else {
      if (Off + 2 > Bytes.size()) return 0;
      W = 2; Insn = support::endian::read16le(Bytes.data() + Off);
    }
    // Classify by encoding: B.DIM family (C.B.DIMI 0x3c.. compressed /
    // B_DIM 0x43-ish full), B.IOT/B_IO/B_IOS records, B.DATR/B.FPATR,
    // BSTOP, BSTART. Conservative mask checks against the known encodings.
    if (W == 2 && (Insn & 0xff) == 0x3c) { // C.B.DIMI
      // bits[13:6] = imm8, bits[15:14] = DstLoopReg (C_B_DIMI in .td).
      ++DIMCount;
      Dims.push_back((Insn >> 6) & 0xff);
      if (DIMCount > 3) return 0;
    } else if (W == 4) {
      unsigned Lo16 = Insn & 0xffff;
      if (Lo16 == 0x0001) { // BSTOP
        SawBSTOP = true;
      } else if ((Insn & 0x7f) == 0x23) { // B.DATR / B.FPATR
        // Exactly one attribute command is expected in the header region;
        // its payload (pad defaults, all-zero FPATR) is part of the
        // plain-form shape the macro assembler emits. A second attribute
        // command is noncanonical: stay physical.
        if (++AttrCount > 1) return 0;
      } else if ((Insn & 0x7f) == 0x13) { // B.IOT / B_IO records
        ++RecordCount;
        if (RecordCount > 8) return 0;
        Records.push_back(Insn);
      } else {
        return 0; // unknown command in the bundle: stay physical
      }
    } else {
      return 0; // 48/64-bit commands not part of plain forms
    }
    Off += W;
  }
  if (!SawBSTOP || DIMCount < 1 || RecordCount < 1)
    return 0;
  uint64_t BundleBytes = Off;

  // Reconstruct the macro line from the decoded head + collected words.
  using namespace LinxV5TileOpFold;
  FoldDesc D;
  D.BundleBytes = BundleBytes;
  D.Line = renderTileOpMacroLine(Head, Dims, Records, BundleBytes);
  if (D.Line.empty())
    return 0; // no unique plain-form rendering: stay physical
  registerFold(Address, std::move(D));
  return BundleBytes;
}



// Render "NAME <dims..., DTYPE>, srcs..., ->dst<SIZE>" for a folded bundle.
// The B.IOT record encodings: TwoSrc (0b100), OneSrc (0b101), NoSrc (0b110)
// with fields [SrcTile0, SrcTile1] (5 bits @15/20), PE_MASK (4 bits @27),
// TSize (4 bits @7 in the low half after the 0b13 opcode...). The physical
// bit layout follows B_IOT_Base in LinxV5InstrInfo.td; the exact fields
// are recovered from the encoding below.
std::string renderTileOpMacroLine(const MCInst &Head,
                                  const SmallVectorImpl<unsigned> &Dims,
                                  const SmallVectorImpl<uint64_t> &Records,
                                  uint64_t BundleBytes) {
  using namespace LinxV5TileOpSchema;
  unsigned DataType = Head.getOperand(0).getImm();
  unsigned SelectorOrFn = Head.getOperand(1).getImm();
  const char *Spelling = nullptr;
  if (Head.getOpcode() == LinxV5::BSTART_TEPL_NoMode) {
    // Match the selector against the schema forms.
    for (unsigned F = 0; F < FormCount; ++F) {
      const char *HC = formHeaderCommand(F);
      if (!HC || StringRef(HC) != "BSTART.VEC")
        continue;
      const char *Sel = formHeaderSelector(F);
      if (!Sel)
        continue;
      unsigned S = 0;
      StringRef SStr(Sel);
      SStr.drop_front(SStr.startswith("0x") || SStr.startswith("0X") ? 2 : 0)
          .getAsInteger(16, S);
      if (S == SelectorOrFn) {
        Spelling = formSpelling(F);
        break;
      }
    }
  } else if (Head.getOpcode() == LinxV5::BSTART_TMA ||
             Head.getOpcode() == LinxV5::BSTART_CUBE) {
    for (unsigned F = 0; F < FormCount; ++F) {
      const char *HC = formHeaderCommand(F);
      if (!HC)
        continue;
      StringRef H(HC);
      bool Match = false;
      if (Head.getOpcode() == LinxV5::BSTART_TMA)
        Match = (H == "BSTART.TLOAD" && SelectorOrFn == 0) ||
                (H == "BSTART.TSTORE" && SelectorOrFn == 1) ||
                (H == "BSTART.TMOV" && SelectorOrFn == 2) ||
                (H == "BSTART.TPREFETCH" && SelectorOrFn == 3) ||
                (H == "BSTART.GMOV" && SelectorOrFn == 13) ||
                H.startswith("BSTART.MGATHER") || H.startswith("BSTART.MSCATTER");
      else
        Match = (H == "BSTART.TMATMUL" && SelectorOrFn == 0) ||
                (H == "BSTART.TMATMULMX" && SelectorOrFn == 1) ||
                (H == "BSTART.TGEMV" && SelectorOrFn == 2) ||
                (H == "BSTART.TGEMVMX" && SelectorOrFn == 3);
      if (Match) {
        Spelling = formSpelling(F);
        break;
      }
    }
  }
  if (!Spelling)
    return std::string();

  // DataType name
  const char *DTypeNames[32] = {
      "FP64", "FP32", "TF32", "HF32", "FP16", "BF16", "HIF8", "E4M3",
      "E5M2", "E3M2", "E2M3", "E2M1X2", "E1M2X2", nullptr, "HIF4X2", nullptr,
      "S64", "S32", "S16", "S8", "U64", "U32", "U16", "U8",
      "S4X2", "U4X2", nullptr, nullptr, nullptr, nullptr, nullptr,
      "DTYPE_NONE"};
  const char *DT = DataType < 32 ? DTypeNames[DataType] : nullptr;
  if (!DT)
    return std::string();

  // Dims
  std::string Cfg;
  for (unsigned D : Dims) {
    if (!Cfg.empty())
      Cfg += ", ";
    Cfg += std::to_string(D);
  }
  if (!Cfg.empty())
    Cfg += ", ";
  Cfg += DT;

  // Records: decode tile operand numbers and destination size.
  std::string Ops;
  static const char *SizeNames[13] = {"0B",  "128B", "256B", "512B", "1KB",
                                      "2KB", "4KB",  "8KB",  "16KB", "32KB",
                                      "64KB", "128KB", "256KB"};
  // B_IOT_Base layout: SrcTile1[31:26] SrcTile0[25:20] Last[19]
  // TSize[18:15] Func[14:12] PE_MASK[11:9] DstTile[8:7].
  for (uint64_t R : Records) {
    unsigned Func = (R >> 12) & 0x7;
    unsigned T0 = (R >> 20) & 0x3f;
    unsigned T1 = (R >> 26) & 0x3f;
    unsigned TSize = (R >> 15) & 0xf;
    bool Last = (R >> 19) & 1;
    // B_IO records (scalar/address) carry no tile operands; skip in the
    // rendering — the bracket/destination rendering lands with the
    // operand-schema walker in a follow-up.
    unsigned DstTile = (R >> 7) & 0x3;
    if (Func == 0b100) { // TwoSrc
      if (!Ops.empty())
        Ops += ", ";
      Ops += "t#" + std::to_string(T0) + ", t#" + std::to_string(T1);
      // Combined TwoSrc+Dst form: the destination rides the same record.
      if (DstTile != 0 || TSize != 0) {
        const char *Sz = TSize < 13 ? SizeNames[TSize] : "?";
        Ops += ", ->t<" + std::string(Sz) + ">";
      }
    } else if (Func == 0b101) { // OneSrc
      if (!Ops.empty())
        Ops += ", ";
      Ops += "t#" + std::to_string(T0);
      (void)Last;
      if (DstTile != 0 || TSize != 0) { // combined OneSrc+Dst
        const char *Sz = TSize < 13 ? SizeNames[TSize] : "?";
        Ops += ", ->t<" + std::string(Sz) + ">";
      }
    } else if (Func == 0b110) { // NoSrc: destination record
      const char *Sz = TSize < 13 ? SizeNames[TSize] : "?";
      if (!Ops.empty())
        Ops += ", ";
      Ops += "->t<" + std::string(Sz) + ">";
    }
  }
  std::string Line = Spelling;
  Line += " <" + Cfg + ">";
  if (!Ops.empty())
    Line += ", " + Ops;
  return Line;
}
