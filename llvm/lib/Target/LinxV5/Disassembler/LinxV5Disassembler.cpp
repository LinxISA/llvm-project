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
      Result = decodeInstruction(DecoderTable32, MI, Insn, Address, this, STI);
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
