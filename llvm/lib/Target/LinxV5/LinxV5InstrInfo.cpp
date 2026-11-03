//===-- LinxV5InstrInfo.cpp - LinxV5 Instruction Information ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the LinxV5 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "LinxV5InstrInfo.h"
#include "LinxV5.h"
#include "LinxV5MachineFunctionInfo.h"
#include "LinxV5Subtarget.h"
#include "LinxV5TargetMachine.h"
#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "MCTargetDesc/LinxV5MatInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveRangeEdit.h"
#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

#include <queue>
#include <tuple>

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#define GET_INSTRMAP_INFO
#include "LinxV5GenInstrInfo.inc"

#define DEBUG_TYPE "linxv5-instr-info"

static cl::opt<bool> DisableConstCheap(
    "disable-linxv5-const-cheap", cl::Hidden, cl::init(false),
    cl::desc("Do not treate an immediate/symbol as cheap as a move"));

static cl::opt<bool> EnableBFIOpt(
    "linxv5-enable-bfi-opt", cl::Hidden, cl::init(true),
    cl::desc("Optimize symmetric 64-bit const inst sequence with BFI."));

STATISTIC(NumStoreReg, "Number of store regs to stack slots");
STATISTIC(NumTileSpill, "Number of jcore-tile reg spills");

Optional<LinxV5::LocalTileABIInfo>
LinxV5::getLocalTileABIInfo(const Function &F) {
  Attribute Attr = F.getFnAttribute("linx-local-tile-abi");
  if (!Attr.isValid())
    return None;

  if (!F.getAttributes().hasRetAttr(Attribute::InReg))
    report_fatal_error(
        "linx-local-tile-abi requires an explicit inreg return marker");

  SmallVector<StringRef, 3> Fields;
  Attr.getValueAsString().split(Fields, ':');
  if (Fields.size() != 3)
    report_fatal_error("malformed linx-local-tile-abi attribute");

  unsigned ArgumentIndex = 0;
  unsigned PhysicalCols = 0;
  unsigned ValidCols = 0;
  if (Fields[0].getAsInteger(10, ArgumentIndex) ||
      Fields[1].getAsInteger(10, PhysicalCols) ||
      Fields[2].getAsInteger(10, ValidCols))
    report_fatal_error("non-numeric linx-local-tile-abi attribute");

  const bool SupportedProfile =
      (PhysicalCols == 128 && (ValidCols == 128 || ValidCols == 32)) ||
      (PhysicalCols == 2048 && ValidCols == 2048);
  if (!SupportedProfile || ArgumentIndex >= F.arg_size())
    report_fatal_error("unsupported linx-local-tile-abi profile");

  auto *ArgumentTy = dyn_cast<FixedVectorType>(
      F.getFunctionType()->getParamType(ArgumentIndex));
  auto *ReturnTy = dyn_cast<FixedVectorType>(F.getReturnType());
  auto IsExactType = [&](FixedVectorType *Ty) {
    return Ty && Ty->getElementType()->isIntegerTy(32) &&
           Ty->getNumElements() == PhysicalCols;
  };
  if (!IsExactType(ArgumentTy) || !IsExactType(ReturnTy))
    report_fatal_error(
        "linx-local-tile-abi type does not match its U32 NORM profile");

  Optional<unsigned> SizeCode =
      LinxV5SizeCode::sizeCodeForBytes(uint64_t{PhysicalCols} * 4);
  if (!SizeCode)
    report_fatal_error("linx-local-tile-abi capacity is not encodable");

  LocalTileABIInfo Info;
  Info.ArgumentIndex = ArgumentIndex;
  Info.Descriptor = TileDescriptor{
      LinxV5Op::DataType::U32, LinxV5Op::ArgFormat::NORM,
      /*ValidRow=*/1, ValidCols, PhysicalCols, *SizeCode,
      /*PackedPredicate=*/false};
  return Info;
}
STATISTIC(NumTileReload, "Number of jcore-tile reg reloads");

static inline MachineOperand *getAsmOperand(MachineInstr *MI, unsigned Val) {
  unsigned OpNo = InlineAsm::MIOp_FirstOperand;

  // Scan to find the machine operand number for the operand.
  for (; Val; --Val) {
    if (OpNo >= MI->getNumOperands())
      break;
    unsigned OpFlags = MI->getOperand(OpNo).getImm();
    OpNo += InlineAsm::getNumOperandRegisters(OpFlags) + 1;
  }

  // We may have a location metadata attached to the end of the
  // instruction, and at no point should see metadata at any
  // other point while processing. It's an error if so.
  if (OpNo >= MI->getNumOperands() || MI->getOperand(OpNo).isMetadata()) {
    return nullptr;
  } else {
    unsigned OpFlags = MI->getOperand(OpNo).getImm();
    ++OpNo; // Skip over the ID number.
    return &MI->getOperand(OpNo);
  }
}

static inline const char *parseAsmOperand(MachineInstr *MI, const char *p,
                                          MachineOperand *&MO) {
  if (*p != '$')
    return nullptr;
  ++p;
  bool hasBraces = false;
  if (*p == '{') {
    hasBraces = true;
    ++p;
  }
  unsigned NumDigits = 0;
  while (isDigit(p[NumDigits]))
    ++NumDigits;
  unsigned Val = 0;
  if (StringRef(p, NumDigits).getAsInteger(10, Val))
    report_fatal_error("Invalid inline asm operand number!");
  MO = getAsmOperand(MI, Val);

  p += NumDigits;

  if (hasBraces) {
    while (*p++ != '}')
      ;
  }
  return p;
}

// parse '${1}<${2}>'
static inline const char *
parseTileDstWithSize(LinxV5::SingleAsm &SA, MachineInstr *MI, const char *p) {
  MachineOperand *Dst = nullptr;
  p = parseAsmOperand(MI, p, Dst);

  if (!p || !Dst)
    return nullptr;

  // v5: "->$N" without <size> — size is now TSize=%c[N] earlier in the
  // string, not embedded in the dst operand. Record def with size=0
  // (implicit/unknown); callers handle size=0 by falling back to other
  // size sources (e.g. TileDType).
  if (*p != '<') {
    // v5 form: ->dst without <size>
    SA.Defs.push_back(Dst);
    SA.Sizes.push_back(0);
    return p;
  }

  // v4 form: ->dst<size>
  MachineOperand *Size = nullptr;
  p = parseAsmOperand(MI, p + 1, Size);

  if (!p)
    return nullptr;
  if (*p != '>')
    return nullptr;
  SA.Defs.push_back(Dst);
  // The <size> field is expected to bind an immediate TSize code ("i"
  // constraint).  If it resolves to a register (or cannot be resolved) instead
  // of an immediate -- e.g. a TileOP region row-expand whose "->dst<%Z>"
  // selector points at the subview range-base GPR (issue #109) -- do not read
  // it with getImm(): that aborts codegen here, in the TReg-to-Offset pass,
  // long before the AsmPrinter's :Z handler can emit its "0B" sentinel.  Record
  // an unknown size (0) and let downstream size resolution proceed, matching
  // the v5 no-<size> fallback above and the AsmPrinter's parallel :Z guard.
  SA.Sizes.push_back(Size && Size->isImm() ? Size->getImm() : 0);
  return p + 1;
}

static inline const char *skipSeparator(const char *p) {
  while (*p == ',' || *p == ' ' || *p == '\t')
    ++p;
  return p;
}

LinxV5::SingleAsm LinxV5::parseSingleAsm(MachineInstr *MI) {
  LinxV5::SingleAsm SA;
  assert(MI->isInlineAsm() && "parseSingleAsm only works on inline asms");

  // Count the number of register definitions to find the asm string.
  unsigned NumDefs = 0;
  for (; MI->getOperand(NumDefs).isReg() && MI->getOperand(NumDefs).isDef();
       ++NumDefs)
    assert(NumDefs != MI->getNumOperands() - 2 && "No asm string?");

  assert(MI->getOperand(NumDefs).isSymbol() && "No asm string?");

  // Disassemble the AsmStr, printing out the literal pieces, the operands, etc.
  const char *AsmStr = MI->getOperand(NumDefs).getSymbolName();
  const char *p = AsmStr;
  while (*p) {
    if (StringRef(p).startswith("->")) {
      p += 2;
      const char *n;
      do {
        n = parseTileDstWithSize(SA, MI, p);
        if (!n)
          break;
        n = skipSeparator(n);
        p = n;
      } while (true);
    }
    ++p;
  }
  return SA;
}

bool LinxV5::isTileOp(const MachineInstr &MI) {
  if (MI.isInlineAsm()) {
    const MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
    for (auto &MO : MI.operands()) {
      if (!MO.isReg())
        continue;
      Register Reg = MO.getReg();
      if (Reg.isPhysical() && LinxV5::Tile_ABSRegClass.contains(Reg))
        return true;
      if (Reg.isVirtual() &&
          LinxV5::Tile_ABSRegClass.hasSubClassEq(MRI.getRegClass(Reg)))
        return true;
    }
    return false;
  }
  uint64_t TSFlags = MI.getDesc().TSFlags;
  return LinxV5II::isTileOp(TSFlags);
}

Optional<bool> LinxV5::getAssembleControlBit(const MachineInstr &MI,
                                             unsigned OperandIndex) {
  if (!MI.isInlineAsm())
    return None;
  StringRef Asm(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName());
  size_t Pos = Asm.find("B.ASSEMBLE");
  if (Pos == StringRef::npos)
    return None;
  Pos += strlen("B.ASSEMBLE");
  unsigned ConstraintIdx = 0;
  for (unsigned Seen = 0; Seen <= OperandIndex; ++Seen) {
    size_t Dollar = Asm.find_first_of("$%", Pos);
    if (Dollar == StringRef::npos)
      return None;
    Pos = Dollar;
    StringRef Digits;
    if (Asm[Pos] == '$') {
      ++Pos;
      if (Pos < Asm.size() && Asm[Pos] == '{') {
        size_t Close = Asm.find('}', Pos);
        if (Close == StringRef::npos)
          return None;
        StringRef Body = Asm.substr(Pos + 1, Close - Pos - 1);
        size_t Colon = Body.find(':');
        if (Colon != StringRef::npos)
          Body = Body.substr(0, Colon);
        Digits = Body;
        Pos = Close + 1;
      } else {
        size_t End = Asm.find_first_not_of("0123456789", Pos);
        Digits = Asm.slice(Pos, End == StringRef::npos ? Asm.size() : End);
        Pos = End == StringRef::npos ? Asm.size() : End;
      }
    } else {
      ++Pos;
      if (Pos < Asm.size() && Asm[Pos] == 'c')
        ++Pos;
      if (Pos < Asm.size() && Asm[Pos] == '[')
        return None;
      size_t End = Asm.find_first_not_of("0123456789", Pos);
      Digits = Asm.slice(Pos, End == StringRef::npos ? Asm.size() : End);
      Pos = End == StringRef::npos ? Asm.size() : End;
    }
    if (Digits.empty() || Digits.getAsInteger(10, ConstraintIdx))
      return None;
  }
  unsigned Group = 0;
  for (unsigned I = InlineAsm::MIOp_FirstOperand, E = MI.getNumOperands();
       I < E; ++Group) {
    const MachineOperand &Flag = MI.getOperand(I);
    if (!Flag.isImm())
      return None;
    unsigned N = InlineAsm::getNumOperandRegisters(Flag.getImm());
    if (Group == ConstraintIdx) {
      if (N != 1 || I + 1 >= E || !MI.getOperand(I + 1).isImm())
        return None;
      return MI.getOperand(I + 1).getImm() != 0;
    }
    I += 1 + N;
  }
  return None;
}

static bool isLocalTileReg(const MachineRegisterInfo &MRI, Register Reg) {
  if (Reg.isPhysical())
    return LinxV5::Tile_ABSRegClass.contains(Reg);
  return LinxV5::Tile_ABSRegClass.hasSubClassEq(MRI.getRegClass(Reg));
}

Register LinxV5::getAssembleParentRef(const MachineInstr &MI) {
  if (!MI.isInlineAsm() ||
      !StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
           .contains("B.ASSEMBLE"))
    return LinxV5::NoRegister;

  // INIT publishes a destination.  Only a destination-free continuation has
  // a ParentRef.  The modifier attaches to the immediately preceding B.IOT
  // or B.IOS source-form binder, whose final Local tile operand is ParentRef;
  // earlier operands in the same binder are numeric writer fragments.
  if (llvm::any_of(MI.operands(), [](const MachineOperand &MO) {
        return MO.isReg() && MO.isDef() && !MO.isImplicit();
      }))
    return LinxV5::NoRegister;

  StringRef Asm = MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  size_t AssemblePos = Asm.find("B.ASSEMBLE");
  StringRef BeforeAssemble = Asm.take_front(AssemblePos);
  size_t IOTPos = BeforeAssemble.rfind("B.IOT");
  size_t IOSPos = BeforeAssemble.rfind("B.IOS");
  size_t BinderPos = IOTPos;
  if (BinderPos == StringRef::npos ||
      (IOSPos != StringRef::npos && IOSPos > BinderPos))
    BinderPos = IOSPos;
  if (BinderPos == StringRef::npos)
    return LinxV5::NoRegister;

  size_t BinderEnd = Asm.find('\n', BinderPos);
  if (BinderEnd == StringRef::npos || BinderEnd > AssemblePos)
    BinderEnd = AssemblePos;

  const MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
  Register ParentRef = LinxV5::NoRegister;
  for (size_t Pos = Asm.find('$', BinderPos);
       Pos != StringRef::npos && Pos < BinderEnd;
       Pos = Asm.find('$', Pos + 1)) {
    size_t DigitPos = Pos + 1;
    if (DigitPos < BinderEnd && Asm[DigitPos] == '{')
      ++DigitPos;
    size_t DigitEnd = DigitPos;
    while (DigitEnd < BinderEnd && isDigit(Asm[DigitEnd]))
      ++DigitEnd;
    if (DigitEnd == DigitPos)
      continue;

    unsigned ConstraintIdx = 0;
    if (Asm.slice(DigitPos, DigitEnd).getAsInteger(10, ConstraintIdx))
      continue;
    unsigned Group = 0;
    const MachineOperand *MO = nullptr;
    for (unsigned I = InlineAsm::MIOp_FirstOperand, E = MI.getNumOperands();
         I < E; ++Group) {
      const MachineOperand &Flag = MI.getOperand(I);
      if (!Flag.isImm())
        break;
      unsigned N = InlineAsm::getNumOperandRegisters(Flag.getImm());
      if (Group == ConstraintIdx) {
        if (N == 1 && I + 1 < E)
          MO = &MI.getOperand(I + 1);
        break;
      }
      I += 1 + N;
    }
    if (!MO || !MO->isReg() || !MO->isUse() || MO->isImplicit())
      continue;
    Register Reg = MO->getReg();
    if (isLocalTileReg(MRI, Reg))
      ParentRef = Reg;
  }
  return ParentRef;
}

static bool isSharedTileReg(const MachineRegisterInfo &MRI, Register Reg) {
  if (Reg.isPhysical())
    return LinxV5::Shared_ABSRegClass.contains(Reg);
  return LinxV5::Shared_ABSRegClass.hasSubClassEq(MRI.getRegClass(Reg));
}

unsigned LinxV5::getTileOpRegSize(MachineInstr &MI, Register Reg) {
  if (MI.getOpcode() == LinxV5::PseudoV5GMOV)
    return MI.getOperand(3).getImm();
  if (MI.getOpcode() == LinxV5::PseudoV5SharedS2L)
    return MI.getOperand(5).getImm();
  if (MI.getOpcode() == LinxV5::PseudoTCOPY ||
      (MI.getOpcode() == LinxV5::PseudoTCOPYDesc ||
       MI.getOpcode() == LinxV5::PseudoPCOPYDesc)) {
    return MI.getOperand(1).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTMOV_SizeI) {
    return MI.getOperand(8).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTADD_Masked_SizeI) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTSUB_Masked_SizeI) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTBinary_SizeI) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTBinary_GPR_Masked_SizeI) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTNEG_GPR_Masked_SizeI) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTCVT_GPR_Masked_SizeI) {
    return MI.getOperand(8).getImm();
  } else if ((MI.getOpcode() == LinxV5::PseudoTCMP_SizeI ||
              MI.getOpcode() == LinxV5::PseudoTCMP_Row_SizeI)) {
    return MI.getOperand(7).getImm();
  } else if ((MI.getOpcode() == LinxV5::PseudoTSEL_SizeI ||
              MI.getOpcode() == LinxV5::PseudoTSEL_Row_SizeI)) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTSEL_GPR_SizeI) {
    return MI.getOperand(7).getImm();
  } else if ((MI.getOpcode() == LinxV5::PseudoTEXPANDS_SizeI ||
              MI.getOpcode() == LinxV5::PseudoTEXPANDS_Row_SizeI)) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoTLEA_SizeI) {
    return MI.getOperand(7).getImm();
  } else if ((MI.getOpcode() == LinxV5::PseudoTCI_EW_SizeI ||
              MI.getOpcode() == LinxV5::PseudoTCI_EW_Row_SizeI)) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMGATHER_MASK_SizeI ||
             MI.getOpcode() == LinxV5::PseudoMGATHER_MASK_SizeI_Range) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMGATHER_GPR_Masked_SizeI ||
             MI.getOpcode() == LinxV5::PseudoMGATHER_GPR_Masked_SizeI_Range) {
    return MI.getOperand(8).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMSCATTER_MASK_SizeI ||
             MI.getOpcode() == LinxV5::PseudoMSCATTER_MASK_SizeI_Range) {
    return MI.getOperand(6).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMSCATTER_GPR_Masked_SizeI ||
             MI.getOpcode() == LinxV5::PseudoMSCATTER_GPR_Masked_SizeI_Range) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMGATHER_ADD_SizeI ||
             MI.getOpcode() == LinxV5::PseudoMGATHER_ADD_SizeI_Range) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMGATHER_ADD_Masked_SizeI ||
             MI.getOpcode() == LinxV5::PseudoMGATHER_ADD_Masked_SizeI_Range) {
    return MI.getOperand(8).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMSCATTER_ADD_SizeI ||
             MI.getOpcode() == LinxV5::PseudoMSCATTER_ADD_SizeI_Range) {
    return MI.getOperand(6).getImm();
  } else if (MI.isInlineAsm()) {
    LinxV5::SingleAsm SA = parseSingleAsm(&MI);
    for (int i = 0; i < SA.Defs.size(); ++i) {
      if (SA.Defs[i]->getReg() == Reg)
        return SA.Sizes[i];
    }
    report_fatal_error("Can not find tile definition in inline asm");
  } else {
    // FIXME: this is a magic num
    unsigned di = 0;
    for (; di < MI.getNumExplicitDefs(); ++di) {
      if (MI.getOperand(di).getReg() == Reg)
        break;
    }
    if (di == MI.getNumExplicitDefs())
      report_fatal_error("Can not find tile definition in machine instruction");
    unsigned Idx = MI.getNumExplicitDefs() + 7 + di;
    return MI.getOperand(Idx).getImm();
  }
}

bool LinxV5::isIsolateInstr(MachineInstr &MI) {
  uint64_t TSFlags = MI.getDesc().TSFlags;
  const LinxV5Subtarget &STI =
      MI.getParent()->getParent()->getSubtarget<LinxV5Subtarget>();
  return LinxV5II::isTileOp(TSFlags) || LinxV5II::isHeaderOnly(TSFlags) ||
         (MI.isInlineAsm() && !STI.isSIMT());
}

bool LinxV5::isRAInstrOfInlineASMBlock(MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case TargetOpcode::COPY:
  case LinxV5::LDI:
  case LinxV5::SDI:
  case LinxV5::ADDI:
  case LinxV5::SUBI:
  case LinxV5::ORI:
  case LinxV5::XORI:
  case LinxV5::LUI:
    return true;
  default:
    return false;
  }
}

bool LinxV5::isPhysScratchRegAvailable(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator At,
                                       Register Reg) {
  for (auto &MI : make_range(At, MBB.end())) {
    // meet use first, busy if not undef.
    if (auto *MO = MI.findRegisterUseOperand(Reg)) {
      if (MO->isUndef())
        continue;
      return false;
    }

    // meet def first, idle.
    if (MI.findRegisterDefOperand(Reg))
      return true;
  }
  // didn't meet any use or def, idle.
  return true;
}

unsigned LinxV5::getSizeFromSIMTType(unsigned DstTypeEnum) {
  switch (DstTypeEnum & 3) {
  case 0:
    return SIMTRegSize::SIMT_REG_SIZE_D; // D type
  case 1:
    return SIMTRegSize::SIMT_REG_SIZE_W; // W type
  case 2:
    return SIMTRegSize::SIMT_REG_SIZE_H; // H type
  case 3:
    return SIMTRegSize::SIMT_REG_SIZE_B; // B type
  default:
    report_fatal_error("unexpected SIMT Reg Type!");
  }
}

unsigned LinxV5::getSIMTDstTypeFromBits(unsigned Bits) {
  switch (Bits) {
  case 64:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_D; // D type
  case 32:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_W; // W type
  case 16:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_H; // H type
  case 8:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_B; // B type
  default:
    report_fatal_error("unexpected reg bits!");
  }
}

unsigned LinxV5::getSIMTDstTypeFromSize(unsigned RegSize) {
  switch (RegSize) {
  case SIMTRegSize::SIMT_REG_SIZE_D:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_D; // D type
  case SIMTRegSize::SIMT_REG_SIZE_W:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_W; // W type
  case SIMTRegSize::SIMT_REG_SIZE_H:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_H; // H type
  case SIMTRegSize::SIMT_REG_SIZE_B:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_B; // B type
  default:
    report_fatal_error("unexpected reg size!");
  }
}

unsigned LinxV5::getSIMTSrcTypeFromSize(unsigned RegSize) {
  switch (RegSize) {
  case SIMTRegSize::SIMT_REG_SIZE_D:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD; // D type
  case SIMTRegSize::SIMT_REG_SIZE_W:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UW; // W type
  case SIMTRegSize::SIMT_REG_SIZE_H:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UH; // H type
  case SIMTRegSize::SIMT_REG_SIZE_B:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UB; // B type
  default:
    report_fatal_error("unexpected reg size!");
  }
}

unsigned LinxV5::getUseRegSizeAtSingleBlock(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MBBI,
                                            MCRegister Reg, bool &FindDef) {
  for (MachineInstr &MI : make_range(MBBI, MBB.end())) {
    if (MI.readsRegister(Reg)) {
      if (MI.getOpcode() == TargetOpcode::COPY) {
        Register DstReg = MI.getOperand(0).getReg();
        return LinxV5::getUseRegSize(MBB, MI.getIterator(), DstReg);
      }
      int UseIdx = MI.findRegisterUseOperandIdx(Reg);
      if (!MI.getOperand(UseIdx + 1).isImm())
        return SIMTRegSize::SIMT_REG_SIZE_W;
      unsigned UseRegTypeEnum = MI.getOperand(UseIdx + 1).getImm();
      return LinxV5::getSizeFromSIMTType(UseRegTypeEnum);
    }
    if (MI.definesRegister(Reg)) {
      FindDef = true;
      return 0;
    }
  }
  return 0;
}

// Return 0 if no UseReg found (redefined before use)
unsigned LinxV5::getUseRegSize(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MBBI,
                               MCRegister Reg) {
  bool FindDef = false;
  unsigned RegSize =
      getUseRegSizeAtSingleBlock(MBB, std::next(MBBI), Reg, FindDef);
  if (FindDef)
    return 0;

  if (RegSize)
    return RegSize;

  std::queue<MachineBasicBlock *> Queue;
  SmallDenseSet<MachineBasicBlock *> Candidates; // for loop
  Candidates.insert(&MBB);
  for (MachineBasicBlock *Succ : MBB.successors()) {
    if (!Candidates.count(Succ) || Succ == &MBB) {
      Queue.push(Succ);
      Candidates.insert(Succ);
    }
  }

  while (!Queue.empty()) {
    MachineBasicBlock *Top = Queue.front();
    Queue.pop();
    FindDef = false;
    RegSize = getUseRegSizeAtSingleBlock(*Top, Top->begin(), Reg, FindDef);
    if (RegSize)
      return RegSize;
    if (FindDef)
      continue;
    for (MachineBasicBlock *Succ : Top->successors()) {
      if (!Candidates.count(Succ)) {
        Queue.push(Succ);
        Candidates.insert(Succ);
      }
    }
  }

  return 0;
}

static unsigned getTileRegSizeImpl(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   MCRegister Reg, bool isSpill);

static unsigned getTileRegSizeAtSingleBlock(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MBBI,
                                            MCRegister Reg, bool isSpill) {
  for (MachineInstr &MI : reverse(make_range(MBB.begin(), MBBI))) {
    for (int i = 0; i < MI.getNumOperands(); ++i) {
      auto &MO = MI.getOperand(i);
      if (!MO.isReg() || !MO.isDef())
        continue;
      if (MO.getReg() != Reg)
        continue;
      if (MI.getOpcode() == TargetOpcode::COPY) {
        Register SrcReg = MI.getOperand(1).getReg();
        return getTileRegSizeImpl(MBB, MI.getIterator(), SrcReg, isSpill);
      }
      if (MI.getOpcode() == LinxV5::PseudoTReload && isSpill)
        return 0;
      if (MI.getOpcode() == LinxV5::IMPLICIT_DEF)
        return 0;
      unsigned RegSizeMask = LinxV5::getTileOpRegSize(MI, Reg);
      return RegSizeMask + 1;
    }
  }
  return 0;
}

static unsigned getTileRegSizeImpl(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   MCRegister Reg, bool isSpill) {
  unsigned RegSize =
      getTileRegSizeAtSingleBlock(MBB, std::next(MBBI), Reg, isSpill);
  if (RegSize)
    return RegSize;

  std::queue<MachineBasicBlock *> Queue;
  SmallDenseSet<MachineBasicBlock *> Candidates; // for loop
  Candidates.insert(&MBB);
  for (MachineBasicBlock *Pred : MBB.predecessors()) {
    if (!Candidates.count(Pred)) {
      Queue.push(Pred);
      Candidates.insert(Pred);
    }
  }

  while (!Queue.empty()) {
    MachineBasicBlock *Top = Queue.front();
    Queue.pop();
    RegSize = getTileRegSizeAtSingleBlock(*Top, Top->end(), Reg, isSpill);
    if (RegSize)
      return RegSize;
    for (MachineBasicBlock *Pred : Top->predecessors()) {
      if (!Candidates.count(Pred)) {
        Queue.push(Pred);
        Candidates.insert(Pred);
      }
    }
  }
  // No error is reported here, and `getTileRegSize` returns -1(0 - 1) to allow
  // the frame index correction to be determined.
  if (isSpill)
    return 0;

  report_fatal_error("Can not find Copy Use at SIMT Mode!");
}

unsigned LinxV5::getTileRegSize(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI,
                                MCRegister Reg, bool isSpill) {
  return getTileRegSizeImpl(MBB, MBBI, Reg, isSpill) - 1;
}

unsigned LinxV5::getTileDataTypeBits(unsigned DataType) {
  using namespace LinxV5Op;
  switch (DataType) {
  case DataType::FP64:
  case DataType::S64:
  case DataType::U64:
    return 64;
  case DataType::FP32:
  case DataType::TF32:
  case DataType::HF32:
  case DataType::S32:
  case DataType::U32:
    return 32;
  case DataType::FP16:
  case DataType::BF16:
  case DataType::S16:
  case DataType::U16:
    return 16;
  case DataType::HiF8:
  case DataType::e4m3:
  case DataType::e5m2:
  case DataType::e3m2:
  case DataType::e2m3:
  case DataType::e8m0:
  case DataType::e6m2:
  case DataType::rcpe6m2:
  case DataType::S8:
  case DataType::U8:
    return 8;
  case DataType::e2m1x2:
  case DataType::e1m2x2:
  case DataType::HiF4x2:
  case DataType::S4x2:
  case DataType::U4x2:
    return 4;
  default:
    return 0;
  }
}

static Optional<unsigned> getInlineAsmOperandImmediate(MachineInstr &MI,
                                                       StringRef Text,
                                                       size_t Before) {
  size_t Start = Text.take_front(std::min(Before + 1, Text.size())).rfind("${");
  if (Start == StringRef::npos)
    return None;
  size_t Digit = Start + 2;
  size_t End = Digit;
  while (End < Text.size() && isDigit(Text[End]))
    ++End;
  unsigned Index = 0;
  if (End == Digit || Text.slice(Digit, End).getAsInteger(10, Index))
    return None;
  MachineOperand *MO = getAsmOperand(&MI, Index);
  if (!MO || !MO->isImm())
    return None;
  return static_cast<unsigned>(MO->getImm());
}

struct InlineAsmDimension {
  Register Reg = LinxV5::R0;
  int64_t Imm = 1;
};

static Optional<InlineAsmDimension>
getInlineAsmDimensionOperand(MachineInstr &MI, StringRef Text,
                             unsigned Dimension) {
  std::string Suffix = (Twine("->lb") + Twine(Dimension)).str();
  size_t End = Text.find(Suffix);
  if (End == StringRef::npos)
    return InlineAsmDimension{};
  size_t Start = Text.take_front(std::min(End + 1, Text.size())).rfind("B.DIM");
  if (Start == StringRef::npos)
    return None;
  StringRef Line = Text.slice(Start + strlen("B.DIM"), End).trim();
  auto Fields = Line.split(',');
  StringRef Base = Fields.first.trim();
  StringRef Offset = Fields.second.rtrim(" \t,").ltrim();
  if (Base == "zero") {
    size_t Zero = Line.find("zero,");
    StringRef Value = Line.drop_front(Zero + strlen("zero,")).trim();
    if (Value.startswith("${")) {
      Optional<unsigned> Imm = getInlineAsmOperandImmediate(
          MI, Text, Start + strlen("B.DIM") + Zero + strlen("zero,") +
                        Value.find(':'));
      if (!Imm)
        return None;
      return InlineAsmDimension{LinxV5::R0, *Imm};
    }
    size_t Digits = 0;
    while (Digits < Value.size() && isDigit(Value[Digits]))
      ++Digits;
    unsigned Imm = 0;
    if (!Digits || Value.take_front(Digits).getAsInteger(10, Imm))
      return None;
    return InlineAsmDimension{LinxV5::R0, Imm};
  }

  if (!Base.consume_front("$"))
    return None;
  unsigned Index = 0;
  if (Base.getAsInteger(10, Index))
    return None;
  MachineOperand *MO = getAsmOperand(&MI, Index);
  if (!MO || !MO->isReg())
    return None;
  int64_t Imm = 0;
  if (Offset.startswith("${")) {
    size_t Marker = Offset.find(':');
    Optional<unsigned> Value = getInlineAsmOperandImmediate(
        MI, Text, Offset.data() - Text.data() + Marker);
    if (!Value)
      return None;
    Imm = *Value;
  } else if (Offset.getAsInteger(10, Imm)) {
    return None;
  }
  return InlineAsmDimension{MO->getReg(), Imm};
}

static Optional<unsigned> getStaticTileDimensionRegister(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Before, Register Reg) {
  SmallPtrSet<MachineBasicBlock *, 8> Visited;
  MachineBasicBlock *Block = &MBB;
  while (Visited.insert(Block).second) {
    if (Reg == LinxV5::R0)
      return 0u;
    for (MachineInstr &Def : reverse(make_range(Block->begin(), Before))) {
      if (!Def.definesRegister(Reg))
        continue;
      if (Def.isCopy()) {
        Reg = Def.getOperand(1).getReg();
        if (Reg == LinxV5::R0)
          return 0u;
        continue;
      }
      if (Def.getOpcode() == LinxV5::ADDI &&
          Def.getOperand(1).getReg() == LinxV5::R0 &&
          Def.getOperand(2).getImm() >= 0 &&
          Def.getOperand(2).getImm() <= 65535)
        return static_cast<unsigned>(Def.getOperand(2).getImm());
      return None;
    }
    if (Block->pred_size() != 1)
      return None;
    Block = *Block->pred_begin();
    Before = Block->end();
  }
  return None;
}

static Optional<unsigned> getInlineAsmDimension(MachineInstr &MI,
                                                StringRef Text,
                                                unsigned Dimension) {
  std::string Suffix = (Twine("->lb") + Twine(Dimension)).str();
  size_t End = Text.find(Suffix);
  if (End == StringRef::npos)
    return 1u; // Omitted dimensions have architectural value one.
  size_t Start = Text.take_front(std::min(End + 1, Text.size())).rfind("B.DIM");
  if (Start == StringRef::npos)
    return None;
  StringRef Line = Text.slice(Start, End);
  size_t Zero = Line.find("zero,");
  if (Zero == StringRef::npos) {
    StringRef RegisterText = Line.drop_front(strlen("B.DIM")).trim();
    MachineOperand *Operand = nullptr;
    const char *After = parseAsmOperand(&MI, RegisterText.data(), Operand);
    if (!After || !Operand || !Operand->isReg())
      return None;
    StringRef Immediate = Text.slice(After - Text.data(), End)
                              .split(',').second.split(',').first.trim();
    unsigned Offset;
    if (Immediate.getAsInteger(10, Offset) || Offset != 0)
      return None;
    return getStaticTileDimensionRegister(*MI.getParent(), MI.getIterator(),
                                          Operand->getReg());
  }
  StringRef Value = Line.drop_front(Zero + strlen("zero,")).trim();
  if (Value.startswith("${"))
    return getInlineAsmOperandImmediate(MI, Text, Start + Zero +
                                                      strlen("zero,") +
                                                      Value.find(':'));
  size_t Digits = 0;
  while (Digits < Value.size() && isDigit(Value[Digits]))
    ++Digits;
  unsigned Result = 0;
  if (!Digits || Value.take_front(Digits).getAsInteger(10, Result))
    return None;
  return Result;
}

using DimensionValue = std::pair<Register, const MachineInstr *>;
using DimensionPosition =
    std::tuple<MachineBasicBlock *, const MachineInstr *, Register>;

static Optional<DimensionValue> resolveDimensionValue(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Before, Register Reg,
    SmallVectorImpl<DimensionPosition> &Active, bool *Cycle = nullptr) {
  if (Cycle)
    *Cycle = false;
  DimensionPosition Position{&MBB, Before == MBB.end() ? nullptr : &*Before, Reg};
  if (is_contained(Active, Position)) {
    if (Cycle)
      *Cycle = true;
    return None;
  }
  Active.push_back(Position);
  for (MachineInstr &Definition : reverse(make_range(MBB.begin(), Before))) {
    if (!Definition.modifiesRegister(
            Reg, MBB.getParent()->getSubtarget().getRegisterInfo()))
      continue;
    bool IsCopy = Definition.isCopy() ||
                  (Definition.getOpcode() == LinxV5::ORI &&
                   Definition.getOperand(2).getImm() == 0);
    Optional<DimensionValue> Result;
    if (IsCopy) {
      Result = resolveDimensionValue(MBB, Definition.getIterator(),
                                    Definition.getOperand(1).getReg(), Active,
                                    Cycle);
    } else if (!Definition.isCall() && Definition.definesRegister(Reg)) {
      Result = DimensionValue{Reg, &Definition};
    }
    Active.pop_back();
    return Result;
  }
  Optional<DimensionValue> Joined;
  if (MBB.pred_empty())
    Joined = DimensionValue{Reg, nullptr};
  for (MachineBasicBlock *Predecessor : MBB.predecessors()) {
    bool IncomingCycle = false;
    auto Incoming = resolveDimensionValue(*Predecessor, Predecessor->end(), Reg,
                                         Active, &IncomingCycle);
    if (!Incoming && IncomingCycle)
      continue;
    if (!Incoming) {
      Active.pop_back();
      return None;
    }
    if (Joined && *Joined != *Incoming) {
      Active.pop_back();
      return None;
    }
    Joined = Incoming;
  }
  Active.pop_back();
  if (!Joined && Cycle)
    *Cycle = true;
  return Joined;
}

static Optional<DimensionValue> resolveDimensionValue(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Before, Register Reg) {
  SmallVector<DimensionPosition, 8> Active;
  return resolveDimensionValue(MBB, Before, Reg, Active);
}

static bool getDynamicInlineAsmDimension(
    MachineInstr &MI, StringRef Text, unsigned Dimension, Register &Reg,
    const MachineInstr *&Definition) {
  size_t End = Text.find((Twine("->lb") + Twine(Dimension)).str());
  if (End == StringRef::npos)
    return false;
  size_t Start = Text.take_front(End).rfind("B.DIM");
  if (Start == StringRef::npos)
    return false;
  StringRef Binding = Text.slice(Start + strlen("B.DIM"), End).trim();
  MachineOperand *Operand = nullptr;
  const char *After = parseAsmOperand(&MI, Binding.data(), Operand);
  if (!After || !Operand || !Operand->isReg())
    return false;
  unsigned Offset;
  if (Text.slice(After - Text.data(), End).split(',').second.split(',')
          .first.trim().getAsInteger(10, Offset) || Offset != 0)
    return false;
  auto Value = resolveDimensionValue(*MI.getParent(), MI.getIterator(),
                                     Operand->getReg());
  if (!Value)
    return false;
  Reg = Value->first;
  Definition = Value->second;
  return true;
}

void LinxV5::addTileCopyDimensions(MachineInstrBuilder &Copy,
                                  const TileDescriptor &Descriptor) {
  MachineInstr &Instruction = *Copy;
  auto AddDimension = [&](unsigned Immediate, Register Reg,
                          const MachineInstr *Definition) {
    if (!Reg) {
      Copy.addImm(Immediate);
      return;
    }
    auto Value = resolveDimensionValue(*Instruction.getParent(),
                                      Instruction.getIterator(), Reg);
    if (!Value || *Value != DimensionValue(Reg, Definition))
      report_fatal_error("Local tile COPY dimension value is no longer available");
    Copy.addReg(Reg);
  };
  AddDimension(Descriptor.ValidCol, Descriptor.ValidColReg,
               Descriptor.ValidColDef);
  AddDimension(Descriptor.ValidRow, Descriptor.ValidRowReg,
               Descriptor.ValidRowDef);
  AddDimension(Descriptor.PhysicalCol, Descriptor.PhysicalColReg,
               Descriptor.PhysicalColDef);
}

static bool hasInlineAsmDimension(StringRef Text, unsigned Dimension) {
  return Text.contains((Twine("->lb") + Twine(Dimension)).str());
}

static Optional<unsigned> getInlineAsmDataType(MachineInstr &MI,
                                               StringRef Text);

static Optional<unsigned> getInlineAsmLayout(MachineInstr &MI,
                                             StringRef Text) {
  struct ConditionalLayout {
    const char *Needle;
    unsigned Result;
  } Cases[] = {{"== 29", LinxV5Op::ArgFormat::CUBE_M32},
               {"== 31", LinxV5Op::ArgFormat::CUBE_M16},
               {"== 21", LinxV5Op::ArgFormat::CUBE_M32},
               {"== 22", LinxV5Op::ArgFormat::CUBE_M16}};
  bool HasConditionalLayout = false;
  Optional<unsigned> ConditionalActual;
  for (const ConditionalLayout &Case : Cases) {
    size_t Pos = Text.find(Case.Needle);
    if (Pos == StringRef::npos)
      continue;
    HasConditionalLayout = true;
    Optional<unsigned> Actual = getInlineAsmOperandImmediate(MI, Text, Pos);
    if (!Actual)
      return None;
    if (ConditionalActual && *ConditionalActual != *Actual)
      return None;
    ConditionalActual = *Actual;
    unsigned Expected = 0;
    StringRef(Case.Needle).drop_front(3).getAsInteger(10, Expected);
    if (Actual && *Actual == Expected)
      return Case.Result;
  }
  if (HasConditionalLayout && ConditionalActual &&
      *ConditionalActual == LinxV5Op::ArgFormat::ND2M16 &&
      Text.contains("B.DATR ND2M16"))
    return LinxV5Op::ArgFormat::CUBE_M16;
  if (HasConditionalLayout && ConditionalActual &&
      *ConditionalActual == LinxV5Op::ArgFormat::ND2N8 &&
      Text.contains("B.DATR ND2N8"))
    return LinxV5Op::ArgFormat::ND2N8;
  // Do not select text from an inactive conditional branch.  The complete
  // layout must be statically resolved before a typed COPY can be emitted.
  if (HasConditionalLayout)
    return LinxV5Op::ArgFormat::NORM;
  if (Text.contains("B.DATR CUBE_M32") || Text.contains("B.DATR ND2M32"))
    return LinxV5Op::ArgFormat::CUBE_M32;
  if (Text.contains("B.DATR CUBE_M16") || Text.contains("B.DATR ND2M16"))
    return LinxV5Op::ArgFormat::CUBE_M16;
  if (Text.contains("B.DATR NORM"))
    return LinxV5Op::ArgFormat::NORM;
  if (!Text.contains("B.DATR"))
    return LinxV5Op::ArgFormat::NORM;

  bool DefaultLayout = true;
  SmallVector<StringRef, 8> AttributeLines;
  Text.split(AttributeLines, '\n');
  for (StringRef Line : AttributeLines) {
    Line = Line.trim();
    if (!Line.consume_front("B.DATR"))
      continue;
    SmallVector<StringRef, 4> Fields;
    Line.trim().split(Fields, ',');
    StringRef Pad = Fields.front().trim();
    if (Pad != "Zero" && Pad != "Max" && Pad != "Min" && Pad != "Null") {
      DefaultLayout = false;
      break;
    }
    if (Fields.size() != 1 && Fields.size() != 2 && Fields.size() != 4) {
      DefaultLayout = false;
      break;
    }
    if (Fields.size() >= 2) {
      StringRef Mode = Fields[1].trim();
      if (Mode != "EQ" && Mode != "NE" && Mode != "LT" && Mode != "LE" &&
          Mode != "GT" && Mode != "GE") {
        DefaultLayout = false;
        break;
      }
    }
    if (Fields.size() == 4 &&
        (Fields[2].trim() != "RNONE" || Fields[3].trim() != "sat")) {
      DefaultLayout = false;
      break;
    }
  }
  if (DefaultLayout)
    return LinxV5Op::ArgFormat::NORM;

  // Identity conversions may spell only dtype and rounding in B.DATR.
  // Every possible attribute line then has the architectural default NORM
  // layout. Require exact two-field spellings and the same resolved dtype as
  // the header; non-identity conversion result typing needs separate proof.
  size_t BStart = Text.find("BSTART.");
  if (BStart == StringRef::npos)
    return None;
  StringRef Header = Text.drop_front(BStart).split('\n').first;
  Optional<unsigned> HeaderType = getInlineAsmDataType(MI, Header);
  if (!HeaderType || *HeaderType == LinxV5Op::DataType::EMPTY_DataType)
    return None;
  SmallVector<StringRef, 8> Lines;
  Text.split(Lines, '\n');
  bool SawAttribute = false;
  for (StringRef Line : Lines) {
    Line = Line.trim();
    if (!Line.consume_front("B.DATR"))
      continue;
    Line = Line.trim();
    auto Fields = Line.split(',');
    StringRef Type = Fields.first.trim();
    StringRef Rounding = Fields.second.trim();
    if (!Type.startswith("${") || !Type.endswith(":D}") ||
        (Rounding != "RNONE" && Rounding != "RNE" && Rounding != "RTZ" &&
         Rounding != "RTM" && Rounding != "RTP" && Rounding != "RNA" &&
         Rounding != "RTO" && Rounding != "RHB"))
      return None;
    size_t Marker = Type.data() - Text.data() + Type.find(":D}");
    Optional<unsigned> ResultType =
        getInlineAsmOperandImmediate(MI, Text, Marker);
    if (!ResultType || *ResultType != *HeaderType)
      return None;
    SawAttribute = true;
  }
  return SawAttribute ? Optional<unsigned>(LinxV5Op::ArgFormat::NORM) : None;
}

static Optional<unsigned> getInlineAsmDataType(MachineInstr &MI,
                                               StringRef Text) {
  size_t Marker = Text.find(":D}");
  if (Marker != StringRef::npos)
    return getInlineAsmOperandImmediate(MI, Text, Marker);

  size_t BStart = Text.find("BSTART.");
  if (BStart == StringRef::npos)
    return None;
  StringRef Header = Text.drop_front(BStart).split('\n').first;
  size_t Comma = Header.find(',');
  StringRef Token;
  if (Header.startswith("BSTART.TEPL ") ||
      Header.startswith("BSTART.TLSU ") ||
      Header.startswith("BSTART.SFU ") ||
      Header.startswith("BSTART.CUBE ")) {
    if (Comma == StringRef::npos)
      return None;
    Token = Header.drop_front(Comma + 1).ltrim();
  } else {
    size_t Space = Header.find_first_of(" \t");
    if (Space == StringRef::npos)
      return None;
    Token = Header.drop_front(Space + 1).ltrim();
