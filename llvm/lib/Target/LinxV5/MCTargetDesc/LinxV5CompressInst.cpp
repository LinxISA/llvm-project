//===- LinxV5CompressInst.cpp ---------------------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Maybe oneday we can add LinxV5 feature to TableGen/CompressInstEmitter.cpp,
// and support CompressPat like RISCV.
// For now, CompressPat only support pure register operand, and imm operand.
//
//===----------------------------------------------------------------------===//

#include "LinxV5CompressInst.h"
#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "llvm/Support/CommandLine.h"

namespace llvm {

// Only control by wireless
static cl::opt<cl::boolOrDefault>
    EnableCompressInst("linxv5-enable-compress-inst",
                       cl::desc("Enable compress inst."), cl::Hidden);

static bool shouldCompress(const MCSubtargetInfo &STI) {
  if (EnableCompressInst == cl::BOU_UNSET)
    return STI.getCPU().str() != "v0.43w";
  else
    return EnableCompressInst == cl::BOU_TRUE;
}

const DenseMap<unsigned, unsigned> CMap = {
#define CInst(OPC)                                                             \
  { LinxV5::OPC, LinxV5::C_##OPC }
    CInst(ADDI),
    CInst(SUB),
    CInst(AND),
    CInst(OR),
    CInst(SETC_EQ),
    CInst(SETC_NE),
    CInst(LWI),
    CInst(LDI),
    CInst(SWI),
    CInst(SDI),
    CInst(CMP_EQI),
    CInst(CMP_NEI),
    CInst(SLLI),
    CInst(SRLI),
    CInst(SETC_TGT),
#undef CInst
    {LinxV5::SETC_EQI, LinxV5::C_SETC_EQ},
    {LinxV5::SETC_NEI, LinxV5::C_SETC_NE},
    {LinxV5::CMP_EQ, LinxV5::C_CMP_EQI},
    {LinxV5::CMP_NE, LinxV5::C_CMP_NEI},
    {LinxV5::BSTART_STD_WITHOUT_TARGET_32_FALL,
     LinxV5::BSTART_STD_WITHOUT_TARGET_16},
    {LinxV5::BSTART_STD_WITHOUT_TARGET_32_RET,
     LinxV5::BSTART_STD_WITHOUT_TARGET_16},
    {LinxV5::BSTART_STD_WITHOUT_TARGET_32_IND,
     LinxV5::BSTART_STD_WITHOUT_TARGET_16},
    {LinxV5::BSTART_STD_WITHOUT_TARGET_32_ICALL,
     LinxV5::BSTART_STD_WITHOUT_TARGET_16},
    {LinxV5::BSTART_AUX_WITHOUT_TARGET_32_FALL,
     LinxV5::BSTART_AUX_WITHOUT_TARGET_16},
    {LinxV5::BSTART_AUX_WITHOUT_TARGET_32_RET,
     LinxV5::BSTART_AUX_WITHOUT_TARGET_16},
    {LinxV5::BSTART_AUX_WITHOUT_TARGET_32_IND,
     LinxV5::BSTART_AUX_WITHOUT_TARGET_16},
    {LinxV5::BSTART_AUX_WITHOUT_TARGET_32_ICALL,
     LinxV5::BSTART_AUX_WITHOUT_TARGET_16},
    {LinxV5::BSTART_FP_WITHOUT_TARGET_32_FALL,
     LinxV5::BSTART_FP_WITHOUT_TARGET_16},
    {LinxV5::BSTART_FP_WITHOUT_TARGET_32_RET,
     LinxV5::BSTART_FP_WITHOUT_TARGET_16},
    {LinxV5::BSTART_FP_WITHOUT_TARGET_32_IND,
     LinxV5::BSTART_FP_WITHOUT_TARGET_16},
    {LinxV5::BSTART_FP_WITHOUT_TARGET_32_ICALL,
     LinxV5::BSTART_FP_WITHOUT_TARGET_16},
};

bool LinxV5::tryCompressInst(MCInst &OutInst, const MCInst &MI,
                             const MCSubtargetInfo &STI, MCContext &Context) {

  if (!shouldCompress(STI))
    return false;

  OutInst.clear();
  switch (MI.getOpcode()) {
  default:
    break;
  case LinxV5::ADD: {
    if (MI.getOperand(1).getReg() == LinxV5::R0 &&
        MI.getOperand(3).getImm() == 0) {
      OutInst.setOpcode(LinxV5::MOVR);
      OutInst.addOperand(MI.getOperand(0));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    } else if (MI.getOperand(0).getReg() == LinxV5::T &&
               MI.getOperand(3).getImm() == 0) {
      OutInst.setOpcode(LinxV5::C_ADD);
      OutInst.addOperand(MI.getOperand(1));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::ADDI: {
    if (MI.getOperand(0).getReg() != LinxV5::R10 &&
        MI.getOperand(1).getReg() == LinxV5::R0 && MI.getOperand(2).isImm() &&
        isInt<5>(MI.getOperand(2).getImm())) {
      OutInst.setOpcode(LinxV5::MOVI);
      OutInst.addOperand(MI.getOperand(0));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    } else if (MI.getOperand(0).getReg() == LinxV5::T &&
               MI.getOperand(2).isImm() &&
               isInt<5>(MI.getOperand(2).getImm())) {
      OutInst.setOpcode(LinxV5::C_ADDI);
      OutInst.addOperand(MI.getOperand(1));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::ADDIW: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(2).getImm() == 0) {
      OutInst.setOpcode(LinxV5::SEXT_W);
      OutInst.addOperand(MI.getOperand(1));
      return true;
    }
    break;
  }
  case LinxV5::SUBI: {
    if (MI.getOperand(0).getReg() != LinxV5::R10 &&
        MI.getOperand(1).getReg() == LinxV5::R0 && MI.getOperand(2).isImm() &&
        isInt<5>(MI.getOperand(2).getImm())) {
      OutInst.setOpcode(LinxV5::MOVI);
      OutInst.addOperand(MI.getOperand(0));
      OutInst.addOperand(MCOperand::createImm(-MI.getOperand(2).getImm()));
      return true;
    }
    break;
  }
  case LinxV5::ORI: {
    if (MI.getOperand(2).getImm() == 0) {
      OutInst.setOpcode(LinxV5::MOVR);
      OutInst.addOperand(MI.getOperand(0));
      OutInst.addOperand(MI.getOperand(1));
      return true;
    }
    break;
  }
  case LinxV5::SUB:
  case LinxV5::AND:
  case LinxV5::OR: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(3).getImm() == 0) {
      OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
      OutInst.addOperand(MI.getOperand(1));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::OR_SW: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(1).getReg() == LinxV5::R0 &&
        MI.getOperand(3).getImm() == 0) {
      OutInst.setOpcode(LinxV5::SEXT_W);
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::OR_UW: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(1).getReg() == LinxV5::R0 &&
        MI.getOperand(3).getImm() == 0) {
      OutInst.setOpcode(LinxV5::ZEXT_W);
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::BXU: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(2).getImm() == 0) {
      if (MI.getOperand(3).getImm() == 8) {
        OutInst.setOpcode(LinxV5::ZEXT_B);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
      if (MI.getOperand(3).getImm() == 16) {
        OutInst.setOpcode(LinxV5::ZEXT_H);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
      if (MI.getOperand(3).getImm() == 32) {
        OutInst.setOpcode(LinxV5::ZEXT_W);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
    }
    break;
  }
  case LinxV5::BXS: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(2).getImm() == 0) {
      if (MI.getOperand(3).getImm() == 8) {
        OutInst.setOpcode(LinxV5::SEXT_B);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
      if (MI.getOperand(3).getImm() == 16) {
        OutInst.setOpcode(LinxV5::SEXT_H);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
      if (MI.getOperand(3).getImm() == 32) {
        OutInst.setOpcode(LinxV5::SEXT_W);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
    }
    break;
  }
  case LinxV5::BIC: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        (MI.getOperand(2).getImm() + MI.getOperand(3).getImm()) == 64) {
      if (MI.getOperand(2).getImm() == 8) {
        OutInst.setOpcode(LinxV5::ZEXT_B);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
      if (MI.getOperand(2).getImm() == 16) {
        OutInst.setOpcode(LinxV5::ZEXT_H);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
      if (MI.getOperand(2).getImm() == 32) {
        OutInst.setOpcode(LinxV5::ZEXT_W);
        OutInst.addOperand(MI.getOperand(1));
        return true;
      }
    }
    break;
  }
  case LinxV5::SETC_EQ:
  case LinxV5::SETC_NE: {
    OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MI.getOperand(0));
    OutInst.addOperand(MI.getOperand(1));
    return true;
  }
  case LinxV5::SETC_EQI:
  case LinxV5::SETC_NEI: {
    if (MI.getOperand(1).getImm() == 0) {
      OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
      OutInst.addOperand(MI.getOperand(0));
      OutInst.addOperand(MCOperand::createReg(LinxV5::R0));
      return true;
    }
    break;
  }
  case LinxV5::LWI:
  case LinxV5::LDI: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        isInt<5>(MI.getOperand(2).getImm())) {
      OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
      OutInst.addOperand(MI.getOperand(1));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::SETC_TGT: {
    OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MI.getOperand(0));
    return true;
  }
  case LinxV5::SWI:
  case LinxV5::SDI: {
    if (MI.getOperand(0).getReg() == LinxV5::TOS1 &&
        isInt<5>(MI.getOperand(2).getImm())) {
      OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
      OutInst.addOperand(MI.getOperand(1));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::CMP_EQ:
  case LinxV5::CMP_NE: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(1).getReg() == LinxV5::TOS1 &&
        MI.getOperand(2).getReg() == LinxV5::R0) {
      OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
      OutInst.addOperand(MCOperand::createImm(0));
      return true;
    }
    break;
  }
  case LinxV5::CMP_EQI:
  case LinxV5::CMP_NEI: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(1).getReg() == LinxV5::TOS1 &&
        isInt<5>(MI.getOperand(2).getImm())) {
      OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::SLLI:
  case LinxV5::SRLI: {
    if (MI.getOperand(0).getReg() == LinxV5::T &&
        MI.getOperand(1).getReg() == LinxV5::TOS1 &&
        isUInt<5>(MI.getOperand(2).getImm())) {
      OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    break;
  }
  case LinxV5::BSTART_STD_WITHOUT_TARGET_32_FALL:
  case LinxV5::BSTART_AUX_WITHOUT_TARGET_32_FALL:
  case LinxV5::BSTART_FP_WITHOUT_TARGET_32_FALL: {
    OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createImm(LinxV5Op::BranchType::FALL));
    return true;
  }
  case LinxV5::BSTART_STD_WITHOUT_TARGET_32_RET:
  case LinxV5::BSTART_AUX_WITHOUT_TARGET_32_RET:
  case LinxV5::BSTART_FP_WITHOUT_TARGET_32_RET: {
    OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createImm(LinxV5Op::BranchType::RET));
    return true;
  }
  case LinxV5::BSTART_STD_WITHOUT_TARGET_32_IND:
  case LinxV5::BSTART_AUX_WITHOUT_TARGET_32_IND:
  case LinxV5::BSTART_FP_WITHOUT_TARGET_32_IND: {
    OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createImm(LinxV5Op::BranchType::IND));
    return true;
  }
  case LinxV5::BSTART_STD_WITHOUT_TARGET_32_ICALL:
  case LinxV5::BSTART_AUX_WITHOUT_TARGET_32_ICALL:
  case LinxV5::BSTART_FP_WITHOUT_TARGET_32_ICALL: {
    OutInst.setOpcode(CMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createImm(LinxV5Op::BranchType::ICALL));
    return true;
  }
  case LinxV5::B_DIM: {
    if (MI.getOperand(1).getReg() != LinxV5::R0 &&
        MI.getOperand(2).getImm() != 0) {
      return false;
    }
    if (MI.getOperand(1).getReg() == LinxV5::R0 && isUInt<8>(MI.getOperand(2).getImm())) {
      OutInst.setOpcode(LinxV5::C_B_DIMI);
      OutInst.addOperand(MI.getOperand(0));
      OutInst.addOperand(MI.getOperand(2));
      return true;
    }
    // v5: C.B.DIM RegSrc compressed form removed; replaced by C.B.IOS Shared
    // operand binder. The imm==0 -> C.B.DIM(RegSrc) compression is gone.
    break;
  }
  }

  return false;
}

const DenseMap<unsigned, unsigned> RCMap = {
#define CInst(OPC)                                                             \
  { LinxV5::C_##OPC, LinxV5::OPC }
    CInst(ADD),     CInst(SUB),     CInst(AND),  CInst(OR),
    CInst(SETC_EQ), CInst(SETC_NE), CInst(LWI),  CInst(LDI),
    CInst(SWI),     CInst(SDI),     CInst(ADDI), CInst(SETC_TGT),
    CInst(CMP_EQI), CInst(CMP_NEI), CInst(SLLI), CInst(SRLI)
#undef CInst
};

bool LinxV5::unCompressInst(MCInst &OutInst, const MCInst &MI,
                            const MCRegisterInfo &MRI,
                            const MCSubtargetInfo &STI) {
  OutInst.clear();
  switch (MI.getOpcode()) {
  default:
    break;
  case LinxV5::C_ADD:
  case LinxV5::C_SUB:
  case LinxV5::C_AND:
  case LinxV5::C_OR: {
    OutInst.setOpcode(RCMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createReg(LinxV5::T));
    OutInst.addOperand(MI.getOperand(0));
    OutInst.addOperand(MI.getOperand(1));
    OutInst.addOperand(MCOperand::createImm(0));
    return true;
  }
  case LinxV5::C_SETC_EQ:
  case LinxV5::C_SETC_NE: {
    OutInst.setOpcode(RCMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MI.getOperand(0));
    OutInst.addOperand(MI.getOperand(1));
    return true;
  }
  case LinxV5::C_LWI:
  case LinxV5::C_LDI: {
    OutInst.setOpcode(RCMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createReg(LinxV5::T));
    OutInst.addOperand(MI.getOperand(0));
    OutInst.addOperand(MI.getOperand(1));
    return true;
  }
  case LinxV5::C_SWI:
  case LinxV5::C_SDI: {
    OutInst.setOpcode(RCMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createReg(LinxV5::TOS1));
    OutInst.addOperand(MI.getOperand(0));
    OutInst.addOperand(MI.getOperand(1));
    return true;
  }
  case LinxV5::C_ADDI: {
    OutInst.setOpcode(RCMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createReg(LinxV5::T));
    OutInst.addOperand(MI.getOperand(0));
    OutInst.addOperand(MI.getOperand(1));
    return true;
  }
  case LinxV5::C_SETC_TGT: {
    OutInst.setOpcode(RCMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MI.getOperand(0));
    return true;
  }
  case LinxV5::SEXT_W: {
    OutInst.setOpcode(LinxV5::ADDIW);
    OutInst.addOperand(MCOperand::createReg(LinxV5::T));
    OutInst.addOperand(MI.getOperand(0));
    OutInst.addOperand(MCOperand::createImm(0));
    return true;
  }
  case LinxV5::C_CMP_EQI:
  case LinxV5::C_CMP_NEI:
  case LinxV5::C_SLLI:
  case LinxV5::C_SRLI: {
    OutInst.setOpcode(RCMap.lookup(MI.getOpcode()));
    OutInst.addOperand(MCOperand::createReg(LinxV5::T));
    OutInst.addOperand(MCOperand::createReg(LinxV5::TOS1));
    OutInst.addOperand(MI.getOperand(0));
    return true;
  }
  }

  return false;
}

} // namespace llvm
