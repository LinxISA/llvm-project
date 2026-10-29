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
  if (MI.getOpcode() == LinxV5::PseudoTCOPY ||
      (MI.getOpcode() == LinxV5::PseudoTCOPYDesc ||
       MI.getOpcode() == LinxV5::PseudoPCOPYDesc)) {
    return MI.getOperand(1).getImm();
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
  } else if (MI.getOpcode() == LinxV5::PseudoMGATHER_MASK_SizeI) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMGATHER_GPR_Masked_SizeI) {
    return MI.getOperand(8).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMSCATTER_MASK_SizeI) {
    return MI.getOperand(6).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMSCATTER_GPR_Masked_SizeI) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMGATHER_ADD_SizeI) {
    return MI.getOperand(7).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMGATHER_ADD_Masked_SizeI) {
    return MI.getOperand(8).getImm();
  } else if (MI.getOpcode() == LinxV5::PseudoMSCATTER_ADD_SizeI) {
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

static unsigned getTileDataTypeBits(unsigned DataType) {
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
  size_t Comma = Text.find(',', BStart);
  if (Comma == StringRef::npos)
    return None;
  StringRef Token = Text.drop_front(Comma + 1).ltrim();
  Token = Token.take_while(
      [](char C) { return isAlnum(C) || C == '_'; });
  if (Token.empty())
    return None;

  unsigned Numeric = 0;
  if (!Token.getAsInteger(10, Numeric))
    return Numeric <= LinxV5Op::DataType::EMPTY_DataType
               ? Optional<unsigned>(Numeric)
               : None;

  return StringSwitch<Optional<unsigned>>(Token.upper())
      .Case("FP64", LinxV5Op::DataType::FP64)
      .Case("FP32", LinxV5Op::DataType::FP32)
      .Case("TF32", LinxV5Op::DataType::TF32)
      .Case("HF32", LinxV5Op::DataType::HF32)
      .Case("FP16", LinxV5Op::DataType::FP16)
      .Case("BF16", LinxV5Op::DataType::BF16)
      .Case("HIF8", LinxV5Op::DataType::HiF8)
      .Case("E4M3", LinxV5Op::DataType::e4m3)
      .Case("E5M2", LinxV5Op::DataType::e5m2)
      .Case("E3M2", LinxV5Op::DataType::e3m2)
      .Case("E2M3", LinxV5Op::DataType::e2m3)
      .Case("E2M1X2", LinxV5Op::DataType::e2m1x2)
      .Case("E1M2X2", LinxV5Op::DataType::e1m2x2)
      .Case("E8M0", LinxV5Op::DataType::e8m0)
      .Case("HIF4X2", LinxV5Op::DataType::HiF4x2)
      .Case("E6M2", LinxV5Op::DataType::e6m2)
      .Case("RCPE6M2", LinxV5Op::DataType::rcpe6m2)
      .Case("S64", LinxV5Op::DataType::S64)
      .Case("S32", LinxV5Op::DataType::S32)
      .Case("S16", LinxV5Op::DataType::S16)
      .Case("S8", LinxV5Op::DataType::S8)
      .Case("S4X2", LinxV5Op::DataType::S4x2)
      .Case("U64", LinxV5Op::DataType::U64)
      .Case("U32", LinxV5Op::DataType::U32)
      .Case("U16", LinxV5Op::DataType::U16)
      .Case("U8", LinxV5Op::DataType::U8)
      .Case("U4X2", LinxV5Op::DataType::U4x2)
      .Case("DTYPE_NONE", LinxV5Op::DataType::EMPTY_DataType)
      .Default(None);
}

static LinxV5::TileDescriptor
checkedTileDescriptor(LinxV5::TileDescriptor Descriptor) {
  unsigned TypeBits = getTileDataTypeBits(Descriptor.DataType);
  if (!TypeBits || !Descriptor.ValidRow || !Descriptor.ValidCol ||
      !Descriptor.PhysicalCol ||
      Descriptor.SizeCode == 0 || Descriptor.SizeCode > 12)
    report_fatal_error("invalid Local tile descriptor for typed COPY");

  uint64_t CapacityBits =
      (uint64_t{1} << (Descriptor.SizeCode + 6)) * 8;
  if (Descriptor.Layout == LinxV5Op::ArgFormat::CUBE_M16 ||
      Descriptor.Layout == LinxV5Op::ArgFormat::CUBE_M32) {
    unsigned Rows = Descriptor.Layout == LinxV5Op::ArgFormat::CUBE_M16
                        ? 16 : 32;
    if (CapacityBits % (Rows * TypeBits) || Descriptor.ValidRow > Rows ||
        Descriptor.ValidCol > CapacityBits / (Rows * TypeBits))
      report_fatal_error("CUBE descriptor geometry does not fit exact capacity");
    Descriptor.PhysicalCol = CapacityBits / (Rows * TypeBits);
  }
  if (Descriptor.PhysicalCol < Descriptor.ValidCol)
    report_fatal_error("invalid Local tile descriptor for typed COPY");
  if (Descriptor.PackedPredicate) {
    bool SupportedShape =
        (Descriptor.ValidRow == 1 &&
         (Descriptor.PhysicalCol == 128 ||
          Descriptor.PhysicalCol == 2048)) ||
        ((Descriptor.ValidRow == 1 || Descriptor.ValidRow == 16) &&
         Descriptor.PhysicalCol == 32);
    if (Descriptor.DataType != LinxV5Op::DataType::U8 ||
        Descriptor.Layout != LinxV5Op::ArgFormat::NORM ||
        !SupportedShape ||
        CapacityBits / 8 <
            uint64_t{Descriptor.ValidRow} * Descriptor.PhysicalCol)
      report_fatal_error("invalid packed NORM predicate COPY descriptor");
    return Descriptor;
  }
  uint64_t RowBits = uint64_t{Descriptor.PhysicalCol} * TypeBits;
  if (!RowBits || CapacityBits % RowBits ||
      CapacityBits / RowBits < Descriptor.ValidRow)
    report_fatal_error(
        "Local tile descriptor geometry does not fit its SizeCode");
  return Descriptor;
}

// These reductions encode source geometry in B.DIM but publish a different
// result rectangle (PTO spec 8f589b7, row/column reduction contracts).
static Optional<unsigned> getInlineAsmReductionSelector(MachineInstr &MI,
                                                        StringRef Text) {
  size_t Start = Text.find("BSTART.");
  if (Start == StringRef::npos)
    return None;
  StringRef Header = Text.drop_front(Start).split('\n').first;
  if (!Header.consume_front("BSTART.TEPL") &&
      !Header.consume_front("BSTART.SFU"))
    return None;
  StringRef Operation = Header.split(',').first.trim();
  Optional<unsigned> Selector;
  if (Operation.startswith("${") && Operation.endswith(":c}")) {
    Selector = getInlineAsmOperandImmediate(
        MI, Text, Operation.data() - Text.data() + Operation.find(':'));
  } else {
    unsigned Value;
    if (!Operation.getAsInteger(0, Value))
      Selector = Value;
    else
      Selector = StringSwitch<Optional<unsigned>>(Operation)
                     .Case("TROWSUM", 64)
                     .Case("TROWMAX", 65)
                     .Case("TROWARGMAX", 76)
                     .Case("TCOLSUM", 80)
                     .Default(None);
  }
  if (Selector && (*Selector == 64 || *Selector == 65 || *Selector == 76 ||
                   *Selector == 80))
    return Selector;
  return None;
}

using DescriptorPosition =
    std::tuple<MachineBasicBlock *, const MachineInstr *, Register>;

static Optional<LinxV5::TileDescriptor> resolveTileDescriptor(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Before, Register Reg,
    SmallVectorImpl<DescriptorPosition> &Active);

static Optional<LinxV5::TileDescriptor> getInlineAsmTileDescriptor(
    MachineInstr &MI, Register Reg,
    SmallVectorImpl<DescriptorPosition> &Active) {
  LinxV5::SingleAsm SA = LinxV5::parseSingleAsm(&MI);
  Optional<unsigned> SizeCode;
  for (unsigned I = 0; I < SA.Defs.size(); ++I)
    if (SA.Defs[I]->getReg() == Reg) {
      SizeCode = SA.Sizes[I];
      break;
    }
  if (!SizeCode || *SizeCode == 0)
    return None;

  StringRef Text =
      MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  Optional<unsigned> DataType = getInlineAsmDataType(MI, Text);
  Optional<unsigned> Layout = getInlineAsmLayout(MI, Text);
  Optional<unsigned> ValidCol = getInlineAsmDimension(MI, Text, 0);
  Optional<unsigned> ValidRow = getInlineAsmDimension(MI, Text, 1);
  Optional<unsigned> PhysicalCol = getInlineAsmDimension(MI, Text, 2);
  bool HasPhysicalCol = hasInlineAsmDimension(Text, 2);
  bool IsMX = Text.startswith("BSTART.CUBE TMATMULMX,") ||
              Text.startswith("BSTART.CUBE TMATMULMX.ACC,");
  bool IsConvert = Text.startswith("BSTART.TEPL 27,");
  bool HasDerivedPhysicalCol = false;
  if (IsMX || IsConvert) {
    size_t Binding = Text.find("B.IOT");
    if (Binding == StringRef::npos)
      return None;
    StringRef SourceText = Text.drop_front(Binding + strlen("B.IOT")).ltrim();
    MachineOperand *Source = nullptr;
    if (!parseAsmOperand(&MI, SourceText.data(), Source) || !Source ||
        !Source->isReg())
      return None;
    Optional<LinxV5::TileDescriptor> SourceDescriptor = resolveTileDescriptor(
        *MI.getParent(), MI.getIterator(), Source->getReg(), Active);
    if (!SourceDescriptor)
      return None;
    bool CubeSource = SourceDescriptor->Layout == LinxV5Op::ArgFormat::CUBE_M16 ||
                      SourceDescriptor->Layout == LinxV5Op::ArgFormat::CUBE_M32;
    if (IsConvert) {
      if (!DataType || !ValidCol || !ValidRow)
        return None;
      if (CubeSource) {
        Layout = SourceDescriptor->Layout;
        unsigned Rows = *Layout == LinxV5Op::ArgFormat::CUBE_M16 ? 16 : 32;
        unsigned Bits = getTileDataTypeBits(*DataType);
        if (!Bits)
          return None;
        PhysicalCol = ((uint64_t{1} << (*SizeCode + 6)) * 8) / (Rows * Bits);
        HasDerivedPhysicalCol = true;
        if (Text.contains("B.ASSEMBLE")) {
          ValidRow = Rows;
          ValidCol = PhysicalCol;
        }
      }
    } else {
      if (!CubeSource || SA.Defs.empty() || SA.Defs.front()->getReg() != Reg)
        return None;
      Layout = SourceDescriptor->Layout;
      size_t Attributes = Text.find("B.FPATR");
      if (Attributes == StringRef::npos)
        return None;
      StringRef QuantText = Text.drop_front(Attributes + strlen("B.FPATR"))
                                .split(',').first.trim();
      Optional<unsigned> Quant;
      if (QuantText.startswith("${") && QuantText.endswith(":c}"))
        Quant = getInlineAsmOperandImmediate(
            MI, Text, QuantText.data() - Text.data() + QuantText.find(':'));
      else {
        unsigned Value;
        if (!QuantText.getAsInteger(10, Value))
          Quant = Value;
      }
      if (!Quant || *Quant != 0 || !ValidCol || !ValidRow)
        return None;
      DataType = LinxV5Op::DataType::FP32;
      unsigned SharedCount = Text.count("B.IOS");
      if (SharedCount) {
        if (SharedCount != 2 || (*ValidCol != 64 && *ValidCol != 128))
          return None;
        *ValidCol /= 4;
      }
      std::swap(ValidCol, ValidRow);
      if (*Layout != LinxV5Op::ArgFormat::CUBE_M16 &&
          *Layout != LinxV5Op::ArgFormat::CUBE_M32)
        return None;
      unsigned Rows = *Layout == LinxV5Op::ArgFormat::CUBE_M16 ? 16 : 32;
      PhysicalCol = ((uint64_t{1} << (*SizeCode + 6)) * 8) / (Rows * 32);
    }
  }
  if (!DataType || *DataType == LinxV5Op::DataType::EMPTY_DataType ||
      !Layout || !ValidCol || !ValidRow || !PhysicalCol || !*ValidCol ||
      !*ValidRow)
    return None;

  // PTO destination-shape defaults resolve an omitted LB2 to ValidCol.  Tile
  // capacity determines the remaining physical rows; it must never invent
  // additional columns.  Public CUBE TCI forms carry LB2 explicitly.
  if (!HasPhysicalCol && !HasDerivedPhysicalCol)
    *PhysicalCol = *ValidCol;
  if (Optional<unsigned> Reduction = getInlineAsmReductionSelector(MI, Text)) {
    const bool Cube = *Layout == LinxV5Op::ArgFormat::CUBE_M16 ||
                      *Layout == LinxV5Op::ArgFormat::CUBE_M32;
    if (!Cube && *Layout != LinxV5Op::ArgFormat::NORM)
      return None;
    if (*Reduction == 76)
      *DataType = LinxV5Op::DataType::U32;
    if (*Reduction == 80) {
      *ValidRow = 1;
      if (Cube) {
        unsigned Bits = getTileDataTypeBits(*DataType);
        if (!Bits)
          return None;
        unsigned CellBits = *Layout == LinxV5Op::ArgFormat::CUBE_M16 ? 64 : 32;
        unsigned CellCols = std::max(1u, CellBits / Bits);
        *PhysicalCol = alignTo(*ValidCol, CellCols);
      }
    } else {
      *ValidCol = 1;
      if (!Cube)
        *PhysicalCol = 1;
    }
  }
  return checkedTileDescriptor(LinxV5::TileDescriptor{
      *DataType, *Layout, *ValidRow, *ValidCol, *PhysicalCol, *SizeCode});
}

static Optional<LinxV5::TileDescriptor> getDefinedTileDescriptor(
    MachineInstr &MI, Register Reg,
    SmallVectorImpl<DescriptorPosition> &Active) {
  if (MI.getOpcode() == LinxV5::PseudoLocalTileABISeed)
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        static_cast<unsigned>(MI.getOperand(5).getImm()),
        static_cast<unsigned>(MI.getOperand(6).getImm()),
        static_cast<unsigned>(MI.getOperand(3).getImm()),
        static_cast<unsigned>(MI.getOperand(2).getImm()),
        static_cast<unsigned>(MI.getOperand(4).getImm()),
        static_cast<unsigned>(MI.getOperand(1).getImm()), false});

  if ((MI.getOpcode() == LinxV5::PseudoTCOPYDesc ||
       MI.getOpcode() == LinxV5::PseudoPCOPYDesc))
    return checkedTileDescriptor(
        LinxV5::TileDescriptor{static_cast<unsigned>(MI.getOperand(6).getImm()),
                               static_cast<unsigned>(MI.getOperand(7).getImm()),
                               static_cast<unsigned>(MI.getOperand(4).getImm()),
                               static_cast<unsigned>(MI.getOperand(3).getImm()),
                               static_cast<unsigned>(MI.getOperand(5).getImm()),
                               static_cast<unsigned>(MI.getOperand(1).getImm()),
                               MI.getOpcode() == LinxV5::PseudoPCOPYDesc});

  if (MI.getOpcode() == LinxV5::PseudoTCMP_Row_SizeI ||
      MI.getOpcode() == LinxV5::PseudoTCMP_SizeI) {
    if (MI.getOperand(1).getReg() != LinxV5::R0 ||
        MI.getOperand(3).getReg() != LinxV5::R0 ||
        MI.getOperand(6).getImm() != 0)
      return None;
    unsigned Rows = MI.getOperand(2).getImm();
    unsigned Cols = MI.getOperand(4).getImm();
    unsigned DType = MI.getOperand(5).getImm();
    unsigned PhysicalCols = MI.getOpcode() == LinxV5::PseudoTCMP_Row_SizeI
                                ? MI.getOperand(11).getImm()
                                : Cols;
    bool U32Row =
        Rows == 1 && DType == LinxV5Op::DataType::U32 &&
        ((PhysicalCols == 128 && (Cols == 32 || Cols == 128)) ||
         (PhysicalCols == 2048 && Cols == 2048));
    bool F32Tile = DType == LinxV5Op::DataType::FP32 &&
                   PhysicalCols == 32 && Cols == 32 &&
                   (Rows == 1 || Rows == 16);
    if (!U32Row && !F32Tile)
      return None;
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        LinxV5Op::DataType::U8, LinxV5Op::ArgFormat::NORM, Rows, Cols,
        PhysicalCols, static_cast<unsigned>(MI.getOperand(7).getImm()), true});
  }

  if (MI.getOpcode() == LinxV5::PseudoTCVT_GPR_Masked_SizeI) {
    if (!MI.getOperand(1).isReg() || MI.getOperand(1).getReg() != LinxV5::R0 ||
        !MI.getOperand(3).isReg() || MI.getOperand(3).getReg() != LinxV5::R0)
      return None;
    // TCVT publishes the destination dtype and persistent Tile layout. The
    // instruction's B.DATR layout is encoded separately by the MC emitter.
    unsigned ValidCol = MI.getOperand(4).getImm();
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        static_cast<unsigned>(MI.getOperand(6).getImm()),
        static_cast<unsigned>(MI.getOperand(7).getImm()),
        static_cast<unsigned>(MI.getOperand(2).getImm()), ValidCol, ValidCol,
        static_cast<unsigned>(MI.getOperand(8).getImm())});
  }

  if (MI.getOpcode() == LinxV5::PseudoTMOV_SizeI) {
    if (!MI.getOperand(1).isReg() || MI.getOperand(1).getReg() != LinxV5::R0 ||
        !MI.getOperand(3).isReg() || MI.getOperand(3).getReg() != LinxV5::R0 ||
        !MI.getOperand(5).isReg() || MI.getOperand(5).getReg() != LinxV5::R0)
      return None;
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        static_cast<unsigned>(MI.getOperand(7).getImm()),
        static_cast<unsigned>(MI.getOperand(10).getImm()),
        static_cast<unsigned>(MI.getOperand(4).getImm()),
        static_cast<unsigned>(MI.getOperand(2).getImm()),
        static_cast<unsigned>(MI.getOperand(6).getImm()),
        static_cast<unsigned>(MI.getOperand(8).getImm())});
  }

  if (MI.getOpcode() == LinxV5::PseudoMGATHER_GPR_Masked_SizeI) {
    if (!MI.getOperand(1).isReg() || MI.getOperand(1).getReg() != LinxV5::R0 ||
        !MI.getOperand(3).isReg() || MI.getOperand(3).getReg() != LinxV5::R0)
      return None;
    // TLSU operands carry LB0 (columns) before LB1 (rows), unlike the
    // elementwise arithmetic pseudos below. Omitted LB2 defaults to LB0.
    unsigned ValidCol = MI.getOperand(2).getImm();
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        static_cast<unsigned>(MI.getOperand(5).getImm()),
        static_cast<unsigned>(MI.getOperand(7).getImm()),
        static_cast<unsigned>(MI.getOperand(4).getImm()), ValidCol, ValidCol,
        static_cast<unsigned>(MI.getOperand(8).getImm())});
  }

  if (MI.getOpcode() == LinxV5::PseudoTLOAD_noDsrc_noDdst ||
      MI.getOpcode() == LinxV5::PseudoTLOAD_noDsrc_Ddst ||
      MI.getOpcode() == LinxV5::PseudoTLOAD_Dsrc_noDdst ||
      MI.getOpcode() == LinxV5::PseudoTLOAD_Dsrc_Ddst) {
    if (!MI.getOperand(1).isReg() || MI.getOperand(1).getReg() != LinxV5::R0 ||
        !MI.getOperand(3).isReg() || MI.getOperand(3).getReg() != LinxV5::R0 ||
        !MI.getOperand(5).isReg() || MI.getOperand(5).getReg() != LinxV5::R0)
      return None;
    unsigned LayoutOperand =
        (MI.getOpcode() == LinxV5::PseudoTLOAD_Dsrc_noDdst ||
         MI.getOpcode() == LinxV5::PseudoTLOAD_Dsrc_Ddst)
            ? 10
            : 9;
    unsigned Layout = MI.getOperand(LayoutOperand).getImm();
    if (Layout == LinxV5Op::ArgFormat::ND2M32)
      Layout = LinxV5Op::ArgFormat::CUBE_M32;
    else if (Layout == LinxV5Op::ArgFormat::ND2M16)
      Layout = LinxV5Op::ArgFormat::CUBE_M16;
    else if (Layout != LinxV5Op::ArgFormat::NORM &&
             Layout != LinxV5Op::ArgFormat::CUBE_M32 &&
             Layout != LinxV5Op::ArgFormat::CUBE_M16 &&
             Layout != LinxV5Op::ArgFormat::ND2N8)
      return None;
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        static_cast<unsigned>(MI.getOperand(7).getImm()), Layout,
        static_cast<unsigned>(MI.getOperand(4).getImm()),
        static_cast<unsigned>(MI.getOperand(2).getImm()),
        static_cast<unsigned>(MI.getOperand(6).getImm()),
        static_cast<unsigned>(MI.getOperand(8).getImm())});
  }

  if (MI.getOpcode() == LinxV5::PseudoTLOADSpillDesc)
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        static_cast<unsigned>(MI.getOperand(14).getImm()),
        static_cast<unsigned>(MI.getOperand(15).getImm()),
        static_cast<unsigned>(MI.getOperand(12).getImm()),
        static_cast<unsigned>(MI.getOperand(11).getImm()),
        static_cast<unsigned>(MI.getOperand(13).getImm()),
        static_cast<unsigned>(MI.getOperand(16).getImm())});

  if (MI.getOpcode() == LinxV5::PseudoTReloadDesc)
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        static_cast<unsigned>(MI.getOperand(5).getImm()),
        static_cast<unsigned>(MI.getOperand(6).getImm()),
        static_cast<unsigned>(MI.getOperand(3).getImm()),
        static_cast<unsigned>(MI.getOperand(2).getImm()),
        static_cast<unsigned>(MI.getOperand(4).getImm()),
        static_cast<unsigned>(MI.getOperand(7).getImm())});

  switch (MI.getOpcode()) {
  case LinxV5::PseudoTADD_Masked_SizeI:
  case LinxV5::PseudoTSUB_Masked_SizeI:
  case LinxV5::PseudoTBinary_SizeI:
  case LinxV5::PseudoTBinary_GPR_Masked_SizeI:
  case LinxV5::PseudoTNEG_GPR_Masked_SizeI:
  case LinxV5::PseudoTSEL_Row_SizeI:
  case LinxV5::PseudoTSEL_SizeI:
  case LinxV5::PseudoTSEL_GPR_SizeI:
  case LinxV5::PseudoTEXPANDS_Row_SizeI:
  case LinxV5::PseudoTEXPANDS_SizeI:
  case LinxV5::PseudoTLEA_SizeI:
  case LinxV5::PseudoTCI_EW_Row_SizeI:
  case LinxV5::PseudoTCI_EW_SizeI: {
    if (!MI.getOperand(1).isReg() || MI.getOperand(1).getReg() != LinxV5::R0 ||
        !MI.getOperand(3).isReg() || MI.getOperand(3).getReg() != LinxV5::R0)
      return None;
    unsigned ValidRow = MI.getOperand(2).getImm();
    unsigned ValidCol = MI.getOperand(4).getImm();
    unsigned DataType = MI.getOperand(5).getImm();
    if (MI.getOpcode() == LinxV5::PseudoTLEA_SizeI) {
      // TLEA names the index dtype but publishes signed/unsigned B64 byte
      // displacements. A copy must preserve the result descriptor.
      if (DataType == LinxV5Op::DataType::S32 ||
          DataType == LinxV5Op::DataType::S64)
        DataType = LinxV5Op::DataType::S64;
      else if (DataType == LinxV5Op::DataType::U32 ||
               DataType == LinxV5Op::DataType::U64)
        DataType = LinxV5Op::DataType::U64;
      else
        return None;
    }
    unsigned Layout = MI.getOperand(6).getImm();
    unsigned SizeCode = MI.getOperand(7).getImm();
    // These pseudos omit LB2 under the common destination-shape contract.
    // Capacity supplies additional physical rows, never extra columns.
    unsigned PhysicalCol = ValidCol;
    switch (MI.getOpcode()) {
    case LinxV5::PseudoTCI_EW_Row_SizeI:
      PhysicalCol = MI.getOperand(10).getImm();
      break;
    case LinxV5::PseudoTSEL_Row_SizeI:
      PhysicalCol = MI.getOperand(11).getImm();
      break;
    case LinxV5::PseudoTEXPANDS_Row_SizeI:
      PhysicalCol = MI.getOperand(9).getImm();
      break;
    default:
      break;
    }
    return checkedTileDescriptor(LinxV5::TileDescriptor{
        DataType, Layout, ValidRow, ValidCol, PhysicalCol, SizeCode});
  }
  default:
    break;
  }
  if (MI.isInlineAsm())
    return getInlineAsmTileDescriptor(MI, Reg, Active);
  return None;
}

static Optional<LinxV5::TileDescriptor> resolveTileDescriptor(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Before, Register Reg,
    SmallVectorImpl<DescriptorPosition> &Active) {
  DescriptorPosition Key{&MBB, Before == MBB.end() ? nullptr : &*Before, Reg};
  if (is_contained(Active, Key))
    return None;
  Active.push_back(Key);

  for (MachineInstr &MI : reverse(make_range(MBB.begin(), Before))) {
    if (!MI.definesRegister(Reg))
      continue;
    Optional<LinxV5::TileDescriptor> Result;
    if (MI.getOpcode() == TargetOpcode::COPY)
      Result = resolveTileDescriptor(MBB, MI.getIterator(),
                                     MI.getOperand(1).getReg(), Active);
    else
      Result = getDefinedTileDescriptor(MI, Reg, Active);
    Active.pop_back();
    return Result;
  }

  Optional<LinxV5::TileDescriptor> Joined;
  for (MachineBasicBlock *Pred : MBB.predecessors()) {
    Optional<LinxV5::TileDescriptor> Incoming =
        resolveTileDescriptor(*Pred, Pred->end(), Reg, Active);
    if (!Incoming) {
      Active.pop_back();
      return None;
    }
    if (Joined && !(*Joined == *Incoming))
      report_fatal_error(
          "conflicting Local tile descriptors at COPY/PHI control-flow join");
    Joined = *Incoming;
  }
  Active.pop_back();
  return Joined;
}

LinxV5::TileDescriptor
LinxV5::getTileRegDescriptor(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI,
                             MCRegister Reg) {
  Optional<TileDescriptor> Descriptor =
      tryGetTileRegDescriptor(MBB, MBBI, Reg);
  if (!Descriptor)
    report_fatal_error(
        "cannot prove a complete Local tile descriptor for COPY/PHI");
  return *Descriptor;
}

Optional<LinxV5::TileDescriptor>
LinxV5::tryGetTileRegDescriptor(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI,
                                MCRegister Reg) {
  SmallVector<DescriptorPosition, 8> Active;
  return resolveTileDescriptor(MBB, MBBI, Reg, Active);
}

bool LinxV5::enableBFIOpt() { return EnableBFIOpt; }

LinxV5InstrInfo::LinxV5InstrInfo(LinxV5Subtarget &STI)
    : LinxV5GenInstrInfo(LinxV5::ADJCALLSTACKDOWN, LinxV5::ADJCALLSTACKUP),
      STI(STI) {}

bool LinxV5InstrInfo::isSchedulingBoundary(const MachineInstr &MI,
                                           const MachineBasicBlock *MBB,
                                           const MachineFunction &MF) const {
  if (TargetInstrInfo::isSchedulingBoundary(MI, MBB, MF))
    return true;

  if (LinxV5::isTileOp(MI))
    return true;

  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT() &&
      MI.modifiesRegister(LinxV5::SIMT_P, STI.getRegisterInfo()))
    return true;

  return false;
}

MachineBasicBlock::iterator
LinxV5InstrInfo::getFirstInsertionPoint(MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator I = TargetInstrInfo::getFirstInsertionPoint(MBB);
  if (!MBB.getParent()->getSubtarget<LinxV5Subtarget>().isSIMT())
    return I;

  while (I != MBB.end()) {
    unsigned Opc = I->getOpcode();
    if (Opc != LinxV5::LinxV5PseudoCopy2P &&
        Opc != LinxV5::LinxV5PseudoCopy2PTerm)
      break;
    ++I;
  }
  return I;
}

unsigned LinxV5InstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                              int &FrameIndex) const {
  switch (MI.getOpcode()) {
  default:
    return 0;
  // LinxV5's instructions.
  case LinxV5::LBI:
  case LinxV5::LHI:
  case LinxV5::LWI:
  case LinxV5::LDI:
  case LinxV5::LBUI:
  case LinxV5::LHUI:
  case LinxV5::LWUI:
    break;
  }

  if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
      MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }

  return 0;
}

unsigned LinxV5InstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                             int &FrameIndex) const {
  switch (MI.getOpcode()) {
  default:
    return 0;
  case LinxV5::SBI:
  case LinxV5::SHI:
  case LinxV5::SWI:
  case LinxV5::SDI:
    break;
  }

  if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
      MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }

  return 0;
}

void LinxV5InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MBBI,
                                  const DebugLoc &DL, MCRegister DstReg,
                                  MCRegister SrcReg, bool KillSrc) const {
  if (SrcReg == LinxV5::Tile_ACC1 || DstReg == LinxV5::Tile_ACC1)
    report_fatal_error("could not copy phys `acc' reg, please manually copy it "
                       "to vec tile reg by blk_acccvt()/TCVT()");
  if (LinxV5::Shared_ABSRegClass.contains(SrcReg) ||
      LinxV5::Shared_ABSRegClass.contains(DstReg))
    report_fatal_error(
        "cannot copy a Shared register: Shared handles have no MOVR/copy "
        "instruction; use B.IOS to publish or keep the handle in Shared_ABS");
  auto *TRI =
      MBB.getParent()->getSubtarget<LinxV5Subtarget>().getRegisterInfo();
  auto &MRI = MBB.getParent()->getRegInfo();
  // Shared tile handles and Local tiles are different architectural register
  // files.  A generic COPY between them must not be lowered to TCOPY/TMOV:
  // those operations have Local tile operands only.  Shared<->Local transfers
  // are represented by the explicit V5 shared intrinsics and their dedicated
  // pseudos instead.
  if ((isLocalTileReg(MRI, DstReg) && isSharedTileReg(MRI, SrcReg)) ||
      (isSharedTileReg(MRI, DstReg) && isLocalTileReg(MRI, SrcReg)))
    report_fatal_error("cannot copy between LinxV5 Local tiles and Shared tile handles");
  if (MBB.getParent()->getSubtarget<LinxV5Subtarget>().isSIMT()) {
    assert(!LinxV5::GRRegClass.contains(DstReg) && "must reduce inst!");
    assert(!(LinxV5::SIMTCGSLRegClass.contains(DstReg) &&
             !TRI->isUniformReg(MRI, SrcReg, false)) &&
           "invalid copy VecReg to ScalarReg!");
    unsigned RegSize = 0;
    RegSize = LinxV5::getUseRegSize(MBB, MBBI, DstReg);
    if (RegSize && LinxV5::SIMTCGSLRegClass.contains(DstReg))
      BuildMI(MBB, MBBI, DL, get(LinxV5::SIMT_ORI_SCAR), DstReg)
          .addImm(LinxV5::getSIMTDstTypeFromSize(RegSize))
          .addReg(SrcReg, getKillRegState(KillSrc))
          .addImm(LinxV5::getSIMTSrcTypeFromSize(RegSize))
          .addImm(0);
    else if (RegSize && LinxV5::SIMTCGVLRegClass.contains(DstReg))
      BuildMI(MBB, MBBI, DL, get(LinxV5::SIMT_MOV), DstReg)
          .addImm(LinxV5::getSIMTDstTypeFromSize(RegSize))
          .addReg(SrcReg, getKillRegState(KillSrc))
          .addImm(LinxV5::getSIMTSrcTypeFromSize(RegSize));
    else {
      // dead copy, no user
      if (LinxV5::SIMTCGSLRegClass.contains(DstReg))
        BuildMI(MBB, MBBI, DL, get(LinxV5::SIMT_ORI_SCAR), DstReg)
            .addImm(LinxV5::getSIMTDstTypeFromSize(
                LinxV5::SIMTRegSize::SIMT_REG_SIZE_W))
            .addReg(SrcReg, getKillRegState(KillSrc))
            .addImm(LinxV5::getSIMTSrcTypeFromSize(
                LinxV5::SIMTRegSize::SIMT_REG_SIZE_W))
            .addImm(0);
      else if (LinxV5::SIMTCGVLRegClass.contains(DstReg))
        BuildMI(MBB, MBBI, DL, get(LinxV5::SIMT_MOV), DstReg)
            .addImm(LinxV5::getSIMTDstTypeFromSize(
                LinxV5::SIMTRegSize::SIMT_REG_SIZE_W))
            .addReg(SrcReg, getKillRegState(KillSrc))
            .addImm(LinxV5::getSIMTSrcTypeFromSize(
                LinxV5::SIMTRegSize::SIMT_REG_SIZE_W));
      else {
        errs() << *MBBI;
        report_fatal_error("unexpected copy");
      }
    }
  } else if (LinxV5::Tile_ABS_CGRegClass.contains(DstReg)) {
    if (!EnableGenericElement) {
      unsigned RegSizeCode = LinxV5::getTileRegSize(MBB, MBBI, SrcReg, false);
      BuildMI(MBB, MBBI, DL, get(LinxV5::PseudoTCOPY), DstReg)
          .addImm(RegSizeCode)
          .addReg(SrcReg, getKillRegState(KillSrc));
      return;
    }
    LinxV5::TileDescriptor Descriptor =
        LinxV5::getTileRegDescriptor(MBB, MBBI, SrcReg);
    BuildMI(MBB, MBBI, DL,
            get(Descriptor.PackedPredicate ? LinxV5::PseudoPCOPYDesc
                                           : LinxV5::PseudoTCOPYDesc),
            DstReg)
        .addImm(Descriptor.SizeCode)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addImm(Descriptor.ValidCol)
        .addImm(Descriptor.ValidRow)
        .addImm(Descriptor.PhysicalCol)
        .addImm(Descriptor.DataType)
        .addImm(Descriptor.Layout);
  } else {
    BuildMI(MBB, MBBI, DL, get(LinxV5::ORI), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addImm(0);
  }

  return;
}

void LinxV5InstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator I,
                                          Register SrcReg, bool IsKill, int FI,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  if ((MF->getInfo<LinxV5MachineFunctionInfo>()->hasLocalTileABICall() ||
       LinxV5::getLocalTileABIInfo(MF->getFunction())) &&
      LinxV5::Tile_ABSRegClass.hasSubClassEq(RC))
    report_fatal_error(
        "linx-local-tile-abi cannot spill a Local Tile payload in an ABI "
        "callee or across its call boundary");
  if (LinxV5::Shared_ABSRegClass.hasSubClassEq(RC))
    report_fatal_error(
        "Shared register spill is required, but Shared spill is not "
        "supported by the current LinxV5 ISA");

  MachineFrameInfo &MFI = MF->getFrameInfo();
  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOStore,
      MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  ++NumStoreReg;

  bool IsSIMT = MF->getSubtarget<LinxV5Subtarget>().isSIMT();
  if (IsSIMT) {
    BuildMI(MBB, I, DL, get(LinxV5::PseudoVecSpill))
        .addReg(SrcReg, getKillRegState(IsKill))
        .addImm(0) // RegSizeInBytes, 0 for scalar
        .addFrameIndex(FI)
        .addMemOperand(MMO);
    return;
  }

  if (LinxV5::Tile_ABSRegClass.hasSubClassEq(RC)) {
    ++NumTileSpill;
    // Tile_ABS is a 64-bit handle register class, while the smallest legal
    // tile spill fragment is 128 B. FastRA creates the stack object from the
    // register class width, so establish the tile ABI minimum here before
    // frame layout. The spill payload is always encoded as S64/NORM; its
    // source tile datatype is intentionally irrelevant.
    if (MFI.getObjectSize(FI) < 128) {
      MFI.setObjectSize(FI, 128);
      MFI.setObjectAlignment(FI, Align(256));
    }
    BuildMI(MBB, I, DL, get(LinxV5::PseudoTSpill))
        .addReg(SrcReg, getKillRegState(IsKill))
        .addFrameIndex(FI)
        .addMemOperand(MMO);
  } else {
    BuildMI(MBB, I, DL, get(LinxV5::SDI))
        .addReg(SrcReg, getKillRegState(IsKill))
        .addFrameIndex(FI)
        .addImm(0)
        .addMemOperand(MMO);
  }
}

void LinxV5InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register DstReg,
    int FI, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI) const {

  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  if (LinxV5::Shared_ABSRegClass.hasSubClassEq(RC))
    report_fatal_error(
        "Shared register reload is required, but Shared reload is not "
        "supported by the current LinxV5 ISA");

  const MachineFrameInfo &MFI = MF->getFrameInfo();
  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
      MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  if (!LinxV5::GRRegClass.hasSubClassEq(RC) &&
      !LinxV5::LTRRegClass.hasSubClassEq(RC) &&
      !LinxV5::LURRegClass.hasSubClassEq(RC) &&
      !LinxV5::MixedGPRRegClass.hasSubClassEq(RC) &&
      !LinxV5::MixedGPRNoRARegClass.hasSubClassEq(RC) &&
      !LinxV5::Tile_ABSRegClass.hasSubClassEq(RC) &&
      !LinxV5::SIMTCGVRegClass.hasSubClassEq(RC))
    llvm_unreachable("Can't load this register from stack slot");

  bool IsSIMT = MF->getSubtarget<LinxV5Subtarget>().isSIMT();
  if (IsSIMT) {
    BuildMI(MBB, I, DL, get(LinxV5::PseudoVecReload), DstReg)
        .addImm(0)
        .addFrameIndex(FI)
        .addMemOperand(MMO);
    return;
  }

  if (LinxV5::Tile_ABSRegClass.hasSubClassEq(RC)) {
    ++NumTileReload;
    BuildMI(MBB, I, DL, get(LinxV5::PseudoTReload), DstReg)
        .addFrameIndex(FI)
        .addMemOperand(MMO);
  } else {  // Scalar Regs
    BuildMI(MBB, I, DL, get(LinxV5::LDI), DstReg)
        .addFrameIndex(FI)
        .addImm(0)
        .addMemOperand(MMO);
  }
}

static void generateInt32InstSeq(int64_t Val, LinxV5MatInt::InstSeq &Res) {
  if (Val < 0 && isUInt<12>(-Val)) {
    Res.push_back(LinxV5MatInt::Inst(LinxV5::SUBI, -Val));
    return;
  }

  int64_t Lo12 = (uint64_t)Val & 0xfff;
  int64_t Hi20 = Val >> 12;
  assert(isInt<20>(Hi20) && "unexpected Int32!");

  if (Hi20)
    Res.push_back(LinxV5MatInt::Inst(LinxV5::LUI, int64_t(Hi20)));

  if (Lo12 || Hi20 == 0) {
    Res.push_back(LinxV5MatInt::Inst(LinxV5::ADDI, Lo12));
  }
}

static void generateUInt32InstSeq(uint64_t Val, LinxV5MatInt::InstSeq &Res,
                                  bool ZeroExt) {
  int64_t SVal = (int64_t)((int32_t)Val);

  generateInt32InstSeq(SVal, Res);

  if ((uint64_t)SVal != Val && ZeroExt)
    Res.push_back(LinxV5MatInt::Inst(LinxV5::OR_UW, 0));
}

static int getSymmetricWidth(uint64_t Val) {
  int Width = 32;
  uint64_t UMask = 0xFFFFFFFFFFFFFFFFU;
  uint64_t LMask = 0xFFFFFFFF;
  while ((((Val >> Width) ^ Val) & LMask) != 0) {
    Width--;
    LMask >>= 1;
    UMask >>= 2;
  }
  if ((Val & (~UMask)) == 0)
    return Width;
  else
    return 0;
}

void LinxV5::generateMatIntSeq(int64_t Val, LinxV5MatInt::InstSeq &Res,
                               bool HasFloat) {
  if (isInt<32>(Val)) {
    generateInt32InstSeq(Val, Res);
    return;
  } else if (isUInt<32>(Val)) {
    generateUInt32InstSeq(Val, Res, true);
    return;
  }

  uint64_t Hi32 = (static_cast<uint64_t>(Val) >> 32) & 0xFFFFFFFF;
  uint64_t Lo32 = static_cast<uint64_t>(Val) & 0xFFFFFFFF;
  if (enableBFIOpt() && !HasFloat) {
    // Optimize symmetric 64-bit long const with BFI
    int SymmWidth = getSymmetricWidth(static_cast<uint64_t>(Val));
    if (SymmWidth >= 16 && SymmWidth % 8 == 0) {
      uint64_t NewVal =
          static_cast<uint64_t>(Val) >> static_cast<unsigned>(SymmWidth);
      if (isUInt<32>(NewVal)) {
        generateUInt32InstSeq(NewVal, Res, false);
      } else {
        generateInt32InstSeq(NewVal, Res);
      }
      // HL.BFI directly encodes the inclusive immr/imms endpoints. Copy the
      // low SymmWidth bits into [2*SymmWidth-1:SymmWidth].
      unsigned First = static_cast<unsigned>(SymmWidth);
      unsigned Last = 2u * static_cast<unsigned>(SymmWidth) - 1u;
      Res.push_back(
          LinxV5MatInt::Inst(LinxV5::HL_BFI, (Last << 6) | First));
      return;
    }
  }

  // General 64-bit long const
  int64_t Lo = SignExtend64<13>(Val);
  // addi + subi can only present [-4095, 4095], and present 0 twice.
  // but the full range of 13bit integer is [-4096, 4095].
  // so we pick Lo12 for -4096.
  if (Lo == (-4096)) {
    Lo = SignExtend64<12>(Val);
  }
  Val = (uint64_t)Val - (uint64_t)Lo;
  unsigned Shamt = 0;
  // 0xffffffff7fffffff - (-1) = 0xffffffff80000000 is int32
  if (!isInt<32>(Val)) {
    Shamt = findFirstSet(static_cast<uint64_t>(Val));
    assert(Shamt >= 12 && "unexpected trailing zero amount!");
    Val >>= Shamt;
    if (!isInt<12>(Val) && isInt<20>(Val)) {
      Val = (uint64_t)Val << 12;
      Shamt -= 12;
    }
  }

  generateMatIntSeq(Val, Res, HasFloat);

  if (Shamt)
    Res.push_back(LinxV5MatInt::Inst(LinxV5::SLLI, Shamt));

  if (Lo) {
    if (Lo >= 0)
      Res.push_back(LinxV5MatInt::Inst(LinxV5::ADDI, Lo));
    else
      Res.push_back(LinxV5MatInt::Inst(LinxV5::SUBI, -Lo));
  }
}

unsigned LinxV5::getSIMTDstType(const MVT VT) {
  switch (VT.SimpleTy) {
  case MVT::i8:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_B;
  case MVT::i16:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_H;
  case MVT::i32:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_W;
  case MVT::i64:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_D;
  default:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_NONE;
  }
}

unsigned LinxV5::getSIMTSignedSrcType(const MVT VT) {
  switch (VT.SimpleTy) {
  case MVT::i8:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SB;
  case MVT::i16:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SH;
  case MVT::i32:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SW;
  case MVT::i64:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD;
  default:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_NONE;
  }
}

unsigned LinxV5::getSIMTUnsignedSrcType(const MVT VT) {
  switch (VT.SimpleTy) {
  case MVT::i8:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UB;
  case MVT::i16:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UH;
  case MVT::i32:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UW;
  case MVT::i64:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD;
  default:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_NONE;
  }
}

void LinxV5::generateSIMTMatIntSeq(int64_t Val, LinxV5MatInt::SIMTInstSeq &Res,
                                   MVT VT) {
  LinxV5MatInt::InstSeq Seq;
  generateMatIntSeq(Val, Seq, true);
  unsigned DstType = getSIMTDstType(VT);
  unsigned SrcType = getSIMTSignedSrcType(VT);

  for (auto &I : Seq) {
    switch (I.Opc) {
    case LinxV5::LUI:
      Res.push_back(
          LinxV5MatInt::SIMTInst(LinxV5::LUI, DstType, SrcType, I.Imm));
      break;
    case LinxV5::ADDI:
      Res.push_back(LinxV5MatInt::SIMTInst(LinxV5::SIMT_ADDI_SCAR, DstType,
                                           SrcType, I.Imm));
      break;
    case LinxV5::SUBI:
      Res.push_back(LinxV5MatInt::SIMTInst(LinxV5::SIMT_SUBI_SCAR, DstType,
                                           SrcType, I.Imm));
      break;
    case LinxV5::OR_UW:
      if (VT == MVT::i64) {
        Res.back().DstType = getSIMTDstType(MVT::i32);
        Res.push_back(LinxV5MatInt::SIMTInst(LinxV5::SIMT_ICVT_U322U64, DstType,
                                             getSIMTUnsignedSrcType(MVT::i32),
                                             I.Imm));
      }
      break;
    case LinxV5::SLLI:
      Res.push_back(LinxV5MatInt::SIMTInst(LinxV5::SIMT_SLLI_SCAR, DstType,
                                           SrcType, I.Imm));
      break;
    default:
      report_fatal_error("unrecognized MatInt Opcode!");
    }
  }
}

void LinxV5InstrInfo::movImm(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI,
                             const DebugLoc &DL, Register DstReg, uint64_t Val,
                             MachineInstr::MIFlag Flag) const {
  int64_t Imm = (int64_t)Val;

  LinxV5MatInt::InstSeq Seq;
  Register SrcReg = LinxV5::R0;
  LinxV5::generateMatIntSeq(Imm, Seq, false);

  for (LinxV5MatInt::Inst &I : Seq) {
    if (I.Opc == LinxV5::LUI) {
      BuildMI(MBB, MBBI, DL, get(I.Opc), DstReg).addImm(I.Imm).setMIFlag(Flag);
    } else if (I.Opc == LinxV5::OR_UW) {
      BuildMI(MBB, MBBI, DL, get(I.Opc), DstReg)
          .addReg(LinxV5::R0)
          .addReg(SrcReg)
          .addImm(I.Imm)
          .setMIFlag(Flag);
    } else {
      BuildMI(MBB, MBBI, DL, get(I.Opc), DstReg)
          .addReg(SrcReg)
          .addImm(I.Imm)
          .setMIFlag(Flag);
    }

    SrcReg = DstReg;
  }
}

// The contents of values added to Cond are not examined outside of
// LinxV5InstrInfo, giving us flexibility in what to push to it. For LinxV5, we
// push BranchOpcode, Reg1, Reg2.
static void parseCondBranch(MachineInstr &LastInst, MachineBasicBlock *&Target,
                            SmallVectorImpl<MachineOperand> &Cond) {
  // Block ends with fall-through condbranch.
  assert(LastInst.getDesc().isConditionalBranch() &&
         "Unknown conditional branch");
  Target = LastInst.getOperand(2).getMBB();
  Cond.push_back(MachineOperand::CreateImm(LastInst.getOpcode()));
  Cond.push_back(LastInst.getOperand(0));
  Cond.push_back(LastInst.getOperand(1));
}

bool LinxV5InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *&TBB,
                                    MachineBasicBlock *&FBB,
                                    SmallVectorImpl<MachineOperand> &Cond,
                                    bool AllowModify) const {
  // FIX ME: when simt ready here.
  if (STI.isSIMT())
    return true;

  TBB = FBB = nullptr;
  Cond.clear();

  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false;

  // Count the number of terminators and find the first unconditional or
  // indirect branch.
  MachineBasicBlock::iterator FirstUncondOrIndirectBr = MBB.end();
  int NumTerminators = 0;
  for (auto J = I.getReverse();
       J != MBB.rend() && (J->isDebugInstr() || isUnpredicatedTerminator(*J));
       J++) {
    if (J->isDebugInstr())
      continue;
    NumTerminators++;
    if (J->getDesc().isUnconditionalBranch() ||
        J->getDesc().isIndirectBranch()) {
      FirstUncondOrIndirectBr = J.getReverse();
    }
  }

  // If AllowModify is true, we can erase any terminators after
  // FirstUncondOrIndirectBR.
  if (AllowModify && FirstUncondOrIndirectBr != MBB.end()) {
    while (std::next(FirstUncondOrIndirectBr) != MBB.end()) {
      std::next(FirstUncondOrIndirectBr)->eraseFromParent();
      NumTerminators--;
    }
    I = FirstUncondOrIndirectBr;
  }

  // We can't handle blocks that end in an indirect branch.
  if (I->getDesc().isIndirectBranch())
    return true;

  // We can't handle blocks with more than 2 terminators.
  if (NumTerminators > 2)
    return true;

  // Handle a single unconditional branch.
  if (NumTerminators == 1 && I->getDesc().isUnconditionalBranch()) {
    TBB = getBranchDestBlock(*I);
    return false;
  }

  // Handle a single conditional branch.
  if (NumTerminators == 1 && I->getDesc().isConditionalBranch()) {
    // PseudoBRCondCARG carries only its target and an implicit CARG use,
    // unlike the three-operand compare-and-branch instructions modeled by
    // parseCondBranch().  Leave it intact for its dedicated lowering rather
    // than reading nonexistent comparison operands.
    if (I->getOpcode() == LinxV5::PseudoBRCondCARG)
      return true;
    parseCondBranch(*I, TBB, Cond);
    return false;
  }

  // Handle a conditional branch followed by an unconditional branch.
  if (NumTerminators == 2 && std::prev(I)->getDesc().isConditionalBranch() &&
      I->getDesc().isUnconditionalBranch()) {
    if (std::prev(I)->getOpcode() == LinxV5::PseudoBRCondCARG)
      return true;
    parseCondBranch(*std::prev(I), TBB, Cond);
    FBB = getBranchDestBlock(*I);
    return false;
  }

  // Otherwise, we can't handle this.
  return true;
}

unsigned LinxV5InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                       int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return 0;

  if (!I->getDesc().isUnconditionalBranch() &&
      !I->getDesc().isConditionalBranch())
    return 0;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();

  I = MBB.end();
  if (I == MBB.begin())
    return 1;
  --I;
  if (!I->getDesc().isConditionalBranch())
    return 1;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();
  return 2;
}

// Inserts a branch into the end of the specific MachineBasicBlock, returning
// the number of instructions inserted.
unsigned LinxV5InstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;

  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 3 || Cond.size() == 0) &&
         "LinxV5 branch conditions have two components!");

  // Unconditional branch.
  if (Cond.empty()) {
    MachineInstr &MI = *BuildMI(&MBB, DL, get(LinxV5::PseudoBR)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    return 1;
  }

  // Either a one or two-way conditional branch.
  unsigned Opc = static_cast<unsigned>(Cond[0].getImm());
  MachineInstr &CondMI =
      *BuildMI(&MBB, DL, get(Opc)).add(Cond[1]).add(Cond[2]).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(CondMI);

  // One-way conditional branch.
  if (!FBB)
    return 1;

  // Two-way conditional branch.
  MachineInstr &MI = *BuildMI(&MBB, DL, get(LinxV5::PseudoBR)).addMBB(FBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(MI);
  return 2;
}

bool LinxV5InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert((Cond.size() == 3) && "Invalid branch condition!");
  Cond[0].setImm(getOppositeOpcode(Cond[0].getImm()));
  return false;
}

MachineBasicBlock *
LinxV5InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  assert(MI.getDesc().isBranch() && "Unexpected opcode!");
  // The branch target is always the last operand.
  unsigned NumOp = MI.getNumExplicitOperands();
  return MI.getOperand(NumOp - 1).getMBB();
}

unsigned LinxV5InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  unsigned Opcode = MI.getOpcode();

  switch (Opcode) {
  default:
    return get(Opcode).getSize();
  case TargetOpcode::EH_LABEL:
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::KILL:
  case TargetOpcode::DBG_VALUE:
    return 0;
  case TargetOpcode::INLINEASM:
  case TargetOpcode::INLINEASM_BR: {
    const MachineFunction &MF = *MI.getParent()->getParent();
    const auto &TM = static_cast<const LinxV5TargetMachine &>(MF.getTarget());
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              *TM.getMCAsmInfo());
  }
  }
}

bool LinxV5InstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
  const unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default:
    break;
  case LinxV5::PseudoVBXCONST: {
    if (DisableConstCheap) {
      break;
    } else {
      return true;
    }
  }
  case LinxV5::ADDI:
  case LinxV5::ORI:
  case LinxV5::XORI:
  case LinxV5::SUBI:
    return (MI.getOperand(1).isReg() &&
            MI.getOperand(1).getReg() == LinxV5::R0) ||
           (MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0);
  }
  return MI.isAsCheapAsAMove();
}

// Remat instruction hoist is not performed in the MachineLICM phase.
bool LinxV5InstrInfo::isHoistRemat(const MachineInstr &MI) const {
  return false;
}

bool LinxV5InstrInfo::isReallyTriviallyReMaterializable(
    const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case LinxV5::SIMT_ADD:
  case LinxV5::L_ADD_LI:
  case LinxV5::SIMT_ADDI_SCAR:
  case LinxV5::LUI:
    return true;
  default:
    return false;
  }
}

Optional<DestSourcePair>
LinxV5InstrInfo::isCopyInstrImpl(const MachineInstr &MI) const {
  if (MI.isMoveReg())
    return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
  return None;
}

bool LinxV5InstrInfo::verifyInstruction(const MachineInstr &MI,
                                        StringRef &ErrInfo) const {
  const MCInstrInfo *MCII = STI.getInstrInfo();
  MCInstrDesc const &Desc = MCII->get(MI.getOpcode());

  const MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
  auto rejectCrossFileCopy = [&](Register Dst, Register Src) {
    return (isLocalTileReg(MRI, Dst) && isSharedTileReg(MRI, Src)) ||
           (isSharedTileReg(MRI, Dst) && isLocalTileReg(MRI, Src));
  };
  if (MI.isCopy() && MI.getNumExplicitOperands() >= 2 &&
      MI.getOperand(0).isReg() && MI.getOperand(1).isReg() &&
      rejectCrossFileCopy(MI.getOperand(0).getReg(),
                          MI.getOperand(1).getReg())) {
    ErrInfo = "copy between LinxV5 Local tile and Shared tile handle";
    return false;
  }
  if ((MI.getOpcode() == LinxV5::PseudoTCOPY ||
       (MI.getOpcode() == LinxV5::PseudoTCOPYDesc ||
        MI.getOpcode() == LinxV5::PseudoPCOPYDesc)) &&
      MI.getOperand(0).isReg() && MI.getOperand(2).isReg()) {
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(2).getReg();
    if (!isLocalTileReg(MRI, Dst) || !isLocalTileReg(MRI, Src)) {
      ErrInfo = "PseudoTCOPY requires Local tile source and destination";
      return false;
    }
  }

  for (auto &OI : enumerate(Desc.operands())) {
    unsigned OpType = OI.value().OperandType;
    if (OpType >= LinxV5Op::OPERAND_FIRST_LinxV5_IMM &&
        OpType <= LinxV5Op::OPERAND_LAST_LinxV5_IMM) {
      const MachineOperand &MO = MI.getOperand(OI.index());
      if (MO.isImm()) {
        int64_t Imm = MO.getImm();
        bool Ok;
        switch (OpType) {
        case LinxV5Op::OPERAND_UIMM2:
          Ok = isUInt<2>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM3:
          Ok = isUInt<3>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM4:
          Ok = isUInt<4>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM5:
          Ok = isUInt<5>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM5_32:
          Ok = isUInt<5>(Imm-1);
          break;
        case LinxV5Op::OPERAND_UIMM6:
          Ok = isUInt<6>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM7:
          Ok = isUInt<7>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM3:
          Ok = isInt<3>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM4:
          Ok = isInt<4>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM5:
          Ok = isInt<5>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM6:
          Ok = isInt<6>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM7:
          Ok = isInt<7>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM8:
          Ok = isInt<8>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM9:
          Ok = isInt<9>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM10:
          Ok = isInt<10>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM11:
          Ok = isInt<11>(Imm);
          break;
        case LinxV5Op::OPERAND_NOT_SIMM12:
          Ok = !isInt<12>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM12:
          Ok = isUInt<12>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM12:
          Ok = isInt<12>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM16:
          Ok = isUInt<16>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM17:
          Ok = isUInt<17>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM24:
          Ok = isUInt<24>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM64:
          Ok = isInt<64>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM20:
          Ok = isUInt<20>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM20:
          Ok = isInt<20>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM32:
          Ok = isInt<32>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMMLOG2XLEN:
          if (STI.getTargetTriple().isArch64Bit())
            Ok = isUInt<6>(Imm);
          else
            Ok = isUInt<5>(Imm);
          break;
        default:
          llvm_unreachable("Unexpected operand type");
        }
        if (!Ok) {
          ErrInfo = "Invalid immediate";
          return false;
        }
      }
    }
  }

  return true;
}

const TargetRegisterClass *
LinxV5InstrInfo::getRegClass(const MCInstrDesc &MCID, unsigned OpNum,
                             const TargetRegisterInfo *TRI,
                             const MachineFunction &MF) const {
  if (LinxV5II::isTileOp(MCID.TSFlags)) {
    if (OpNum >= MCID.getNumOperands()) {
      return &LinxV5::GRNoR0RegClass;
    }
  }
  return TargetInstrInfo::getRegClass(MCID, OpNum, TRI, MF);
}

bool LinxV5InstrInfo::areMemAccessesTriviallyDisjoint(
    const MachineInstr &MIa, const MachineInstr &MIb) const {
  assert(MIa.mayLoadOrStore() && "MIa must be a load or store.");
  assert(MIb.mayLoadOrStore() && "MIb must be a load or store.");
  return false;
}

std::pair<unsigned, unsigned>
LinxV5InstrInfo::decomposeMachineOperandsTargetFlags(unsigned TF) const {
  const unsigned Mask = LinxV5II::MO_DIRECT_FLAG_MASK;
  return std::make_pair(TF & Mask, TF & ~Mask);
}

ArrayRef<std::pair<unsigned, const char *>>
LinxV5InstrInfo::getSerializableDirectMachineOperandTargetFlags() const {
  using namespace LinxV5II;
  static const std::pair<unsigned, const char *> TargetFlags[] = {
      {MO_CALL, "linx-call"},           {MO_TPCREL, "linx-tpcrel"},
      {MO_TPREL_LO, "linx-tprel-lo"},   {MO_TPREL_HI, "linx-tprel-hi"},
      {MO_TPCREL_LO, "linx-tpcrel-lo"}, {MO_TPCREL_HI, "linx-tpcrel-hi"},
      {MO_TPREL, "linx-tprel"}};
  return makeArrayRef(TargetFlags);
}

unsigned LinxV5InstrInfo::getExtendedOpcode(unsigned OpBefore,
                                            unsigned Ext) const {
  if (Ext == 0) return OpBefore;
  unsigned OpAfter = LinxV5InstrExtensionMap[OpBefore][Ext - 1];
  assert(OpAfter != LinxV5::INSTRUCTION_LIST_END &&
         "Can not get the extended opcode for instruction which has no "
         "extension type!");
  return OpAfter;
}

#define OPPOSITE(Cond, Oppo)                                                   \
  {LinxV5::SETC_##Cond##_BR, LinxV5::SETC_##Oppo##_BR},                        \
      {LinxV5::SETC_##Cond##_SW_BR, LinxV5::SETC_##Oppo##_SW_BR},              \
      {LinxV5::SETC_##Cond##_UW_BR, LinxV5::SETC_##Oppo##_UW_BR}, {            \
    LinxV5::SETC_##Cond##I_BR, LinxV5::SETC_##Oppo##I_BR                       \
  }

static DenseMap<unsigned, unsigned> OppositeConditionInstrMap = {
    OPPOSITE(EQ, NE), OPPOSITE(NE, EQ),   OPPOSITE(LT, GE),
    OPPOSITE(GE, LT), OPPOSITE(LTU, GEU), OPPOSITE(GEU, LTU)
};
#undef OPPOSITE

unsigned LinxV5InstrInfo::getOppositeOpcode(unsigned Op) const {
  auto I = OppositeConditionInstrMap.find(Op);
  assert(I != OppositeConditionInstrMap.end() && "unknown condition opcode!");
  return I->second;
}
