//===-- LinxISABlockify.cpp - Block boundary + T-hand lowering ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "LinxISABaseInfo.h"
#include "LinxISAInstrInfo.h"
#include "LinxISAMachineFunctionInfo.h"
#include "LinxISARegisterInfo.h"
#include "LinxISATileOpcodesV057.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/MC/MCContext.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

using namespace llvm;

#define DEBUG_TYPE "linx-blockify"

namespace {

struct LocalDUInfo {
  unsigned DefCount = 0;
  unsigned UseCount = 0;
  unsigned DefIdx = 0;
  unsigned UseIdx = 0;
  MachineInstr *DefMI = nullptr;
  MachineInstr *UseMI = nullptr;
  unsigned DefOpNo = 0;
  unsigned UseOpNo = 0;
};

enum class TileHand : uint8_t {
  T = 0,
  U = 1,
  M = 2,
  N = 3,
  ACC = 4,
};

struct TileRelRef {
  TileHand Hand = TileHand::T;
  uint8_t Depth = 1; // 1..8
  bool Reuse = false;
};

struct TileMeta {
  uint8_t SizeCode = 0;
  uint8_t DataType = 17;
  int64_t Layout = 0;
  bool HasLayout = false;
};

struct TileQueueState {
  std::array<SmallVector<Register, 8>, 4> Hands;

  bool operator==(const TileQueueState &Other) const {
    return Hands == Other.Hands;
  }
};

enum class TMovMode : uint8_t {
  V2V = 0,
  A2V = 1,
};

enum class TEPLMode : uint8_t {
  VV = 0,
  VS = 1,
  SV = 2,
};

static std::optional<uint64_t> tileSizeCodeToBytes(unsigned SizeCode) {
  if (SizeCode >= 60)
    return std::nullopt;
  return 1ull << (SizeCode + 4u);
}

static std::optional<unsigned> tileBytesToSizeCode(uint64_t Bytes) {
  if (Bytes < 16u || Bytes > 4096u || !isPowerOf2_64(Bytes))
    return std::nullopt;
  return static_cast<unsigned>(Log2_64(Bytes) - 4u);
}

static bool isStrictTileSizeCode(unsigned SizeCode) {
  std::optional<uint64_t> Bytes = tileSizeCodeToBytes(SizeCode);
  return Bytes && *Bytes >= 512u && *Bytes <= 4096u;
}

static void validateStrictTileSizeCode(int64_t SizeCode, StringRef Context) {
  if (SizeCode < 0 || SizeCode > 31 ||
      !isStrictTileSizeCode(static_cast<unsigned>(SizeCode))) {
    report_fatal_error(Twine("Linx: ") + Context +
                       " requires SizeCode in strict 512B..4KB policy");
  }
}

static void validateTileOpcode(int64_t TileOpcode, StringRef Context) {
  if (TileOpcode < 0 || TileOpcode > 1023)
    report_fatal_error(Twine("Linx: ") + Context +
                       " requires TileOpcode in range 0..1023");
}

static bool isWhitelistedTEPLTileOpcode(int64_t TileOpcode) {
  return TileOpcode >= 0 && LinxISA::isCanonicalTEPLTileOpcodeV057(
                                static_cast<unsigned>(TileOpcode));
}

static void validateWhitelistedTEPLTileOpcode(int64_t TileOpcode,
                                            StringRef Context) {
  validateTileOpcode(TileOpcode, Context);
  if (!isWhitelistedTEPLTileOpcode(TileOpcode))
    report_fatal_error(Twine("Linx: ") + Context +
                       " uses a reserved TileOpcode in canonical v0.57");
}

static void validateCubeDimImm(int64_t Dim, StringRef DimName,
                               StringRef Context) {
  if (Dim < 0 || Dim > 131071)
    report_fatal_error(Twine("Linx: ") + Context + " requires " + DimName +
                       " in range 0..131071");
}

static uint64_t dtypeElementBitsForTileCheck(int64_t DType) {
  switch (DType & 0x1f) {
  case 0:  // FP64
  case 16: // INT64
  case 24: // UINT64
    return 64u;
  case 1:  // FP32
  case 17: // INT32
  case 25: // UINT32
    return 32u;
  case 2:  // FP16
  case 6:  // BF16
  case 18: // INT16
  case 26: // UINT16
    return 16u;
  case 3:  // FP8
  case 7:  // FPL8
  case 19: // INT8
  case 27: // UINT8
    return 8u;
  case 11: // FP4
  case 12: // FPL4
  case 20: // INT4
  case 28: // UINT4
    return 4u;
  default:
    // Keep bring-up compatibility for unknown dtype encodings and apply a
    // conservative 32-bit element width for strict byte-budget checks.
    return 32u;
  }
}

static uint64_t requirePositiveDimImm(int64_t Dim, StringRef DimName,
                                      StringRef Context) {
  if (Dim <= 0)
    report_fatal_error(Twine("Linx: ") + Context + " requires " + DimName +
                       " > 0 for tile-byte validation (got " + Twine(Dim) +
                       ")");
  return static_cast<uint64_t>(Dim);
}

static bool isArchivedRawVectorOperandName(StringRef Name) {
  std::string Upper = Name.trim().upper();
  return StringSwitch<bool>(Upper)
      .Cases({"TE", "TF", "TG", "TH"}, true)
      .Cases({"TO1", "TO2", "TO3"}, true)
      .Default(false);
}

static uint64_t computeTileBytesOrDie(StringRef Context, uint64_t Dim0,
                                      uint64_t Dim1, uint64_t Dim2,
                                      uint64_t ElemBits) {
  auto MulOverflowU64 = [](uint64_t A, uint64_t B, uint64_t &Out) {
    if (A == 0 || B == 0) {
      Out = 0;
      return false;
    }
    if (A > (std::numeric_limits<uint64_t>::max() / B))
      return true;
    Out = A * B;
    return false;
  };

  uint64_t ElemCount = 0;
  uint64_t Tmp = 0;
  if (MulOverflowU64(Dim0, Dim1, Tmp) || MulOverflowU64(Tmp, Dim2, ElemCount) ||
      MulOverflowU64(ElemCount, ElemBits, Tmp)) {
    report_fatal_error(Twine("Linx: ") + Context +
                       " tile-byte check overflow while evaluating "
                       "dim0*dim1*dim2*elem_bits");
  }
  if (Tmp > std::numeric_limits<uint64_t>::max() - 7u) {
    report_fatal_error(Twine("Linx: ") + Context +
                       " tile-byte check overflow while rounding bits to bytes");
  }
  return (Tmp + 7u) / 8u;
}

static void validateTileByteBudget(StringRef Context, uint64_t Dim0,
                                   uint64_t Dim1, uint64_t Dim2,
                                   uint64_t ElemBits,
                                   std::optional<uint64_t> SizeCode) {
  const uint64_t Bytes =
      computeTileBytesOrDie(Context, Dim0, Dim1, Dim2, ElemBits);
  constexpr uint64_t StrictMaxBytes = 4096u;
  if (Bytes > StrictMaxBytes) {
    report_fatal_error(Twine("Linx: ") + Context +
                       " tile-byte check failed: bytes=" + Twine(Bytes) +
                       "B (dim0=" + Twine(Dim0) + ", dim1=" + Twine(Dim1) +
                       ", dim2=" + Twine(Dim2) +
                       ", elem_bits=" + Twine(ElemBits) +
                       ") exceeds strict max 4096B. Shrink dimensions or "
                       "element width.");
  }

  if (SizeCode) {
    std::optional<uint64_t> LimitBytes = tileSizeCodeToBytes(*SizeCode);
    if (!LimitBytes) {
      report_fatal_error(Twine("Linx: ") + Context +
                         " internal error: invalid SizeCode while checking "
                         "tile-byte budget");
    }
    if (Bytes > *LimitBytes) {
      report_fatal_error(Twine("Linx: ") + Context +
                         " tile-byte check failed: bytes=" + Twine(Bytes) +
                         "B (dim0=" + Twine(Dim0) + ", dim1=" + Twine(Dim1) +
                         ", dim2=" + Twine(Dim2) +
                         ", elem_bits=" + Twine(ElemBits) +
                         ") exceeds descriptor limit " + Twine(*LimitBytes) +
                         "B (SizeCode=" + Twine(*SizeCode) +
                         "). Shrink dimensions/element width or increase "
                         "SizeCode.");
    }
  }
}

static unsigned tileHandBase(TileHand Hand) {
  switch (Hand) {
  case TileHand::T:
    return 0;
  case TileHand::U:
    return 16;
  case TileHand::M:
    return 32;
  case TileHand::N:
    return 48;
  case TileHand::ACC:
    return 32;
  }
  llvm_unreachable("invalid tile hand");
}

static unsigned physicalTileHandIndex(unsigned TileId) {
  if (TileId >= 32)
    report_fatal_error("Linx: physical tile register id must be in [0,31]");
  return TileId / 8u;
}

static unsigned tileRegIdFromReg(const TargetRegisterInfo &TRI, Register Reg);

static unsigned encodeTileQueueSource(const TargetRegisterInfo &TRI,
                                      const TileQueueState &State,
                                      Register Reg, StringRef Context) {
  const unsigned TileId = tileRegIdFromReg(TRI, Reg);
  const unsigned Hand = physicalTileHandIndex(TileId);
  const auto &Queue = State.Hands[Hand];
  auto It = llvm::find(Queue, Reg);
  if (It == Queue.end())
    report_fatal_error(Twine("Linx: cannot prove tile queue rank for ") +
                       Context);
  const unsigned Rank = static_cast<unsigned>(It - Queue.begin()) + 1u;
  if (Rank > 8u)
    report_fatal_error("Linx: tile queue rank exceeds architectural depth 8");
  return Hand * 16u + Rank - 1u;
}

static void consumeTileQueueValue(const TargetRegisterInfo &TRI,
                                  TileQueueState &State, Register Reg,
                                  StringRef Context) {
  const unsigned Hand = physicalTileHandIndex(tileRegIdFromReg(TRI, Reg));
  auto &Queue = State.Hands[Hand];
  auto It = llvm::find(Queue, Reg);
  if (It == Queue.end())
    report_fatal_error(Twine("Linx: cannot consume absent tile value for ") +
                       Context);
  Queue.erase(It);
}

static void pushTileQueueValue(const TargetRegisterInfo &TRI,
                               TileQueueState &State, Register Reg) {
  const unsigned Hand = physicalTileHandIndex(tileRegIdFromReg(TRI, Reg));
  auto &Queue = State.Hands[Hand];
  if (auto It = llvm::find(Queue, Reg); It != Queue.end())
    Queue.erase(It);
  Queue.insert(Queue.begin(), Reg);
  if (Queue.size() > 8u)
    report_fatal_error("Linx: tile queue live depth exceeds architectural limit 8");
}

static TileRelRef tileRelRefFromId(unsigned TileId, bool Reuse = false) {
  TileRelRef Ref;
  if (TileId < 8) {
    Ref.Hand = TileHand::T;
  } else if (TileId < 16) {
    Ref.Hand = TileHand::U;
  } else if (TileId < 24) {
    Ref.Hand = TileHand::M;
  } else {
    Ref.Hand = TileHand::N;
  }
  Ref.Depth = static_cast<uint8_t>((TileId & 0x7u) + 1u);
  Ref.Reuse = Reuse;
  return Ref;
}

static unsigned tileIdFromRelRef(const TileRelRef &Ref) {
  if (Ref.Hand == TileHand::ACC)
    report_fatal_error("Linx: ACC is not encodable as a source tile relref");
  if (Ref.Depth < 1 || Ref.Depth > 8)
    report_fatal_error("Linx: invalid tile relref depth (expected 1..8)");
  return tileHandBase(Ref.Hand) + static_cast<unsigned>(Ref.Depth - 1u);
}

static unsigned dstTileFieldFromHand(TileHand Hand) {
  switch (Hand) {
  case TileHand::T:
    return 0;
  case TileHand::U:
    return 1;
  case TileHand::M:
    return 2;
  case TileHand::N:
    return 3;
  case TileHand::ACC:
    return 4;
  }
  llvm_unreachable("invalid tile hand");
}

static unsigned dstTileFieldFromRelRef(const TileRelRef &Ref) {
  return dstTileFieldFromHand(Ref.Hand);
}

static unsigned tileRegIdFromReg(const TargetRegisterInfo &TRI, Register Reg) {
  if (!Reg || !Reg.isPhysical() || !LinxISA::TILERegClass.contains(Reg))
    report_fatal_error("Linx: expected physical tile register");
  return TRI.getEncodingValue(Reg) & 0x1fu;
}

static bool isMarkerInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::CBSTART_STD:
  case LinxISA::BSTART_STD_FALL:
  case LinxISA::BSTART_STD_DIRECT:
  case LinxISA::BSTART_STD_COND:
  case LinxISA::BSTART_STD_CALL:
  case LinxISA::BSTART_STD_IND:
  case LinxISA::BSTART_STD_ICALL:
  case LinxISA::BSTART_STD_RET:
  case LinxISA::BSTART_TMA:
  case LinxISA::BSTART_CUBE:
  case LinxISA::BSTART_TEPL:
  case LinxISA::BSTART_VPAR:
  case LinxISA::BSTART_VSEQ:
  case LinxISA::BSTART_MPAR:
  case LinxISA::BSTART_MSEQ:
  case LinxISA::BSTOP:
    return true;
  default:
    return false;
  }
}

static bool inlineAsmHasExplicitBlockBoundary(const MachineInstr &MI) {
  if (!MI.isInlineAsm() ||
      MI.getNumOperands() <= InlineAsm::MIOp_AsmString ||
      !MI.getOperand(InlineAsm::MIOp_AsmString).isSymbol())
    return false;

  StringRef Asm(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName());
  return Asm.contains_insensitive("bstop") ||
         Asm.contains_insensitive("bstart") ||
         Asm.contains_insensitive("acrc");
}

static bool isSimtBodyHeaderOpcode(unsigned Opc) {
  switch (Opc) {
  case LinxISA::BSTART_MPAR:
  case LinxISA::BSTART_MSEQ:
  case LinxISA::BSTART_VPAR:
  case LinxISA::BSTART_VSEQ:
    return true;
  default:
    return false;
  }
}

static bool isHeaderDescriptorOpcode(unsigned Opc) {
  switch (Opc) {
  case LinxISA::B_TEXT:
  case LinxISA::B_ARG:
  case LinxISA::B_CATR:
  case LinxISA::B_DATR:
  case LinxISA::B_DIM_LB0:
  case LinxISA::B_DIM_LB1:
  case LinxISA::B_DIM_LB2:
  case LinxISA::C_B_DIMI:
  case LinxISA::B_IOR:
  case LinxISA::B_IOT_SIZE_G0:
  case LinxISA::B_IOT_SIZE_G1:
    return true;
  default:
    return false;
  }
}

static bool isFrameMacroInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::FENTRY:
  case LinxISA::FEXIT:
  case LinxISA::FRET_RA:
  case LinxISA::FRET_STK:
  case LinxISA::MCOPY:
  case LinxISA::MSET:
    return true;
  default:
    return false;
  }
}

static bool isTilePseudoInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::PSEUDO_TMA_TLOAD:
  case LinxISA::PSEUDO_TMA_TLOAD_ANY:
  case LinxISA::PSEUDO_TMA_TLOAD_DESC:
  case LinxISA::PSEUDO_TMA_TSTORE:
  case LinxISA::PSEUDO_TMA_TSTORE_DESC:
  case LinxISA::PSEUDO_TMA_TMOV:
  case LinxISA::PSEUDO_CUBE_MAMULB:
  case LinxISA::PSEUDO_CUBE_MAMULB_ACC:
  case LinxISA::PSEUDO_CUBE_ACCCVT:
  case LinxISA::PSEUDO_TEPL_UNARY:
  case LinxISA::PSEUDO_TEPL_BINARY:
  case LinxISA::PSEUDO_TEPL_BINARY_SCALAR:
  case LinxISA::PSEUDO_TEPL_SPLAT:
  case LinxISA::PSEUDO_VPAR_TADD:
  case LinxISA::PSEUDO_VPAR_TSUB:
  case LinxISA::PSEUDO_VTILE_ADD:
  case LinxISA::PSEUDO_VTILE_SUB:
    return true;
  default:
    return false;
  }
}

static bool isVBlockPseudoInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::PSEUDO_VBLOCK_LAUNCH:
  case LinxISA::PSEUDO_VBLOCK_LAUNCH_DYN1:
    return true;
  default:
    return false;
  }
}

static bool isTileBlockStartInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::BSTART_TMA:
  case LinxISA::BSTART_CUBE:
  case LinxISA::BSTART_TEPL:
    return true;
  default:
    return false;
  }
}

static bool isStandaloneFrameMacroBlock(const MachineBasicBlock &MBB) {
  const MachineInstr *MacroMI = nullptr;
  for (const MachineInstr &MI : MBB) {
    if (MI.isDebugInstr() || MI.isCFIInstruction())
      continue;
    if (isMarkerInstr(MI))
      continue;
    if (!isFrameMacroInstr(MI))
      return false;
    if (MacroMI)
      return false;
    MacroMI = &MI;
  }
  return MacroMI != nullptr;
}

static bool canEncodeShiftedSignedImm(int64_t Imm, unsigned BaseBits) {
  for (unsigned Sh = 0; Sh < 32; ++Sh) {
    int64_t Pow = (1LL << Sh);
    if (Imm % Pow != 0)
      continue;
    int64_t Base = Imm / Pow;
    if (isIntN(BaseBits, Base))
      return true;
  }
  return false;
}

static bool canEncodeShiftedUnsignedImm(int64_t Imm, unsigned BaseBits) {
  if (Imm < 0)
    return false;
  uint64_t UImm = static_cast<uint64_t>(Imm);
  for (unsigned Sh = 0; Sh < 32; ++Sh) {
    uint64_t Pow = (1ULL << Sh);
    if (UImm % Pow != 0)
      continue;
    uint64_t Base = UImm / Pow;
    if (isUIntN(BaseBits, Base))
      return true;
  }
  return false;
}

static Register getTQueueUseReg(unsigned Index) {
  switch (Index) {
  case 1:
    return LinxISA::T1;
  case 2:
    return LinxISA::T2;
  case 3:
    return LinxISA::T3;
  case 4:
    return LinxISA::T4;
  default:
    return Register();
  }
}

static Register getUQueueUseReg(unsigned Index) {
  switch (Index) {
  case 1:
    return LinxISA::U1;
  case 2:
    return LinxISA::U2;
  case 3:
    return LinxISA::U3; // u#3
  case 4:
    return LinxISA::U4; // u#4
  default:
    return Register();
  }
}

class LinxISABlockify : public MachineFunctionPass {
public:
  static char ID;

  LinxISABlockify() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "Linx Blockify"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII = *MF.getSubtarget().getInstrInfo();
    const auto &TRI = *MF.getSubtarget().getRegisterInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();

    const BitVector Reserved = TRI.getReservedRegs(MF);
    bool Changed = false;

    // Per-function decoupled-body stubs used by block headers.
    // - Empty body: tile headers that execute via descriptor-only semantics.
    // - VBlock body: generic SIMT vector body used by autovec vblock launch.
    MachineBasicBlock *EmptyBodyBB = nullptr;
    MachineBasicBlock *VBlockBodyBB = nullptr;
    MCSymbol *VBlockBodySym = nullptr;
    MachineBasicBlock *VTileAddBodyBB = nullptr;
    MCSymbol *VTileAddBodySym = nullptr;
    MachineBasicBlock *VTileSubBodyBB = nullptr;
    MCSymbol *VTileSubBodySym = nullptr;
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        if (MI.getOpcode() != TargetOpcode::EH_LABEL || MI.getNumOperands() < 1)
          continue;
        const MachineOperand &MO = MI.getOperand(0);
        if (!MO.isMCSymbol())
          continue;
        MCSymbol *Sym = MO.getMCSymbol();
        if (!Sym)
          continue;
        if (Sym->getName().starts_with(".__linx_empty_body.")) {
          EmptyBodyBB = &MBB;
        } else if (Sym->getName().starts_with(".__linx_vblock_body.")) {
          VBlockBodyBB = &MBB;
          VBlockBodySym = Sym;
        } else if (Sym->getName().starts_with(".__linx_vtile_add_body.")) {
          VTileAddBodyBB = &MBB;
          VTileAddBodySym = Sym;
        } else if (Sym->getName().starts_with(".__linx_vtile_sub_body.")) {
          VTileSubBodyBB = &MBB;
          VTileSubBodySym = Sym;
          break;
        }
      }
      if (EmptyBodyBB && VBlockBodyBB && VTileAddBodyBB && VTileSubBodyBB)
        break;
    }

    SmallPtrSet<MachineBasicBlock *, 8> DecoupledBodyBBs;
    if (EmptyBodyBB)
      DecoupledBodyBBs.insert(EmptyBodyBB);
    if (VBlockBodyBB)
      DecoupledBodyBBs.insert(VBlockBodyBB);
    if (VTileAddBodyBB)
      DecoupledBodyBBs.insert(VTileAddBodyBB);
    if (VTileSubBodyBB)
      DecoupledBodyBBs.insert(VTileSubBodyBB);

    struct ParsedVReg {
      unsigned Code = 0;
      unsigned SrcRType = 3; // default: no suffix modifier
      unsigned Shamt = 0;
    };

    struct VecPipeCursorState {
      unsigned NextVt = 1;
      unsigned NextVu = 1;
      unsigned NextVm = 1;
      unsigned NextVn = 1;
    };

    auto toUpperStr = [](StringRef S) -> std::string {
      std::string Out;
      Out.reserve(S.size());
      for (char C : S)
        Out.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(C))));
      return Out;
    };

    auto parseSrcRTypeSuffix = [&](StringRef Suffix) -> std::optional<unsigned> {
      std::string Up = toUpperStr(Suffix);
      if (Up == "SW")
        return 0u;
      if (Up == "UW" || Up == "UH")
        return 1u;
      if (Up == "NEG" || Up == "NOT")
        return 2u;
      return std::nullopt;
    };

    auto parseRegCode = [&](StringRef Name) -> std::optional<unsigned> {
      StringRef N = Name.trim();
      if (N.size() >= 2 && (N[0] == 'r' || N[0] == 'R') &&
          std::isdigit(static_cast<unsigned char>(N[1]))) {
        N = N.drop_front();
        unsigned V = 0;
        if (!N.getAsInteger(10, V) && V < 32)
          return V;
        return std::nullopt;
      }

      std::string Upper = toUpperStr(N);

      auto parsePrefixedIndex = [&](StringRef Prefix, unsigned Class,
                                    unsigned MaxIndex)
          -> std::optional<unsigned> {
        StringRef U(Upper);
        if (!U.starts_with(Prefix))
          return std::nullopt;
        StringRef Tail = U.drop_front(Prefix.size());
        if (Tail.empty())
          return std::nullopt;
        unsigned Index = 0;
        if (Tail.getAsInteger(10, Index) || Index > MaxIndex)
          return std::nullopt;
        return (Class << 5) | (Index & 0x1fu);
      };

      auto parseVecQueue = [&](StringRef Prefix, unsigned Class,
                               unsigned MaxIndex) -> std::optional<unsigned> {
        StringRef U(Upper);
        if (!U.starts_with(Prefix))
          return std::nullopt;
        StringRef Tail = U.drop_front(Prefix.size());
        unsigned Index = 0;
        if (Tail.empty()) {
          Index = 0;
        } else {
          if (!Tail.consume_front("#"))
            return std::nullopt;
          if (Tail.getAsInteger(10, Index) || Index == 0 || Index > MaxIndex)
            return std::nullopt;
        }
        return (Class << 5) | (Index & 0x1fu);
      };

      if (auto V = parsePrefixedIndex("RI", /*Class=*/1, /*MaxIndex=*/31))
        return *V;
      if (auto V = parsePrefixedIndex("LC", /*Class=*/3, /*MaxIndex=*/2))
        return *V;
      if (auto V = parseVecQueue("VT", /*Class=*/4, /*MaxIndex=*/31))
        return *V;
      if (auto V = parseVecQueue("VU", /*Class=*/5, /*MaxIndex=*/31))
        return *V;
      if (auto V = parseVecQueue("VM", /*Class=*/6, /*MaxIndex=*/31))
        return *V;
      if (auto V = parseVecQueue("VN", /*Class=*/7, /*MaxIndex=*/31))
        return *V;

      if (Upper == "TA")
        return (8u << 5) | 0u;
      if (Upper == "TB")
        return (8u << 5) | 1u;
      if (Upper == "TC")
        return (8u << 5) | 2u;
      if (Upper == "TD")
        return (8u << 5) | 3u;
      if (Upper == "TO")
        return (8u << 5) | 4u;
      if (Upper == "TS")
        return (8u << 5) | 5u;

      if (Upper == "ZERO")
        return 0u;
      if (Upper == "SP")
        return 1u;
      if (Upper == "A0")
        return 2u;
      if (Upper == "A1")
        return 3u;
      if (Upper == "A2")
        return 4u;
      if (Upper == "A3")
        return 5u;
      if (Upper == "A4")
        return 6u;
      if (Upper == "A5")
        return 7u;
      if (Upper == "A6")
        return 8u;
      if (Upper == "A7")
        return 9u;
      if (Upper == "RA")
        return 10u;
      if (Upper == "S0")
        return 11u;
      if (Upper == "S1")
        return 12u;
      if (Upper == "S2")
        return 13u;
      if (Upper == "S3")
        return 14u;
      if (Upper == "S4")
        return 15u;
      if (Upper == "S5")
        return 16u;
      if (Upper == "S6")
        return 17u;
      if (Upper == "S7")
        return 18u;
      if (Upper == "S8")
        return 19u;
      if (Upper == "X0")
        return 20u;
      if (Upper == "X1")
        return 21u;
      if (Upper == "X2")
        return 22u;
      if (Upper == "X3")
        return 23u;
      if (Upper == "T#1")
        return 24u;
      if (Upper == "T#2")
        return 25u;
      if (Upper == "T#3")
        return 26u;
      if (Upper == "T#4")
        return 27u;
      if (Upper == "U#1")
        return 28u;
      if (Upper == "U#2")
        return 29u;
      if (Upper == "U#3" || Upper == "U")
        return 30u;
      if (Upper == "U#4" || Upper == "T")
        return 31u;
      if (Upper == "P")
        return 92u;

      return std::nullopt;
    };

    auto parseVecRegToken = [&](StringRef Token) -> std::optional<ParsedVReg> {
      StringRef T = Token.trim();
      if (T.empty())
        return std::nullopt;

      ParsedVReg Out;
      if (size_t ShiftPos = T.find("<<"); ShiftPos != StringRef::npos) {
        StringRef Sh = T.drop_front(ShiftPos + 2).trim();
        unsigned Shamt = 0;
        if (Sh.getAsInteger(10, Shamt))
          return std::nullopt;
        Out.Shamt = Shamt;
        T = T.take_front(ShiftPos).trim();
      }

      StringRef Base = T;
      StringRef Suffix;
      if (size_t Dot = T.rfind('.'); Dot != StringRef::npos) {
        Base = T.take_front(Dot).trim();
        Suffix = T.drop_front(Dot + 1).trim();
      }
      if (Base.ends_with_insensitive(".reuse"))
        Base = Base.drop_back(strlen(".reuse")).trim();
      if (isArchivedRawVectorOperandName(Base))
        report_fatal_error(
            "Linx blockify: archived raw vector operand name is not allowed "
            "in canonical v0.4; use TA/TB/TC/TD/TO/TS");

      auto RegCode = parseRegCode(Base);
      if (!RegCode)
        return std::nullopt;
      Out.Code = *RegCode;

      if (!Suffix.empty()) {
        if (auto SrcRType = parseSrcRTypeSuffix(Suffix))
          Out.SrcRType = *SrcRType;
      }
      return Out;
    };

    auto splitCSV = [](StringRef S, SmallVectorImpl<StringRef> &Out) {
      SmallVector<StringRef, 8> Raw;
      S.split(Raw, ',', -1, false);
      for (StringRef R : Raw) {
        R = R.trim();
        if (!R.empty())
          Out.push_back(R);
      }
    };

    auto parseMemTriple = [&](StringRef MemExpr, unsigned WantLaneShamt,
                              ParsedVReg &Base, ParsedVReg &Index) -> bool {
      const size_t L = MemExpr.find('[');
      const size_t R = MemExpr.rfind(']');
      if (L == StringRef::npos || R == StringRef::npos || R <= L)
        return false;
      StringRef Inside = MemExpr.slice(L + 1, R).trim();
      SmallVector<StringRef, 4> Parts;
      splitCSV(Inside, Parts);
      if (Parts.size() != 3)
        return false;
      auto BaseOp = parseVecRegToken(Parts[0]);
      auto LaneOp = parseVecRegToken(Parts[1]);
      auto IndexOp = parseVecRegToken(Parts[2]);
      if (!BaseOp || !LaneOp || !IndexOp)
        return false;
      // Bring-up contract: vector body memory lanes are lc0 shifted by the
      // element size.
      const unsigned WantLc0Code = (3u << 5) | 0u;
      if (LaneOp->Code != WantLc0Code || LaneOp->Shamt != WantLaneShamt)
        return false;
      Base = *BaseOp;
      Index = *IndexOp;
      return true;
    };

    auto parseMemImm = [&](StringRef MemExpr, unsigned WantLaneShamt,
                           ParsedVReg &Base, int64_t &Imm) -> bool {
      const size_t L = MemExpr.find('[');
      const size_t R = MemExpr.rfind(']');
      if (L == StringRef::npos || R == StringRef::npos || R <= L)
        return false;
      StringRef Inside = MemExpr.slice(L + 1, R).trim();
      SmallVector<StringRef, 4> Parts;
      splitCSV(Inside, Parts);
      if (Parts.size() != 3)
        return false;
      auto BaseOp = parseVecRegToken(Parts[0]);
      auto LaneOp = parseVecRegToken(Parts[1]);
      if (!BaseOp || !LaneOp)
        return false;
      const unsigned WantLc0Code = (3u << 5) | 0u;
      if (LaneOp->Code != WantLc0Code || LaneOp->Shamt != WantLaneShamt)
        return false;
      if (Parts[2].getAsInteger(/*Radix=*/0, Imm))
        return false;
      Base = *BaseOp;
      return true;
    };

    auto normalizeLabel = [&](StringRef Label) -> std::string {
      std::string Out;
      StringRef L = Label.trim();
      if (L.ends_with(":"))
        L = L.drop_back().trim();
      Out.reserve(L.size());
      for (char C : L) {
        if (std::isalnum(static_cast<unsigned char>(C)) || C == '_' || C == '.')
          Out.push_back(C);
      }
      return Out;
    };

    auto getHeadQueueClass = [&](StringRef DstPart) -> std::optional<unsigned> {
      StringRef Base = DstPart.trim();
      if (size_t Dot = Base.rfind('.'); Dot != StringRef::npos)
        Base = Base.take_front(Dot).trim();
      std::string Upper = toUpperStr(Base);
      if (Upper == "VT")
        return 4u;
      if (Upper == "VU")
        return 5u;
      if (Upper == "VM")
        return 6u;
      if (Upper == "VN")
        return 7u;
      return std::nullopt;
    };

    auto noteExplicitVecPipeDest = [&](StringRef DstPart,
                                       VecPipeCursorState &PipeState) {
      StringRef Base = DstPart.trim();
      if (size_t Dot = Base.rfind('.'); Dot != StringRef::npos)
        Base = Base.take_front(Dot).trim();
      std::string Upper = toUpperStr(Base);
      auto bumpCounter = [&](StringRef Prefix, unsigned &NextIndex) {
        StringRef UpperRef(Upper);
        if (!UpperRef.starts_with(Prefix))
          return;
        StringRef Tail = UpperRef.drop_front(Prefix.size());
        if (!Tail.consume_front("#"))
          return;
        unsigned Index = 0;
        if (Tail.getAsInteger(10, Index) || Index == 0)
          return;
        NextIndex = std::max(NextIndex, Index + 1);
      };
      bumpCounter("VT", PipeState.NextVt);
      bumpCounter("VU", PipeState.NextVu);
      bumpCounter("VM", PipeState.NextVm);
      bumpCounter("VN", PipeState.NextVn);
    };

    auto assignVecPipeDstCode = [&](StringRef DstPart, unsigned ParsedCode,
                                    VecPipeCursorState &PipeState)
        -> unsigned {
      auto nextCode = [&](unsigned Class, unsigned &NextIndex) {
        return (Class << 5) | (NextIndex++ & 0x1fu);
      };
      if (auto HeadClass = getHeadQueueClass(DstPart)) {
        switch (*HeadClass) {
        case 4:
          return nextCode(*HeadClass, PipeState.NextVt);
        case 5:
          return nextCode(*HeadClass, PipeState.NextVu);
        case 6:
          return nextCode(*HeadClass, PipeState.NextVm);
        case 7:
          return nextCode(*HeadClass, PipeState.NextVn);
        default:
          llvm_unreachable("unexpected vector head queue class");
        }
      }
      noteExplicitVecPipeDest(DstPart, PipeState);
      return ParsedCode;
    };

    auto emitVectorBodyLine =
        [&](MachineBasicBlock &BodyBB, StringRef RawLine, StringRef CtxName,
            VecPipeCursorState &PipeState,
            function_ref<MCSymbol *(StringRef)> LookupLabelSym) {
      StringRef Line = RawLine;
      if (size_t Semi = Line.find(';'); Semi != StringRef::npos)
        Line = Line.take_front(Semi);
      Line = Line.trim();
      if (Line.empty())
        return;

      auto fail = [&](StringRef Msg) -> void {
        SmallString<256> Full;
        raw_svector_ostream OS(Full);
        OS << "Linx blockify: " << Msg << " in " << CtxName << ": '" << Line
           << "'";
        report_fatal_error(OS.str());
      };

      if (Line.ends_with(":")) {
        std::string Label = normalizeLabel(Line);
        if (Label.empty())
          fail("invalid vector body label");
        MCSymbol *Sym = LookupLabelSym(Label);
        if (!Sym)
          fail("undefined vector body label");
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(TargetOpcode::EH_LABEL))
            .addSym(Sym);
        return;
      }

      if (Line.equals_insensitive("C.BSTOP") || Line.equals_insensitive("BSTOP")) {
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(LinxISA::BSTOP));
        return;
      }

      size_t SpacePos = Line.find(' ');
      if (SpacePos == StringRef::npos)
        fail("invalid vector body statement");
      StringRef Head = Line.take_front(SpacePos).trim();
      StringRef Rest = Line.drop_front(SpacePos + 1).trim();
      if (Head.empty() || Rest.empty())
        fail("invalid vector body statement");

      if (Head.equals_insensitive("j")) {
        std::string Label = normalizeLabel(Rest);
        if (Label.empty())
          fail("missing label in j");
        MCSymbol *Sym = LookupLabelSym(Label);
        if (!Sym)
          fail("undefined vector body label");
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(LinxISA::PSEUDO_V_J))
            .addSym(Sym);
        return;
      }

      if (Head.equals_insensitive("b.z") || Head.equals_insensitive("b.nz")) {
        std::string Label = normalizeLabel(Rest);
        if (Label.empty())
          fail("missing label in b.z/b.nz");
        MCSymbol *Sym = LookupLabelSym(Label);
        if (!Sym)
          fail("undefined vector body label");
        const unsigned Opc = Head.equals_insensitive("b.z")
                                 ? LinxISA::PSEUDO_V_B_Z
                                 : LinxISA::PSEUDO_V_B_NZ;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc)).addSym(Sym);
        return;
      }

      if (Head.equals_insensitive("b.eq") || Head.equals_insensitive("b.ne") ||
          Head.equals_insensitive("b.lt") || Head.equals_insensitive("b.ge") ||
          Head.equals_insensitive("b.ltu") || Head.equals_insensitive("b.geu")) {
        SmallVector<StringRef, 4> Ops;
        splitCSV(Rest, Ops);
        if (Ops.size() != 3)
          fail("expected 'b.<cc> SrcL, SrcR, label'");
        auto SrcL = parseVecRegToken(Ops[0]);
        auto SrcR = parseVecRegToken(Ops[1]);
        std::string Label = normalizeLabel(Ops[2]);
        if (!SrcL || !SrcR || Label.empty())
          fail("failed to parse operands for branch");
        MCSymbol *Sym = LookupLabelSym(Label);
        if (!Sym)
          fail("undefined vector body label");

        unsigned Opc = 0;
        if (Head.equals_insensitive("b.eq"))
          Opc = LinxISA::PSEUDO_V_B_EQ;
        else if (Head.equals_insensitive("b.ne"))
          Opc = LinxISA::PSEUDO_V_B_NE;
        else if (Head.equals_insensitive("b.lt"))
          Opc = LinxISA::PSEUDO_V_B_LT;
        else if (Head.equals_insensitive("b.ge"))
          Opc = LinxISA::PSEUDO_V_B_GE;
        else if (Head.equals_insensitive("b.ltu"))
          Opc = LinxISA::PSEUDO_V_B_LTU;
        else if (Head.equals_insensitive("b.geu"))
          Opc = LinxISA::PSEUDO_V_B_GEU;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(SrcL->Code)
            .addImm(SrcR->Code)
            .addSym(Sym);
        return;
      }

      auto parseArrow = [&](StringRef Expr, StringRef &SrcPart,
                            ParsedVReg &Dst) -> bool {
        size_t Arrow = Expr.find("->");
        if (Arrow == StringRef::npos)
          return false;
        SrcPart = Expr.take_front(Arrow).trim();
        StringRef DstPart = Expr.drop_front(Arrow + 2).trim();
        if (size_t Comma = DstPart.find(','); Comma != StringRef::npos)
          DstPart = DstPart.take_front(Comma).trim();
        auto DstOp = parseVecRegToken(DstPart);
        if (!DstOp)
          return false;
        Dst = *DstOp;
        Dst.Code = assignVecPipeDstCode(DstPart, DstOp->Code, PipeState);
        return true;
      };

      auto parseArrowDstCode = [&](StringRef Expr, StringRef &SrcPart,
                                   unsigned &DstCode) -> bool {
        size_t Arrow = Expr.find("->");
        if (Arrow == StringRef::npos)
          return false;
        SrcPart = Expr.take_front(Arrow).trim();
        StringRef DstPart = Expr.drop_front(Arrow + 2).trim();
        if (size_t Comma = DstPart.find(','); Comma != StringRef::npos)
          DstPart = DstPart.take_front(Comma).trim();
        auto DstOp = parseVecRegToken(DstPart);
        if (!DstOp)
          return false;
        DstCode = assignVecPipeDstCode(DstPart, DstOp->Code, PipeState);
        return true;
      };
    
      if (Head.equals_insensitive("c.movr")) {
        StringRef SrcPart;
        ParsedVReg Dst;
        if (!parseArrow(Rest, SrcPart, Dst))
          fail("expected '->Dst' in c.movr");
        SmallVector<StringRef, 2> Ops;
        splitCSV(SrcPart, Ops);
        if (Ops.size() != 1)
          fail("expected one source operand for c.movr");
        auto SrcL = parseVecRegToken(Ops[0]);
        if (!SrcL)
          fail("failed to parse source operand for c.movr");
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(),
                TII.get(LinxISA::PSEUDO_V_C_MOVR))
            .addImm(Dst.Code)
            .addImm(SrcL->Code);
        return;
      }

      if (Head.equals_insensitive("v.add") || Head.equals_insensitive("v.sub")) {
        StringRef SrcPart;
        ParsedVReg Dst;
        if (!parseArrow(Rest, SrcPart, Dst))
	          fail("expected '->Dst' in vector ALU op");
        SmallVector<StringRef, 4> Ops;
        splitCSV(SrcPart, Ops);
        if (Ops.size() != 2)
          fail("expected two source operands for vector ALU op");
        auto SrcL = parseVecRegToken(Ops[0]);
        auto SrcR = parseVecRegToken(Ops[1]);
        if (!SrcL || !SrcR)
          fail("failed to parse source operands for vector ALU op");
        const unsigned Opc = Head.equals_insensitive("v.add")
                                 ? LinxISA::PSEUDO_V_ADD
                                 : LinxISA::PSEUDO_V_SUB;
        const unsigned SrcRType = (SrcR->SrcRType == 3u) ? 0u : SrcR->SrcRType;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(Dst.Code)
            .addImm(SrcL->Code)
            .addImm(SrcR->Code)
            .addImm(SrcRType)
            .addImm(SrcR->Shamt);
	        return;
	      }

	      if (Head.equals_insensitive("v.mul")) {
	        StringRef SrcPart;
	        ParsedVReg Dst;
	        if (!parseArrow(Rest, SrcPart, Dst))
	          fail("expected '->Dst' in vector mul op");
	        SmallVector<StringRef, 4> Ops;
	        splitCSV(SrcPart, Ops);
	        if (Ops.size() != 2)
	          fail("expected two source operands for vector mul op");
	        auto SrcL = parseVecRegToken(Ops[0]);
	        auto SrcR = parseVecRegToken(Ops[1]);
	        if (!SrcL || !SrcR)
	          fail("failed to parse source operands for vector mul op");
	        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(LinxISA::PSEUDO_V_MUL))
	            .addImm(Dst.Code)
	            .addImm(SrcL->Code)
	            .addImm(SrcR->Code);
	        return;
	      }

	      if (Head.equals_insensitive("v.fadd") || Head.equals_insensitive("v.fsub") ||
	          Head.equals_insensitive("v.fmul") || Head.equals_insensitive("v.fdiv")) {
	        StringRef SrcPart;
	        ParsedVReg Dst;
        if (!parseArrow(Rest, SrcPart, Dst))
          fail("expected '->Dst' in vector FP op");
        SmallVector<StringRef, 4> Ops;
        splitCSV(SrcPart, Ops);
        if (Ops.size() != 2)
          fail("expected two source operands for vector FP op");
        auto SrcL = parseVecRegToken(Ops[0]);
        auto SrcR = parseVecRegToken(Ops[1]);
        if (!SrcL || !SrcR)
          fail("failed to parse source operands for vector FP op");
        unsigned Opc = LinxISA::PSEUDO_V_FADD;
        if (Head.equals_insensitive("v.fsub"))
          Opc = LinxISA::PSEUDO_V_FSUB;
        else if (Head.equals_insensitive("v.fmul"))
          Opc = LinxISA::PSEUDO_V_FMUL;
        else if (Head.equals_insensitive("v.fdiv"))
          Opc = LinxISA::PSEUDO_V_FDIV;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(Dst.Code)
            .addImm(SrcL->Code)
            .addImm(SrcR->Code);
	        return;
	      }

	      if (Head.equals_insensitive("v.fabs") || Head.equals_insensitive("v.fsqrt")) {
	        StringRef SrcPart;
	        ParsedVReg Dst;
	        if (!parseArrow(Rest, SrcPart, Dst))
	          fail("expected '->Dst' in vector FP unop");
	        SmallVector<StringRef, 2> Ops;
	        splitCSV(SrcPart, Ops);
	        if (Ops.size() != 1)
	          fail("expected one source operand for vector FP unop");
	        auto SrcL = parseVecRegToken(Ops[0]);
	        if (!SrcL)
	          fail("failed to parse source operand for vector FP unop");
	        const unsigned Opc = Head.equals_insensitive("v.fabs")
	                                 ? LinxISA::PSEUDO_V_FABS
	                                 : LinxISA::PSEUDO_V_FSQRT;
	        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
	            .addImm(Dst.Code)
	            .addImm(SrcL->Code);
	        return;
	      }

      if (Head.starts_with_insensitive("v.icvtf.")) {
        auto parseVecIntSrcType = [&](StringRef Suffix)
            -> std::optional<unsigned> {
          if (Suffix.equals_insensitive("sd"))
            return 0u;
          if (Suffix.equals_insensitive("sw"))
            return 1u;
          if (Suffix.equals_insensitive("sh"))
            return 2u;
          if (Suffix.equals_insensitive("sb"))
            return 3u;
          return std::nullopt;
        };
        auto parseVecFpDstType = [&](StringRef Suffix)
            -> std::optional<unsigned> {
          if (Suffix.equals_insensitive("fd"))
            return 0u;
          if (Suffix.equals_insensitive("fs"))
            return 1u;
          if (Suffix.equals_insensitive("fh"))
            return 2u;
          if (Suffix.equals_insensitive("fb"))
            return 3u;
          return std::nullopt;
        };

        StringRef Suffix = Head.drop_front(strlen("v.icvtf."));
        size_t Sep = Suffix.find('2');
        if (Sep == StringRef::npos)
          fail("expected v.icvtf.<srcT>2<dstT>");
        auto SrcType = parseVecIntSrcType(Suffix.take_front(Sep));
        auto DstType = parseVecFpDstType(Suffix.drop_front(Sep + 1));
        if (!SrcType || !DstType)
          fail("unsupported v.icvtf type suffix");

        StringRef SrcPart;
        ParsedVReg Dst;
        if (!parseArrow(Rest, SrcPart, Dst))
          fail("expected '->Dst' in v.icvtf");
        SmallVector<StringRef, 2> Ops;
        splitCSV(SrcPart, Ops);
        if (Ops.size() != 1)
          fail("expected one source operand for v.icvtf");
        auto SrcL = parseVecRegToken(Ops[0]);
        if (!SrcL)
          fail("failed to parse source operand for v.icvtf");
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(),
                TII.get(LinxISA::PSEUDO_V_ICVTF))
            .addImm(Dst.Code)
            .addImm(SrcL->Code)
            .addImm(*DstType)
            .addImm(*SrcType);
        return;
      }

	      if (Head.starts_with_insensitive("v.cmp.")) {
	        StringRef SrcPart;
	        unsigned DstCode = 0;
	        if (!parseArrowDstCode(Rest, SrcPart, DstCode))
          fail("expected '->Dst' in vector compare op");
        SmallVector<StringRef, 4> Ops;
        splitCSV(SrcPart, Ops);
        if (Ops.size() != 2)
          fail("expected two source operands for vector compare op");
        auto SrcL = parseVecRegToken(Ops[0]);
        auto SrcR = parseVecRegToken(Ops[1]);
        if (!SrcL || !SrcR)
          fail("failed to parse source operands for vector compare op");

        unsigned Opc = 0;
        if (Head.equals_insensitive("v.cmp.eq"))
          Opc = LinxISA::PSEUDO_V_CMP_EQ;
        else if (Head.equals_insensitive("v.cmp.ne"))
          Opc = LinxISA::PSEUDO_V_CMP_NE;
        else if (Head.equals_insensitive("v.cmp.lt"))
          Opc = LinxISA::PSEUDO_V_CMP_LT;
        else if (Head.equals_insensitive("v.cmp.ltu"))
          Opc = LinxISA::PSEUDO_V_CMP_LTU;
        else if (Head.equals_insensitive("v.cmp.ge"))
          Opc = LinxISA::PSEUDO_V_CMP_GE;
        else if (Head.equals_insensitive("v.cmp.geu"))
          Opc = LinxISA::PSEUDO_V_CMP_GEU;
        if (!Opc)
          fail("unsupported vector compare op");

        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(DstCode)
            .addImm(SrcL->Code)
            .addImm(SrcR->Code);
        return;
      }

      if (Head.equals_insensitive("v.feq") || Head.equals_insensitive("v.fne") ||
          Head.equals_insensitive("v.flt") || Head.equals_insensitive("v.fge")) {
        StringRef SrcPart;
        unsigned DstCode = 0;
        if (!parseArrowDstCode(Rest, SrcPart, DstCode))
          fail("expected '->Dst' in vector FP compare op");
        SmallVector<StringRef, 4> Ops;
        splitCSV(SrcPart, Ops);
        if (Ops.size() != 2)
          fail("expected two source operands for vector FP compare op");
        auto SrcL = parseVecRegToken(Ops[0]);
        auto SrcR = parseVecRegToken(Ops[1]);
        if (!SrcL || !SrcR)
          fail("failed to parse source operands for vector FP compare op");

        unsigned Opc = 0;
        if (Head.equals_insensitive("v.feq"))
          Opc = LinxISA::PSEUDO_V_FEQ;
        else if (Head.equals_insensitive("v.fne"))
          Opc = LinxISA::PSEUDO_V_FNE;
        else if (Head.equals_insensitive("v.flt"))
          Opc = LinxISA::PSEUDO_V_FLT;
        else if (Head.equals_insensitive("v.fge"))
          Opc = LinxISA::PSEUDO_V_FGE;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(DstCode)
            .addImm(SrcL->Code)
            .addImm(SrcR->Code);
        return;
      }

      if (Head.starts_with_insensitive("v.rd")) {
        StringRef SrcPart;
        ParsedVReg Dst;
        if (!parseArrow(Rest, SrcPart, Dst))
          fail("expected '->Dst' in vector reduction op");
        SmallVector<StringRef, 2> Ops;
        splitCSV(SrcPart, Ops);
        if (Ops.size() != 1)
          fail("expected one source operand for vector reduction op");
        auto SrcL = parseVecRegToken(Ops[0]);
        if (!SrcL)
          fail("failed to parse source operand for vector reduction op");

        unsigned Opc = 0;
        if (Head.equals_insensitive("v.rdadd"))
          Opc = LinxISA::PSEUDO_V_RDADD;
        else if (Head.equals_insensitive("v.rdand"))
          Opc = LinxISA::PSEUDO_V_RDAND;
        else if (Head.equals_insensitive("v.rdfadd"))
          Opc = LinxISA::PSEUDO_V_RDFADD;
        else if (Head.equals_insensitive("v.rdfmax"))
          Opc = LinxISA::PSEUDO_V_RDFMAX;
        else if (Head.equals_insensitive("v.rdfmin"))
          Opc = LinxISA::PSEUDO_V_RDFMIN;
        else if (Head.equals_insensitive("v.rdmax"))
          Opc = LinxISA::PSEUDO_V_RDMAX;
        else if (Head.equals_insensitive("v.rdmin"))
          Opc = LinxISA::PSEUDO_V_RDMIN;
        else if (Head.equals_insensitive("v.rdor"))
          Opc = LinxISA::PSEUDO_V_RDOR;
        else if (Head.equals_insensitive("v.rdxor"))
          Opc = LinxISA::PSEUDO_V_RDXOR;
        if (!Opc)
          fail("unsupported vector reduction op");

        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(Dst.Code)
            .addImm(SrcL->Code);
        return;
      }

      if (Head.equals_insensitive("v.csel") ||
          Head.equals_insensitive("v.psel")) {
        StringRef SrcPart;
        ParsedVReg Dst;
        if (!parseArrow(Rest, SrcPart, Dst))
          fail("expected '->Dst' in vector select");
        SmallVector<StringRef, 6> Ops;
        splitCSV(SrcPart, Ops);
        if (Head.equals_insensitive("v.psel")) {
          if (Ops.size() != 2)
            fail("expected two source operands for v.psel");
          auto SrcP = parseVecRegToken(Ops[0]);
          auto SrcL = parseVecRegToken(Ops[1]);
          if (!SrcP || !SrcL)
            fail("failed to parse source operands for v.psel");
          BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(LinxISA::PSEUDO_V_PSEL))
              .addImm(Dst.Code)
              .addImm(SrcP->Code)
              .addImm(SrcL->Code);
          return;
        }
        if (Ops.size() != 3)
          fail("expected three source operands for v.csel");
        auto SrcP = parseVecRegToken(Ops[0]);
        auto SrcL = parseVecRegToken(Ops[1]);
        auto SrcR = parseVecRegToken(Ops[2]);
        if (!SrcP || !SrcL || !SrcR)
          fail("failed to parse source operands for vector select");
        const unsigned SrcRType = (SrcR->SrcRType == 3u) ? 0u : SrcR->SrcRType;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(LinxISA::PSEUDO_V_CSEL))
            .addImm(Dst.Code)
            .addImm(SrcP->Code)
            .addImm(SrcL->Code)
            .addImm(SrcR->Code)
            .addImm(SrcRType);
        return;
      }

      if (Head.starts_with_insensitive("v.lb.brg") ||
          Head.equals_insensitive("v.lb.local") ||
          Head.starts_with_insensitive("v.lh.brg") ||
          Head.equals_insensitive("v.lh.local") ||
          Head.starts_with_insensitive("v.lbu.brg") ||
          Head.equals_insensitive("v.lbu.local") ||
          Head.starts_with_insensitive("v.lhu.brg") ||
          Head.equals_insensitive("v.lhu.local") ||
          Head.starts_with_insensitive("v.lw.brg") ||
          Head.equals_insensitive("v.lw.local")) {
        StringRef SrcPart;
        ParsedVReg Dst;
        if (!parseArrow(Rest, SrcPart, Dst))
          fail("expected '->Dst' in v.l[b|h|w]");
        ParsedVReg Base, Index;
        const bool IsSignedByte =
            Head.starts_with_insensitive("v.lb.brg") ||
            Head.equals_insensitive("v.lb.local");
        const bool IsSignedHalf =
            Head.starts_with_insensitive("v.lh.brg") ||
            Head.equals_insensitive("v.lh.local");
        const bool IsByte =
            Head.starts_with_insensitive("v.lbu.brg") ||
            Head.equals_insensitive("v.lbu.local");
        const bool IsHalf =
            Head.starts_with_insensitive("v.lhu.brg") ||
            Head.equals_insensitive("v.lhu.local");
        const bool IsNarrowByte = IsSignedByte || IsByte;
        const bool IsNarrowHalf = IsSignedHalf || IsHalf;
        const unsigned WantLaneShamt =
            IsNarrowByte ? 0u : (IsNarrowHalf ? 1u : 2u);
        if (!parseMemTriple(SrcPart, WantLaneShamt, Base, Index))
          fail("expected memory form [base, lc0<<esize, idx] in v.l[b|h|w]");
        const unsigned LocalBit =
            (Head.contains_insensitive(".local") ||
             Head.equals_insensitive("v.lb.local") ||
             Head.equals_insensitive("v.lh.local") ||
             Head.equals_insensitive("v.lbu.local") ||
             Head.equals_insensitive("v.lhu.local") ||
             Head.equals_insensitive("v.lw.local"))
                ? 1u
                : 0u;
        const unsigned Opc =
            IsSignedByte    ? LinxISA::PSEUDO_V_LB_BRG
            : IsSignedHalf ? LinxISA::PSEUDO_V_LH_BRG
            : IsByte       ? LinxISA::PSEUDO_V_LBU_BRG
            : IsHalf       ? LinxISA::PSEUDO_V_LHU_BRG
                           : LinxISA::PSEUDO_V_LW_BRG;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(Dst.Code)
            .addImm(Base.Code)
            .addImm(Index.Code)
            .addImm(Index.Shamt)
            .addImm(LocalBit);
        return;
      }

      if (Head.starts_with_insensitive("v.lwi.u") ||
          Head.starts_with_insensitive("v.ldi.u")) {
        StringRef SrcPart;
        ParsedVReg Dst;
        if (!parseArrow(Rest, SrcPart, Dst))
          fail("expected '->Dst' in v.lwi/v.ldi");
        ParsedVReg Base;
        int64_t Imm = 0;
        const bool IsDword = Head.starts_with_insensitive("v.ldi.u");
        const unsigned LaneShamt = IsDword ? 3u : 2u;
        if (!parseMemImm(SrcPart, LaneShamt, Base, Imm))
          fail("expected memory form [base, lc0<<shift, simm] in v.lwi/v.ldi");
        const unsigned LocalBit = Head.contains_insensitive(".local") ? 1u : 0u;
        const unsigned BrgBit = Head.contains_insensitive(".brg") ? 1u : 0u;
        const unsigned Opc =
            IsDword ? LinxISA::PSEUDO_V_LDI_U : LinxISA::PSEUDO_V_LWI_U;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(Dst.Code)
            .addImm(Base.Code)
            .addImm(Imm)
            .addImm(LocalBit)
            .addImm(BrgBit);
        return;
      }

      if (Head.starts_with_insensitive("v.sb.brg") ||
          Head.equals_insensitive("v.sb.local") ||
          Head.starts_with_insensitive("v.sh.brg") ||
          Head.equals_insensitive("v.sh.local") ||
          Head.starts_with_insensitive("v.sw.brg") ||
          Head.equals_insensitive("v.sw.local")) {
        const size_t LBr = Rest.find('[');
        if (LBr == StringRef::npos)
          fail("expected memory operand in v.s[b|h|w]");
        StringRef ValuePart = Rest.take_front(LBr).trim();
        if (ValuePart.ends_with(","))
          ValuePart = ValuePart.drop_back().trim();
        StringRef MemPart = Rest.drop_front(LBr).trim();
        auto SrcD = parseVecRegToken(ValuePart);
        ParsedVReg Base, Index;
        const bool IsByte =
            Head.starts_with_insensitive("v.sb.brg") ||
            Head.equals_insensitive("v.sb.local");
        const bool IsHalf =
            Head.starts_with_insensitive("v.sh.brg") ||
            Head.equals_insensitive("v.sh.local");
        const unsigned WantLaneShamt = IsByte ? 0u : (IsHalf ? 1u : 2u);
        if (!SrcD || !parseMemTriple(MemPart, WantLaneShamt, Base, Index))
          fail("expected v.s[b|h|w] SrcD, [base, lc0<<esize, idx]");
        const bool IsZeroIndex = Index.Code == 0u && Index.Shamt == 0u;
        if (!IsByte && !IsZeroIndex && Index.Shamt < WantLaneShamt)
          fail("v.sh/v.sw index shift must be >= lane shift");
        const unsigned EncodedShamt =
            IsByte ? Index.Shamt
                   : (IsZeroIndex ? 0u : (Index.Shamt - WantLaneShamt));
        const unsigned LocalBit =
            (Head.contains_insensitive(".local") ||
             Head.equals_insensitive("v.sb.local") ||
             Head.equals_insensitive("v.sh.local") ||
             Head.equals_insensitive("v.sw.local"))
                ? 1u
                : 0u;
        const unsigned Opc = IsByte    ? LinxISA::PSEUDO_V_SB_BRG
                             : IsHalf ? LinxISA::PSEUDO_V_SH_BRG
                                      : LinxISA::PSEUDO_V_SW_BRG;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(SrcD->Code)
            .addImm(Base.Code)
            .addImm(Index.Code)
            .addImm(EncodedShamt)
            .addImm(LocalBit);
        return;
      }

      if (Head.starts_with_insensitive("v.swi.u") ||
          Head.starts_with_insensitive("v.sdi.u")) {
        const size_t LBr = Rest.find('[');
        if (LBr == StringRef::npos)
          fail("expected memory operand in v.swi/v.sdi");
        StringRef ValuePart = Rest.take_front(LBr).trim();
        if (ValuePart.ends_with(","))
          ValuePart = ValuePart.drop_back().trim();
        StringRef MemPart = Rest.drop_front(LBr).trim();
        auto SrcD = parseVecRegToken(ValuePart);
        ParsedVReg Base;
        int64_t Imm = 0;
        const bool IsDword = Head.starts_with_insensitive("v.sdi.u");
        const unsigned LaneShamt = IsDword ? 3u : 2u;
        if (!SrcD || !parseMemImm(MemPart, LaneShamt, Base, Imm))
          fail("expected v.swi/v.sdi Src, [base, lc0<<shift, simm]");
        const unsigned LocalBit = Head.contains_insensitive(".local") ? 1u : 0u;
        const unsigned BrgBit = Head.contains_insensitive(".brg") ? 1u : 0u;
        const unsigned Opc =
            IsDword ? LinxISA::PSEUDO_V_SDI_U : LinxISA::PSEUDO_V_SWI_U;
        BuildMI(BodyBB, BodyBB.end(), DebugLoc(), TII.get(Opc))
            .addImm(SrcD->Code)
            .addImm(Base.Code)
            .addImm(Imm)
            .addImm(LocalBit)
            .addImm(BrgBit);
        return;
      }

      fail("unsupported vector body statement");
    };

    auto emitVectorBodyText = [&](MachineBasicBlock &BodyBB, StringRef BodyText,
                                  StringRef CtxName) {
      SmallVector<StringRef, 64> Lines;
      StringRef Cursor = BodyText;
      while (!Cursor.empty()) {
        auto Split = Cursor.split('\n');
        Lines.push_back(Split.first);
        Cursor = Split.second;
      }

      StringMap<MCSymbol *> LabelSyms;
      MCContext &Ctx = MF.getContext();

      auto makeContextTag = [&](StringRef S) -> std::string {
        std::string Tag;
        Tag.reserve(S.size());
        for (char C : S) {
          if (std::isalnum(static_cast<unsigned char>(C)))
            Tag.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(C))));
          else
            Tag.push_back('_');
        }
        return Tag;
      };
      const std::string CtxTag = makeContextTag(CtxName);

      auto getOrCreateLabelSym = [&](StringRef LabelToken) -> MCSymbol * {
        std::string Key = normalizeLabel(LabelToken);
        if (Key.empty())
          return nullptr;
        auto It = LabelSyms.find(Key);
        if (It != LabelSyms.end())
          return It->second;
        SmallString<96> SymName;
        raw_svector_ostream OS(SymName);
        OS << ".__linx_vbody_" << CtxTag << "." << MF.getFunctionNumber()
           << "." << Key;
        MCSymbol *Sym = Ctx.getOrCreateSymbol(OS.str());
        LabelSyms[Key] = Sym;
        return Sym;
      };

      // Pass 1: collect all labels (forward references are legal).
      for (StringRef RawLine : Lines) {
        StringRef Line = RawLine;
        if (size_t Semi = Line.find(';'); Semi != StringRef::npos)
          Line = Line.take_front(Semi);
        Line = Line.trim();
        if (!Line.empty() && Line.ends_with(":"))
          (void)getOrCreateLabelSym(Line);
      }

      auto lookupLabelSym = [&](StringRef LabelToken) -> MCSymbol * {
        std::string Key = normalizeLabel(LabelToken);
        if (Key.empty())
          return nullptr;
        auto It = LabelSyms.find(Key);
        if (It == LabelSyms.end())
          return nullptr;
        return It->second;
      };

      VecPipeCursorState PipeState;
      for (StringRef RawLine : Lines)
        emitVectorBodyLine(BodyBB, RawLine, CtxName, PipeState,
                           lookupLabelSym);
    };

    auto getOrCreateVBlockBodySym = [&]() -> MCSymbol * {
      if (VBlockBodySym)
        return VBlockBodySym;

      MCContext &Ctx = MF.getContext();
      SmallString<64> Name;
      raw_svector_ostream OS(Name);
      OS << ".__linx_vblock_body." << MF.getFunctionNumber();
      VBlockBodySym = Ctx.getOrCreateSymbol(OS.str());

      VBlockBodyBB = MF.CreateMachineBasicBlock();
      MF.insert(MF.end(), VBlockBodyBB);
      VBlockBodyBB->setLabelMustBeEmitted();

      BuildMI(*VBlockBodyBB, VBlockBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(VBlockBodySym);
      static const char kDefaultBodyAsm[] =
          "  v.add lc0.sw, lc1.sw, ->vt\n"
          "  C.BSTOP\n";
      StringRef BodyText = kDefaultBodyAsm;
      if (auto *MFI = MF.getInfo<LinxISAMachineFunctionInfo>()) {
        if (MFI->hasVBlockBodyAsm())
          BodyText = MFI->getVBlockBodyAsm();
      }
      emitVectorBodyText(*VBlockBodyBB, BodyText, "vblock body");
      BuildMI(*VBlockBodyBB, VBlockBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(Ctx.getOrCreateSymbol(VBlockBodySym->getName() + ".end"));

      DecoupledBodyBBs.insert(VBlockBodyBB);
      Changed = true;
      return VBlockBodySym;
    };

    auto getOrCreateVTileAddBodySym = [&]() -> MCSymbol * {
      if (VTileAddBodySym)
        return VTileAddBodySym;

      MCContext &Ctx = MF.getContext();
      SmallString<64> Name;
      raw_svector_ostream OS(Name);
      OS << ".__linx_vtile_add_body." << MF.getFunctionNumber();
      VTileAddBodySym = Ctx.getOrCreateSymbol(OS.str());

      VTileAddBodyBB = MF.CreateMachineBasicBlock();
      MF.insert(MF.end(), VTileAddBodyBB);
      VTileAddBodyBB->setLabelMustBeEmitted();

      BuildMI(*VTileAddBodyBB, VTileAddBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(VTileAddBodySym);

      static const char kBodyAsm[] =
          "  v.lw.local [ta, lc0<<2, lc1<<8], ->vt.w\n"
          "  v.lw.local [tb, lc0<<2, lc1<<8], ->vu.w\n"
          "  v.add vt#1.sw, vu#1.sw, ->vt.w\n"
          "  v.sw.local vt#1, [to, lc0<<2, lc1<<8]\n"
          "  C.BSTOP\n";
      emitVectorBodyText(*VTileAddBodyBB, StringRef(kBodyAsm), "vtile add body");
      BuildMI(*VTileAddBodyBB, VTileAddBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(Ctx.getOrCreateSymbol(VTileAddBodySym->getName() + ".end"));

      DecoupledBodyBBs.insert(VTileAddBodyBB);
      Changed = true;
      return VTileAddBodySym;
    };

    auto getOrCreateVTileSubBodySym = [&]() -> MCSymbol * {
      if (VTileSubBodySym)
        return VTileSubBodySym;

      MCContext &Ctx = MF.getContext();
      SmallString<64> Name;
      raw_svector_ostream OS(Name);
      OS << ".__linx_vtile_sub_body." << MF.getFunctionNumber();
      VTileSubBodySym = Ctx.getOrCreateSymbol(OS.str());

      VTileSubBodyBB = MF.CreateMachineBasicBlock();
      MF.insert(MF.end(), VTileSubBodyBB);
      VTileSubBodyBB->setLabelMustBeEmitted();

      BuildMI(*VTileSubBodyBB, VTileSubBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(VTileSubBodySym);

      static const char kBodyAsm[] =
          "  v.lw.local [ta, lc0<<2, lc1<<8], ->vt.w\n"
          "  v.lw.local [tb, lc0<<2, lc1<<8], ->vu.w\n"
          "  v.sub vt#1.sw, vu#1.sw, ->vt.w\n"
          "  v.sw.local vt#1, [to, lc0<<2, lc1<<8]\n"
          "  C.BSTOP\n";
      emitVectorBodyText(*VTileSubBodyBB, StringRef(kBodyAsm), "vtile sub body");
      BuildMI(*VTileSubBodyBB, VTileSubBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(Ctx.getOrCreateSymbol(VTileSubBodySym->getName() + ".end"));

      DecoupledBodyBBs.insert(VTileSubBodyBB);
      Changed = true;
      return VTileSubBodySym;
    };

    auto splitAfterCall = [&](MachineBasicBlock &MBB, MachineInstr &CallMI)
        -> MachineBasicBlock * {
      MachineFunction &MF = *MBB.getParent();
      auto *ContBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
      MF.insert(std::next(MBB.getIterator()), ContBB);

      // Move everything after the call into the continuation block.
      auto SplitPt = std::next(CallMI.getIterator());
      ContBB->splice(ContBB->end(), &MBB, SplitPt, MBB.end());

      // Continuation inherits the original CFG edges; call block falls through to
      // the continuation after return.
      ContBB->transferSuccessorsAndUpdatePHIs(&MBB);
      MBB.addSuccessor(ContBB);
      return ContBB;
    };

    auto splitAfterInstr = [&](MachineBasicBlock &MBB, MachineInstr &MI)
        -> MachineBasicBlock * {
      MachineFunction &MF = *MBB.getParent();
      auto *ContBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
      MF.insert(std::next(MBB.getIterator()), ContBB);

      auto SplitPt = std::next(MI.getIterator());
      ContBB->splice(ContBB->end(), &MBB, SplitPt, MBB.end());

      ContBB->transferSuccessorsAndUpdatePHIs(&MBB);
      MBB.addSuccessor(ContBB);
      return ContBB;
    };

    auto splitBeforeInstr = [&](MachineBasicBlock &MBB, MachineInstr &MI)
        -> MachineBasicBlock * {
      MachineFunction &MF = *MBB.getParent();
      auto *TailBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
      MF.insert(std::next(MBB.getIterator()), TailBB);

      auto SplitPt = MI.getIterator();
      TailBB->splice(TailBB->end(), &MBB, SplitPt, MBB.end());

      TailBB->transferSuccessorsAndUpdatePHIs(&MBB);
      MBB.addSuccessor(TailBB);
      return TailBB;
    };

    // Inline asm used for low-level syscall/runtime bring-up can carry explicit
    // BlockISA boundaries (ACRC/C.BSTOP/C.BSTART). Keep those snippets from
    // being absorbed into a later CALL/ICALL block whose header is inserted at
    // the start of the MachineBasicBlock.
    SmallVector<MachineBasicBlock *, 32> InlineAsmBoundarySplitWorklist;
    InlineAsmBoundarySplitWorklist.reserve(MF.size());
    for (MachineBasicBlock &MBB : MF)
      InlineAsmBoundarySplitWorklist.push_back(&MBB);

    while (!InlineAsmBoundarySplitWorklist.empty()) {
      MachineBasicBlock *MBB = InlineAsmBoundarySplitWorklist.pop_back_val();
      for (MachineInstr &MI : *MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction())
          continue;
        if (!inlineAsmHasExplicitBlockBoundary(MI))
          continue;

        auto Next = std::next(MI.getIterator());
        while (Next != MBB->end() &&
               (Next->isDebugInstr() || Next->isCFIInstruction()))
          ++Next;
        if (Next == MBB->end())
          break;

        MachineBasicBlock *ContBB = splitAfterInstr(*MBB, MI);
        InlineAsmBoundarySplitWorklist.push_back(ContBB);
        Changed = true;
        break;
      }
    }

    // Ensure call-transfer pseudos end a block. This matches BlockISA: call
    // headers are block exits, and musttail transfer pseudos are terminators.
    SmallVector<MachineBasicBlock *, 32> CallSplitWorklist;
    CallSplitWorklist.reserve(MF.size());
    for (MachineBasicBlock &MBB : MF)
      CallSplitWorklist.push_back(&MBB);

    while (!CallSplitWorklist.empty()) {
      MachineBasicBlock *MBB = CallSplitWorklist.pop_back_val();
      for (MachineInstr &MI : *MBB) {
        if (MI.isDebugInstr())
          continue;
        if (MI.getOpcode() != LinxISA::PSEUDO_CALL &&
            MI.getOpcode() != LinxISA::PSEUDO_ICALL)
          continue;

        auto Next = std::next(MI.getIterator());
        while (Next != MBB->end() && Next->isDebugInstr())
          ++Next;
        if (Next == MBB->end())
          break; // already ends the block

        MachineBasicBlock *ContBB = splitAfterCall(*MBB, MI);
        CallSplitWorklist.push_back(ContBB);
        Changed = true;
        break;
      }
    }

    // Ensure frame macro instructions are standalone blocks.
    //
    // FENTRY/FEXIT/FRET.* are "block instructions": they already contain the
    // required block markers and micro-ops for stack/register management. Some
    // mid/late CodeGen passes may merge these blocks back into surrounding
    // blocks; re-split here so the final assembly keeps them isolated.
    SmallVector<MachineBasicBlock *, 32> MacroSplitWorklist;
    MacroSplitWorklist.reserve(MF.size());
    for (MachineBasicBlock &MBB : MF)
      MacroSplitWorklist.push_back(&MBB);

    while (!MacroSplitWorklist.empty()) {
      MachineBasicBlock *MBB = MacroSplitWorklist.pop_back_val();
      MachineInstr *MacroMI = nullptr;
      for (MachineInstr &MI : *MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
          continue;
        if (isFrameMacroInstr(MI)) {
          MacroMI = &MI;
          break;
        }
      }

      if (!MacroMI || isStandaloneFrameMacroBlock(*MBB))
        continue;

      auto hasRealInstrBefore = [&](const MachineInstr &Anchor) -> bool {
        for (const MachineInstr &MI : *MBB) {
          if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
            continue;
          if (&MI == &Anchor)
            return false;
          return true;
        }
        return false;
      };
      auto hasRealInstrAfter = [&](const MachineInstr &Anchor) -> bool {
        bool SeenAnchor = false;
        for (const MachineInstr &MI : *MBB) {
          if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
            continue;
          if (!SeenAnchor) {
            SeenAnchor = (&MI == &Anchor);
            continue;
          }
          return true;
        }
        return false;
      };

      switch (MacroMI->getOpcode()) {
      case LinxISA::FENTRY: {
        if (hasRealInstrBefore(*MacroMI))
          report_fatal_error("Linx: FENTRY must be the first instruction in its block");
        if (!hasRealInstrAfter(*MacroMI))
          continue;
        MachineBasicBlock *ContBB = splitAfterInstr(*MBB, *MacroMI);
        MacroSplitWorklist.push_back(ContBB);
        Changed = true;
        break;
      }
      case LinxISA::FEXIT:
      case LinxISA::FRET_RA:
      case LinxISA::FRET_STK: {
        if (hasRealInstrAfter(*MacroMI)) {
          // Some late CFG cleanups may merge a standalone frame-macro block with
          // its successor. Re-split instead of hard-failing.
          MachineBasicBlock *ContBB = splitAfterInstr(*MBB, *MacroMI);
          MacroSplitWorklist.push_back(ContBB);
          MacroSplitWorklist.push_back(MBB);
          Changed = true;
          break;
        }
        if (!hasRealInstrBefore(*MacroMI))
          continue;
        MachineBasicBlock *TailBB = splitBeforeInstr(*MBB, *MacroMI);
        MacroSplitWorklist.push_back(TailBB);
        Changed = true;
        break;
      }
      case LinxISA::MCOPY:
      case LinxISA::MSET: {
        // Template blocks are standalone block start markers and must not be
        // merged with surrounding instructions.
        if (hasRealInstrBefore(*MacroMI)) {
          MachineBasicBlock *TailBB = splitBeforeInstr(*MBB, *MacroMI);
          MacroSplitWorklist.push_back(TailBB);
          Changed = true;
          break;
        }
        if (hasRealInstrAfter(*MacroMI)) {
          MachineBasicBlock *ContBB = splitAfterInstr(*MBB, *MacroMI);
          MacroSplitWorklist.push_back(ContBB);
          Changed = true;
          break;
        }
        break;
      }
      default:
        break;
      }
    }

    // Ensure tile/vector pseudo instructions are standalone blocks, then expand
    // them to decoupled-header descriptor sequences.
    //
    // Tile blocks are block-structured ISA units; their headers must be the
    // first real instruction in the block.
    SmallVector<MachineBasicBlock *, 32> TileSplitWorklist;
    TileSplitWorklist.reserve(MF.size());
    for (MachineBasicBlock &MBB : MF)
      TileSplitWorklist.push_back(&MBB);

    while (!TileSplitWorklist.empty()) {
      MachineBasicBlock *MBB = TileSplitWorklist.pop_back_val();
      MachineInstr *PseudoMI = nullptr;
      for (MachineInstr &MI : *MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
          continue;
        if (isTilePseudoInstr(MI) || isVBlockPseudoInstr(MI)) {
          PseudoMI = &MI;
          break;
        }
      }
      if (!PseudoMI)
        continue;

      auto hasRealInstrBefore = [&](const MachineInstr &Anchor) -> bool {
        for (const MachineInstr &MI : *MBB) {
          if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
            continue;
          if (&MI == &Anchor)
            return false;
          return true;
        }
        return false;
      };
      auto hasRealInstrAfter = [&](const MachineInstr &Anchor) -> bool {
        bool SeenAnchor = false;
        for (const MachineInstr &MI : *MBB) {
          if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
            continue;
          if (!SeenAnchor) {
            SeenAnchor = (&MI == &Anchor);
            continue;
          }
          return true;
        }
        return false;
      };

      if (hasRealInstrBefore(*PseudoMI)) {
        MachineBasicBlock *TailBB = splitBeforeInstr(*MBB, *PseudoMI);
        TileSplitWorklist.push_back(TailBB);
        Changed = true;
        continue;
      }
      if (hasRealInstrAfter(*PseudoMI)) {
        MachineBasicBlock *ContBB = splitAfterInstr(*MBB, *PseudoMI);
        TileSplitWorklist.push_back(ContBB);
        Changed = true;
        continue;
      }
    }

    // Tile registers are physical allocation identities, while B.IOT sources
    // are architectural relative queue references (#1 is newest).  Simulate
    // each hand across the CFG after tile pseudos have been isolated, and
    // reject joins whose incoming queue order is not uniquely provable.  The
    // canonical source code uses 16 encodings per hand (T/U/M/N), even though
    // the compiler's physical TILE register groups contain eight registers.
    DenseMap<const MachineInstr *, SmallVector<unsigned, 2>> TileSourceCodes;
    DenseMap<const MachineBasicBlock *, TileQueueState> TileQueueEntries;
    DenseMap<const MachineBasicBlock *, bool> TileQueueProcessed;
    SmallVector<const MachineBasicBlock *, 32> TileQueueWorklist;

    for (const MachineBasicBlock &MBB : MF) {
      if (MBB.pred_empty()) {
        TileQueueEntries.try_emplace(&MBB);
        TileQueueWorklist.push_back(&MBB);
      }
    }
    if (!TileQueueEntries.count(&MF.front())) {
      TileQueueEntries.try_emplace(&MF.front());
      TileQueueWorklist.push_back(&MF.front());
    }

    auto recordSources = [&](const MachineInstr &MI, TileQueueState &State,
                             ArrayRef<unsigned> OperandNos,
                             StringRef Context) {
      SmallVector<unsigned, 2> Codes;
      SmallVector<Register, 2> Consumed;
      for (unsigned OperandNo : OperandNos) {
        const MachineOperand &MO = MI.getOperand(OperandNo);
        const Register Reg = MO.getReg();
        Codes.push_back(encodeTileQueueSource(TRI, State, Reg, Context));
        if (MO.isKill() && !llvm::is_contained(Consumed, Reg))
          Consumed.push_back(Reg);
      }
      TileSourceCodes[&MI] = std::move(Codes);
      for (Register Reg : Consumed)
        consumeTileQueueValue(TRI, State, Reg, Context);
    };

    auto transferTilePseudo = [&](const MachineInstr &MI,
                                  TileQueueState &State) {
      switch (MI.getOpcode()) {
      case LinxISA::PSEUDO_TMA_TLOAD:
      case LinxISA::PSEUDO_TMA_TLOAD_ANY:
      case LinxISA::PSEUDO_TMA_TLOAD_DESC:
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      case LinxISA::PSEUDO_TMA_TSTORE:
      case LinxISA::PSEUDO_TMA_TSTORE_DESC:
        recordSources(MI, State, {1}, "TMA.TSTORE");
        return;
      case LinxISA::PSEUDO_TMA_TMOV: {
        const bool IsA2V = MI.getOperand(6).getImm() ==
                           static_cast<int64_t>(TMovMode::A2V);
        if (!IsA2V) {
          const Register Src = MI.getOperand(1).getReg();
          TileSourceCodes[&MI] = {
              encodeTileQueueSource(TRI, State, Src, "TMA.TMOV")};
          const bool Reuse = (MI.getOperand(7).getImm() & 1) != 0;
          if (!Reuse)
            consumeTileQueueValue(TRI, State, Src, "TMA.TMOV");
        }
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      }
      case LinxISA::PSEUDO_CUBE_MAMULB:
        recordSources(MI, State, {1, 2}, "CUBE.MAMULB");
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      case LinxISA::PSEUDO_CUBE_MAMULB_ACC:
        if (!MI.getOperand(1).isKill())
          report_fatal_error(
              "Linx: CUBE.MAMULB.ACC accumulator carrier must be killed");
        // Descriptor sources are resolved against the entry queue.  Commit
        // releases the implicit accumulator carrier only after those source
        // ranks have been frozen.
        recordSources(MI, State, {2, 3}, "CUBE.MAMULB.ACC");
        consumeTileQueueValue(TRI, State, MI.getOperand(1).getReg(),
                              "CUBE.MAMULB.ACC accumulator carrier");
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      case LinxISA::PSEUDO_CUBE_ACCCVT:
        if (!MI.getOperand(1).isKill())
          report_fatal_error(
              "Linx: CUBE.ACCCVT accumulator carrier must be killed");
        consumeTileQueueValue(TRI, State, MI.getOperand(1).getReg(),
                              "CUBE.ACCCVT accumulator carrier");
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      case LinxISA::PSEUDO_TEPL_UNARY:
      case LinxISA::PSEUDO_TEPL_BINARY_SCALAR:
        recordSources(MI, State, {1}, "TEPL tile op");
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      case LinxISA::PSEUDO_TEPL_BINARY:
        recordSources(MI, State, {1, 2}, "TEPL.BINARY");
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      case LinxISA::PSEUDO_TEPL_SPLAT:
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      case LinxISA::PSEUDO_VPAR_TADD:
      case LinxISA::PSEUDO_VPAR_TSUB:
      case LinxISA::PSEUDO_VTILE_ADD:
      case LinxISA::PSEUDO_VTILE_SUB:
        recordSources(MI, State, {1, 2}, "VPAR tile binop");
        pushTileQueueValue(TRI, State, MI.getOperand(0).getReg());
        return;
      default:
        return;
      }
    };

    while (!TileQueueWorklist.empty()) {
      const MachineBasicBlock *MBB = TileQueueWorklist.pop_back_val();
      if (TileQueueProcessed.lookup(MBB))
        continue;
      TileQueueProcessed[MBB] = true;
      TileQueueState Exit = TileQueueEntries.lookup(MBB);
      for (const MachineInstr &MI : *MBB) {
        if (isTilePseudoInstr(MI))
          transferTilePseudo(MI, Exit);
      }
      for (const MachineBasicBlock *Succ : MBB->successors()) {
        auto [It, Inserted] = TileQueueEntries.try_emplace(Succ, Exit);
        if (!Inserted && !(It->second == Exit))
          report_fatal_error(
              "Linx: tile queue order is ambiguous at a control-flow join");
        if (Inserted)
          TileQueueWorklist.push_back(Succ);
      }
    }

    for (const MachineBasicBlock &MBB : MF) {
      for (const MachineInstr &MI : MBB) {
        if (isTilePseudoInstr(MI) && !TileQueueProcessed.lookup(&MBB))
          report_fatal_error(
              "Linx: tile queue order is not provable through unreachable or cyclic control flow");
      }
    }

    for (MachineBasicBlock &MBB : MF) {
      MachineInstr *PseudoMI = nullptr;
      for (MachineInstr &MI : MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
          continue;
        if (isTilePseudoInstr(MI) || isVBlockPseudoInstr(MI)) {
          PseudoMI = &MI;
          break;
        }
      }
      if (!PseudoMI)
        continue;

      // Strip any stale standard block markers (in case the pass runs twice).
      for (auto It = MBB.begin(); It != MBB.end();) {
        if (It->isDebugInstr() || It->isCFIInstruction()) {
          ++It;
          continue;
        }
        if (It->getOpcode() == LinxISA::CBSTART_STD ||
            It->getOpcode() == LinxISA::BSTART_STD_FALL ||
            It->getOpcode() == LinxISA::BSTART_STD_DIRECT ||
            It->getOpcode() == LinxISA::BSTART_STD_COND ||
            It->getOpcode() == LinxISA::BSTART_STD_CALL ||
            It->getOpcode() == LinxISA::BSTART_STD_IND ||
            It->getOpcode() == LinxISA::BSTART_STD_ICALL ||
            It->getOpcode() == LinxISA::BSTART_STD_RET) {
          It = MBB.erase(It);
          Changed = true;
          continue;
        }
        ++It;
      }

      DebugLoc DL = PseudoMI->getDebugLoc();
      auto InsertPt = PseudoMI->getIterator();

      // Remove any markers that may have been left immediately before the
      // pseudo; tile headers must be first real instruction.
      while (InsertPt != MBB.begin()) {
        auto Prev = std::prev(InsertPt);
        if (Prev->isDebugInstr() || Prev->isCFIInstruction()) {
          InsertPt = Prev;
          continue;
        }
        if (isMarkerInstr(*Prev)) {
          Prev->eraseFromParent();
          Changed = true;
          continue;
        }
        break;
      }

      constexpr unsigned DType_I32 = 17;
      constexpr unsigned TMA_TLOAD = 0;
      constexpr unsigned TMA_TSTORE = 1;
      constexpr unsigned TMA_TMOV = 2;
      constexpr unsigned CUBE_MAMULB = 0;
      constexpr unsigned CUBE_MAMULB_ACC = 2;
      constexpr unsigned CUBE_ACCCVT = 8;

      auto emitDim = [&](MachineBasicBlock &DimMBB, MachineBasicBlock::iterator DimInsertPt,
                         unsigned LoopNest, int64_t Imm) {
        if (Imm >= 0 && Imm <= 255) {
          BuildMI(DimMBB, DimInsertPt, DL, TII.get(LinxISA::C_B_DIMI))
              .addImm(LoopNest)
              .addImm(Imm);
          return;
        }
        const unsigned BDimOpc = (LoopNest == 0)   ? LinxISA::B_DIM_LB0
                               : (LoopNest == 1) ? LinxISA::B_DIM_LB1
                               :                  LinxISA::B_DIM_LB2;
        BuildMI(DimMBB, DimInsertPt, DL, TII.get(BDimOpc))
            .addReg(LinxISA::R0)
            .addImm(Imm);
      };

      auto emitDimReg = [&](MachineBasicBlock &DimMBB,
                            MachineBasicBlock::iterator DimInsertPt,
                            unsigned LoopNest, Register SrcReg) {
        const unsigned BDimOpc = (LoopNest == 0)   ? LinxISA::B_DIM_LB0
                               : (LoopNest == 1) ? LinxISA::B_DIM_LB1
                               :                  LinxISA::B_DIM_LB2;
        BuildMI(DimMBB, DimInsertPt, DL, TII.get(BDimOpc))
            .addReg(SrcReg)
            .addImm(0);
      };

      switch (PseudoMI->getOpcode()) {
      case LinxISA::PSEUDO_TMA_TLOAD: {
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Base = PseudoMI->getOperand(1).getReg();
        const int64_t Size = PseudoMI->getOperand(2).getImm();
        validateStrictTileSizeCode(Size, "TMA.TLOAD");

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);
        if (DstID >= 16)
          report_fatal_error("Linx: TMA.TLOAD dst must be in TILE0..TILE15");

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType_I32)
            .addImm(TMA_TLOAD);

        // Canonical descriptor-carrying TLOAD header:
        //   B.DIM(LB0/LB1) + B.ARG + B.IOR + B.IOT/B.IOT.
        //
        // The current PTO auto-mode bridge does not pass explicit layout/dim
        // metadata yet, so use bring-up defaults here:
        //   LB0/LB1 = 0, format=0 (Normal), RegSrc1/2=zero, RegDst=zero.
        emitDim(MBB, InsertPt, /*LoopNest=*/0, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, /*Imm=*/0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG))
            .addImm(0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0) // RegDst (bring-up: unused)
            .addReg(LinxISA::R0) // RegSrc0: stride bytes (default 0)
            .addReg(Base)        // RegSrc1: base pointer
            .addReg(LinxISA::R0);// RegSrc2: aux/layout source (default 0)

        // Canonical v0.4 contract: B.IOT is the canonical descriptor; encode the
        // tile destination register in the first absent source slot (SrcTile1)
        // and set S0V/S1V to indicate no tile inputs.
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(dstTileFieldFromRelRef(tileRelRefFromId(DstID))) // DstTile (hand)
            .addImm(0)      // S0R
            .addImm(1)      // S0V (absent)
            .addImm(0)      // S1R
            .addImm(1)      // S1V (absent)
            .addImm(0)      // SrcTile0
            .addImm(DstID)  // SrcTile1 (dst tile reg id)
            .addImm(Size)   // SizeCode (imm5)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TLOAD_ANY: {
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Base = PseudoMI->getOperand(1).getReg();
        const int64_t Size = PseudoMI->getOperand(2).getImm();
        validateStrictTileSizeCode(Size, "TMA.TLOAD");

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType_I32)
            .addImm(TMA_TLOAD);

        emitDim(MBB, InsertPt, /*LoopNest=*/0, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, /*Imm=*/0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG))
            .addImm(0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0)
            .addReg(LinxISA::R0)
            .addReg(Base)
            .addReg(LinxISA::R0);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(dstTileFieldFromRelRef(tileRelRefFromId(DstID)))
            .addImm(0)
            .addImm(1)
            .addImm(0)
            .addImm(1)
            .addImm(0)
            .addImm(DstID)
            .addImm(Size)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TLOAD_DESC: {
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Base = PseudoMI->getOperand(1).getReg();
        const int64_t DType = PseudoMI->getOperand(2).getImm();
        const int64_t Layout = PseudoMI->getOperand(3).getImm();
        const int64_t LB0 = PseudoMI->getOperand(4).getImm();
        const int64_t LB1 = PseudoMI->getOperand(5).getImm();
        const int64_t Size = PseudoMI->getOperand(6).getImm();
        const Register StrideReg = PseudoMI->getOperand(7).getReg();
        if (DType < 0 || DType > 31)
          report_fatal_error("Linx: TMA.TLOAD dtype must fit u5");
        validateStrictTileSizeCode(Size, "TMA.TLOAD");
        const uint64_t Dim0 = requirePositiveDimImm(LB0, "lb0", "TMA.TLOAD");
        const uint64_t Dim1 = requirePositiveDimImm(LB1, "lb1", "TMA.TLOAD");
        validateTileByteBudget("TMA.TLOAD", Dim0, Dim1, /*dim2=*/1u,
                               dtypeElementBitsForTileCheck(DType),
                               static_cast<uint64_t>(Size));
        if (!StrideReg)
          report_fatal_error("Linx: TMA.TLOAD requires stride register binding");

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);
        if (DstID >= 16)
          report_fatal_error("Linx: TMA.TLOAD dst must be in TILE0..TILE15");

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType)
            .addImm(TMA_TLOAD);
        emitDim(MBB, InsertPt, /*LoopNest=*/0, LB0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, LB1);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG)).addImm(Layout);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0)
            .addReg(StrideReg)
            .addReg(Base)
            .addReg(LinxISA::R0);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(dstTileFieldFromRelRef(tileRelRefFromId(DstID)))
            .addImm(0)
            .addImm(1)
            .addImm(0)
            .addImm(1)
            .addImm(0)
            .addImm(DstID)
            .addImm(Size)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TSTORE: {
        const Register Base = PseudoMI->getOperand(0).getReg();
        const Register Src = PseudoMI->getOperand(1).getReg();
        const int64_t Size = PseudoMI->getOperand(2).getImm();
        validateStrictTileSizeCode(Size, "TMA.TSTORE");

        const unsigned SrcID = tileRegIdFromReg(TRI, Src);
        const unsigned EncSrc = TileSourceCodes.lookup(PseudoMI).front();
        const bool SrcReuse = !PseudoMI->getOperand(1).isKill();

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType_I32)
            .addImm(TMA_TSTORE);

        // Canonical descriptor-carrying TSTORE header:
        //   B.DIM(LB0/LB1) + B.ARG + B.IOR + B.IOT/B.IOT.
        emitDim(MBB, InsertPt, /*LoopNest=*/0, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, /*Imm=*/0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG))
            .addImm(0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0) // RegDst: valid mask / flags (default 0)
            .addReg(LinxISA::R0) // RegSrc0: stride bytes (default 0)
            .addReg(Base)        // RegSrc1: base pointer
            .addReg(LinxISA::R0);// RegSrc2: aux/layout source (default 0)

        // Store: encode the source tile in SrcTile0 and mark it present (S0V=0).
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(dstTileFieldFromRelRef(tileRelRefFromId(SrcID))) // destination hand
            .addImm(SrcReuse ? 1 : 0) // S0R
            .addImm(0)      // S0V (present)
            .addImm(0)      // S1R
            .addImm(1)      // S1V (absent)
            .addImm(EncSrc) // SrcTile0 (architectural relative rank)
            .addImm(0)      // SrcTile1 (unused)
            .addImm(Size)   // SizeCode (imm5)
            .addReg(Src, RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TSTORE_DESC: {
        const Register Base = PseudoMI->getOperand(0).getReg();
        const Register Src = PseudoMI->getOperand(1).getReg();
        const int64_t DType = PseudoMI->getOperand(2).getImm();
        const int64_t Layout = PseudoMI->getOperand(3).getImm();
        const int64_t LB0 = PseudoMI->getOperand(4).getImm();
        const int64_t LB1 = PseudoMI->getOperand(5).getImm();
        const int64_t Size = PseudoMI->getOperand(6).getImm();
        const Register StrideReg = PseudoMI->getOperand(7).getReg();
        if (DType < 0 || DType > 31)
          report_fatal_error("Linx: TMA.TSTORE dtype must fit u5");
        validateStrictTileSizeCode(Size, "TMA.TSTORE");
        const uint64_t Dim0 = requirePositiveDimImm(LB0, "lb0", "TMA.TSTORE");
        const uint64_t Dim1 = requirePositiveDimImm(LB1, "lb1", "TMA.TSTORE");
        validateTileByteBudget("TMA.TSTORE", Dim0, Dim1, /*dim2=*/1u,
                               dtypeElementBitsForTileCheck(DType),
                               static_cast<uint64_t>(Size));
        if (!StrideReg)
          report_fatal_error("Linx: TMA.TSTORE requires stride register binding");
        const unsigned SrcID = tileRegIdFromReg(TRI, Src);
        const unsigned EncSrc = TileSourceCodes.lookup(PseudoMI).front();
        const bool SrcReuse = !PseudoMI->getOperand(1).isKill();

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType)
            .addImm(TMA_TSTORE);
        emitDim(MBB, InsertPt, /*LoopNest=*/0, LB0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, LB1);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG)).addImm(Layout);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0)
            .addReg(StrideReg)
            .addReg(Base)
            .addReg(LinxISA::R0);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(dstTileFieldFromRelRef(tileRelRefFromId(SrcID)))
            .addImm(SrcReuse ? 1 : 0)
            .addImm(0)
            .addImm(0)
            .addImm(1)
            .addImm(EncSrc)
            .addImm(0)
            .addImm(Size)
            .addReg(Src, RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TMOV: {
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Src = PseudoMI->getOperand(1).getReg();

        TileMeta Meta;
        Meta.SizeCode = static_cast<uint8_t>(PseudoMI->getOperand(2).getImm() & 0x1f);
        Meta.DataType = static_cast<uint8_t>(PseudoMI->getOperand(3).getImm() & 0x1f);
        Meta.Layout = PseudoMI->getOperand(4).getImm();
        Meta.HasLayout = (PseudoMI->getOperand(5).getImm() & 1) != 0;
        validateStrictTileSizeCode(Meta.SizeCode, "TMOV");

        const int64_t Mode = PseudoMI->getOperand(6).getImm();
        if (Mode != static_cast<int64_t>(TMovMode::V2V) &&
            Mode != static_cast<int64_t>(TMovMode::A2V))
          report_fatal_error("Linx: TMOV mode must be V2V(0) or A2V(1)");
        const bool IsA2V = Mode == static_cast<int64_t>(TMovMode::A2V);
        const bool SrcReuse = (PseudoMI->getOperand(7).getImm() & 1) != 0;
        if (IsA2V && SrcReuse)
          report_fatal_error("Linx: TMOV A2V mode does not allow src_reuse=1");

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);
        const TileRelRef DstRef = tileRelRefFromId(DstID);
        // Destination identity is compiler-internal; the descriptor publishes
        // only its architectural destination hand.
        const unsigned EncDstTile = DstID;

        unsigned EncSrc = 0;
        RegState SrcFlags = RegState::Implicit;
        if (!IsA2V) {
          EncSrc = TileSourceCodes.lookup(PseudoMI).front();
          (void)tileIdFromRelRef(DstRef);
          if (!SrcReuse)
            SrcFlags |= RegState::Kill;
        } else {
          // Validate destination relref in A2V mode too.
          (void)tileIdFromRelRef(DstRef);
        }

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(Meta.DataType)
            .addImm(TMA_TMOV);

        // B.ARG carries TMOV mode (strict profile: V2V + A2V).
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG)).addImm(Mode);

        if (!IsA2V) {
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
              .addImm(dstTileFieldFromRelRef(DstRef)) // DstTile (hand)
              .addImm(SrcReuse ? 1 : 0)               // S0R
              .addImm(0)                              // S0V (present)
              .addImm(0)                              // S1R
              .addImm(1)                              // S1V (absent)
              .addImm(EncSrc)                         // SrcTile0
              .addImm(EncDstTile)                     // SrcTile1 (dst tile id)
              .addImm(Meta.SizeCode)                  // SizeCode
              .addReg(Src, SrcFlags)
              .addReg(Dst, RegState::Define | RegState::Implicit);
        } else {
          // A2V: source is implicit accumulator state, so no explicit source
          // tile is bound in B.IOT.
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
              .addImm(dstTileFieldFromRelRef(DstRef)) // DstTile (hand)
              .addImm(0)                              // S0R
              .addImm(1)                              // S0V (absent)
              .addImm(0)                              // S1R
              .addImm(1)                              // S1V (absent)
              .addImm(0)                              // SrcTile0 (unused)
              .addImm(EncDstTile)                     // SrcTile1 (dst tile id)
              .addImm(Meta.SizeCode)                  // SizeCode
              .addReg(Dst, RegState::Define | RegState::Implicit);
        }

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_CUBE_MAMULB: {
        // Expand into two blocks:
        //   BSTART.CUBE(MAMULB) + dims + B.IOT(srcA, srcB) -> ACC
        //   BSTART.CUBE(ACCCVT) + B.IOT(dst)              -> tile
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register SrcA = PseudoMI->getOperand(1).getReg();
        const Register SrcB = PseudoMI->getOperand(2).getReg();
        const int64_t M = PseudoMI->getOperand(3).getImm();
        const int64_t N = PseudoMI->getOperand(4).getImm();
        const int64_t K = PseudoMI->getOperand(5).getImm();
        validateCubeDimImm(M, "m", "CUBE.MAMULB");
        validateCubeDimImm(N, "n", "CUBE.MAMULB");
        validateCubeDimImm(K, "k", "CUBE.MAMULB");
        validateTileByteBudget("CUBE.MAMULB",
                               requirePositiveDimImm(M, "m", "CUBE.MAMULB"),
                               requirePositiveDimImm(N, "n", "CUBE.MAMULB"),
                               requirePositiveDimImm(K, "k", "CUBE.MAMULB"),
                               dtypeElementBitsForTileCheck(DType_I32),
                               std::nullopt);

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);
        if (DstID < 16)
          report_fatal_error("Linx: CUBE.ACCCVT dst must be in TILE16..TILE31");
        const unsigned Group = (DstID >> 3) & 0x1u;
        const unsigned Depth = DstID & 0x7u;

        const auto &SourceCodes = TileSourceCodes.lookup(PseudoMI);
        const unsigned EncA = SourceCodes[0];
        const unsigned EncB = SourceCodes[1];
        const bool ReuseA = !PseudoMI->getOperand(1).isKill();
        const bool ReuseB = !PseudoMI->getOperand(2).isKill();

        // First block: MAMULB
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType_I32)
            .addImm(CUBE_MAMULB);

        emitDim(MBB, InsertPt, /*LoopNest=*/0, M);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, N);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, K);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(4)    // DstTile (acc)
            .addImm(ReuseA ? 1 : 0) // S0R
            .addImm(0)    // S0V (present)
            .addImm(ReuseB ? 1 : 0) // S1R
            .addImm(0)    // S1V (present)
            .addImm(EncA) // SrcTile0 (architectural relative rank)
            .addImm(EncB) // SrcTile1 (architectural relative rank)
            .addImm(8)    // SizeCode (bring-up: 4KiB accumulator)
            .addReg(SrcA, RegState::Implicit)
            .addReg(SrcB, RegState::Implicit);

        // Second block: ACCCVT into dst tile.
        MachineFunction &MF = *MBB.getParent();
        auto *AccBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
        MF.insert(std::next(MBB.getIterator()), AccBB);
        AccBB->transferSuccessorsAndUpdatePHIs(&MBB);
        MBB.addSuccessor(AccBB);

        BuildMI(*AccBB, AccBB->end(), DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType_I32)
            .addImm(CUBE_ACCCVT);

        const unsigned DstKind =
            dstTileFieldFromRelRef(tileRelRefFromId(Depth | (Group << 3) | 16u));
        BuildMI(*AccBB, AccBB->end(), DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(DstKind)
            .addImm(0)       // S0R
            .addImm(1)       // S0V (absent)
            .addImm(0)       // S1R
            .addImm(1)       // S1V (absent)
            .addImm(0)       // SrcTile0 (unused)
            .addImm(16u | (Group << 3) | Depth) // SrcTile1 (dst tile reg id)
            .addImm(8)       // SizeCode (bring-up: 4KiB)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_CUBE_MAMULB_ACC: {
        // Expand into two blocks:
        //   BSTART.CUBE(MAMULB.ACC) + dims + B.IOT(srcA, srcB) -> ACC
        //   BSTART.CUBE(ACCCVT)     + B.IOT(dst)              -> tile
        //
        // The explicit ACC operand is preserved as an implicit use so SSA
        // dependencies are maintained (the emulator models the accumulator as
        // implicit state).
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Acc = PseudoMI->getOperand(1).getReg();
        const Register SrcA = PseudoMI->getOperand(2).getReg();
        const Register SrcB = PseudoMI->getOperand(3).getReg();
        const int64_t M = PseudoMI->getOperand(4).getImm();
        const int64_t N = PseudoMI->getOperand(5).getImm();
        const int64_t K = PseudoMI->getOperand(6).getImm();
        validateCubeDimImm(M, "m", "CUBE.MAMULB.ACC");
        validateCubeDimImm(N, "n", "CUBE.MAMULB.ACC");
        validateCubeDimImm(K, "k", "CUBE.MAMULB.ACC");
        validateTileByteBudget(
            "CUBE.MAMULB.ACC",
            requirePositiveDimImm(M, "m", "CUBE.MAMULB.ACC"),
            requirePositiveDimImm(N, "n", "CUBE.MAMULB.ACC"),
            requirePositiveDimImm(K, "k", "CUBE.MAMULB.ACC"),
            dtypeElementBitsForTileCheck(DType_I32), std::nullopt);

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);
        if (DstID < 16)
          report_fatal_error("Linx: CUBE.ACCCVT dst must be in TILE16..TILE31");
        const unsigned Group = (DstID >> 3) & 0x1u;
        const unsigned Depth = DstID & 0x7u;

        const auto &SourceCodes = TileSourceCodes.lookup(PseudoMI);
        const unsigned EncA = SourceCodes[0];
        const unsigned EncB = SourceCodes[1];
        const bool ReuseA = !PseudoMI->getOperand(2).isKill();
        const bool ReuseB = !PseudoMI->getOperand(3).isKill();

        // First block: MAMULB.ACC
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType_I32)
            .addImm(CUBE_MAMULB_ACC);

        emitDim(MBB, InsertPt, /*LoopNest=*/0, M);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, N);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, K);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(4)    // DstTile (acc)
            .addImm(ReuseA ? 1 : 0) // S0R
            .addImm(0)    // S0V (present)
            .addImm(ReuseB ? 1 : 0) // S1R
            .addImm(0)    // S1V (present)
            .addImm(EncA) // SrcTile0 (architectural relative rank)
            .addImm(EncB) // SrcTile1 (architectural relative rank)
            .addImm(8)    // SizeCode (bring-up: 4KiB accumulator)
            .addReg(SrcA, RegState::Implicit)
            .addReg(SrcB, RegState::Implicit)
            .addReg(Acc, RegState::Implicit);

        // Second block: ACCCVT into dst tile.
        MachineFunction &MF = *MBB.getParent();
        auto *AccBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
        MF.insert(std::next(MBB.getIterator()), AccBB);
        AccBB->transferSuccessorsAndUpdatePHIs(&MBB);
        MBB.addSuccessor(AccBB);

        BuildMI(*AccBB, AccBB->end(), DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType_I32)
            .addImm(CUBE_ACCCVT);

        const unsigned DstKind =
            dstTileFieldFromRelRef(tileRelRefFromId(Depth | (Group << 3) | 16u));
        BuildMI(*AccBB, AccBB->end(), DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(DstKind)
            .addImm(0)       // S0R
            .addImm(1)       // S0V (absent)
            .addImm(0)       // S1R
            .addImm(1)       // S1V (absent)
            .addImm(0)       // SrcTile0 (unused)
            .addImm(16u | (Group << 3) | Depth) // SrcTile1 (dst tile reg id)
            .addImm(8)       // SizeCode (bring-up: 4KiB)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_CUBE_ACCCVT: {
        // Expand into one block:
        //   BSTART.CUBE(ACCCVT) + B.ARG(qarg0) + B.IOT(dst)
        //
        // qarg1 is reserved for follow-on quant wiring and must be 0 in PR5.
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Acc = PseudoMI->getOperand(1).getReg();
        const int64_t Size = PseudoMI->getOperand(2).getImm();
        const int64_t DType = PseudoMI->getOperand(3).getImm();
        const int64_t QArg0 = PseudoMI->getOperand(4).getImm();
        const int64_t QArg1 = PseudoMI->getOperand(5).getImm();

        validateStrictTileSizeCode(Size, "CUBE.ACCCVT");
        if (DType < 0 || DType > 31)
          report_fatal_error("Linx: CUBE.ACCCVT dtype must fit u5");
        if (QArg1 != 0)
          report_fatal_error(
              "Linx: CUBE.ACCCVT currently requires qarg1=0 in canonical v0.4");

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);
        if (DstID < 16)
          report_fatal_error("Linx: CUBE.ACCCVT dst must be in TILE16..TILE31");
        const unsigned DstKind =
            dstTileFieldFromRelRef(tileRelRefFromId(DstID));

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType)
            .addImm(CUBE_ACCCVT);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG)).addImm(QArg0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(DstKind)
            .addImm(0)       // S0R
            .addImm(1)       // S0V (absent)
            .addImm(0)       // S1R
            .addImm(1)       // S1V (absent)
            .addImm(0)       // SrcTile0 (unused)
            .addImm(DstID)   // SrcTile1 (dst tile id)
            .addImm(Size)    // SizeCode
            .addReg(Acc, RegState::Implicit)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TEPL_UNARY:
      case LinxISA::PSEUDO_TEPL_BINARY:
      case LinxISA::PSEUDO_TEPL_BINARY_SCALAR:
      case LinxISA::PSEUDO_TEPL_SPLAT: {
        const unsigned Opc = PseudoMI->getOpcode();
        const bool IsUnary = Opc == LinxISA::PSEUDO_TEPL_UNARY;
        const bool IsBinary = Opc == LinxISA::PSEUDO_TEPL_BINARY;
        const bool IsBinaryScalar = Opc == LinxISA::PSEUDO_TEPL_BINARY_SCALAR;
        const bool IsSplat = Opc == LinxISA::PSEUDO_TEPL_SPLAT;
        const char *Ctx = IsUnary
                              ? "TEPL.UNARY"
                              : (IsBinary ? "TEPL.BINARY"
                                          : (IsBinaryScalar ? "TEPL.BINARY.SCALAR"
                                                            : "TEPL.SPLAT"));

        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register SrcA = (IsUnary || IsBinary || IsBinaryScalar)
                                  ? PseudoMI->getOperand(1).getReg()
                                  : Register();
        const Register SrcB = IsBinary ? PseudoMI->getOperand(2).getReg() : Register();
        const Register SrcS = IsBinaryScalar
                                  ? PseudoMI->getOperand(2).getReg()
                                  : (IsSplat ? PseudoMI->getOperand(1).getReg()
                                             : Register());
        const int64_t TileOpcode =
            PseudoMI->getOperand(IsUnary ? 2 : (IsBinary ? 3 : (IsBinaryScalar ? 3 : 2)))
                .getImm();
        const int64_t Size =
            PseudoMI->getOperand(IsUnary ? 3 : (IsBinary ? 4 : (IsBinaryScalar ? 4 : 3)))
                .getImm();
        const int64_t DType =
            PseudoMI->getOperand(IsUnary ? 4 : (IsBinary ? 5 : (IsBinaryScalar ? 5 : 4)))
                .getImm();
        const int64_t Mode =
            IsBinaryScalar
                ? PseudoMI->getOperand(6).getImm()
                : (IsSplat ? PseudoMI->getOperand(5).getImm()
                           : static_cast<int64_t>(TEPLMode::VV));

        validateWhitelistedTEPLTileOpcode(TileOpcode, Ctx);
        validateStrictTileSizeCode(Size, Ctx);
        if (DType < 0 || DType > 31)
          report_fatal_error(Twine("Linx: ") + Ctx + " dtype must fit u5");
        if (Mode < 0 || Mode > 2)
          report_fatal_error(Twine("Linx: ") + Ctx + " mode must be in range 0..2");
        if (IsBinaryScalar && Mode != static_cast<int64_t>(TEPLMode::VS))
          report_fatal_error("Linx: TEPL.BINARY.SCALAR requires mode=1 (VS)");
        if (IsSplat && Mode != static_cast<int64_t>(TEPLMode::SV))
          report_fatal_error("Linx: TEPL.SPLAT requires mode=2 (SV)");
        if ((IsUnary || IsBinary) && Mode != static_cast<int64_t>(TEPLMode::VV))
          report_fatal_error("Linx: TEPL.UNARY/BINARY require mode=0 (VV)");

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);
        const TileRelRef DstRef = tileRelRefFromId(DstID);
        const bool HasS0Tile = IsUnary || IsBinary || IsBinaryScalar;
        const bool HasS1Tile = IsBinary;
        const auto &SourceCodes = TileSourceCodes.lookup(PseudoMI);
        const unsigned EncA = HasS0Tile ? SourceCodes[0] : 0u;
        const unsigned EncB = HasS1Tile ? SourceCodes[1] : 0u;
        const bool ReuseA = HasS0Tile && !PseudoMI->getOperand(1).isKill();
        const bool ReuseB = HasS1Tile && !PseudoMI->getOperand(2).isKill();

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TEPL))
            .addImm(DType)
            .addImm(TileOpcode);
        // Legacy TEPL builtins carry the tile byte size and dtype but not an
        // explicit logical shape.  Canonical v0.57 TEPL reductions, expands,
        // transpose, sort, gather, and scatter require B.DIM metadata, so
        // materialize the full-tile profile used by the public PTO wrappers.
        //
        // Keep LB0 at 32 lanes when possible and derive LB1 from the active
        // element count.  This yields 32x32 for the 4 KiB FP32/S32 profile and
        // 32x128 for the 4 KiB S8/U8 profile.  A future shape-carrying
        // intrinsic may override this compatibility profile explicitly.
        const unsigned ElemBits = dtypeElementBitsForTileCheck(DType);
        const uint64_t TileBytes = uint64_t{1} << (Size + 4);
        const uint64_t TileElems =
            ElemBits == 0 ? 0 : (TileBytes * 8u) / ElemBits;
        uint64_t LB0 = (TileElems % 32u) == 0u ? 32u : 1u;
        uint64_t LB1 = LB0 == 0 ? 0 : TileElems / LB0;
        if (LB0 > 0 && LB0 <= 255 && LB1 > 0 && LB1 <= 255) {
          emitDim(MBB, InsertPt, /*LoopNest=*/0, LB0);
          emitDim(MBB, InsertPt, /*LoopNest=*/1, LB1);
        } else {
          report_fatal_error("Linx: TEPL full-tile profile does not fit B.DIM");
        }
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG)).addImm(Mode);
        if (IsBinaryScalar || IsSplat) {
          // TEPL scalar extensions bind scalar source through B.IOR.
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
              .addReg(LinxISA::R0) // RegDst (unused)
              .addReg(SrcS)        // RegSrc0 (scalar)
              .addReg(LinxISA::R0) // RegSrc1
              .addReg(LinxISA::R0) // RegSrc2
              .addReg(SrcS, RegState::Implicit);
        }

        // One last-marked descriptor binds the input ranks and publishes the
        // output hand; a second source-less descriptor would publish a second
        // unwritten output.
        auto Desc = BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
                        .addImm(dstTileFieldFromRelRef(DstRef)) // destination hand
                        .addImm(ReuseA ? 1 : 0)                 // S0R
                        .addImm(HasS0Tile ? 0 : 1)              // S0V
                        .addImm(ReuseB ? 1 : 0)                 // S1R
                        .addImm(HasS1Tile ? 0 : 1)              // S1V
                        .addImm(EncA)                           // SrcTile0
                        .addImm(EncB)                           // SrcTile1
                        .addImm(Size);                          // SizeCode
        if (HasS0Tile)
          Desc.addReg(SrcA, RegState::Implicit);
        if (HasS1Tile)
          Desc.addReg(SrcB, RegState::Implicit);
        Desc.addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_VPAR_TADD:
      case LinxISA::PSEUDO_VPAR_TSUB:
      case LinxISA::PSEUDO_VTILE_ADD:
      case LinxISA::PSEUDO_VTILE_SUB: {
        // Expand into a VPAR decoupled header that binds:
        // - input tiles through TA/TB
        // - the output tile through TO
        //
        // A canonical B.IOT descriptor binds both the sources and destination.
        // Keep them in one last-marked descriptor: a second, source-less output
        // descriptor would declare another architectural output that the body
        // never writes.
        //
        // The out-of-line body is a single-lane snippet that executes:
        //   load TA, load TB, add/sub, store TO
        // and terminates at C.BSTOP so QEMU can replay it across LB0/LB1.
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register SrcA = PseudoMI->getOperand(1).getReg();
        const Register SrcB = PseudoMI->getOperand(2).getReg();
        const bool IsAdd =
            (PseudoMI->getOpcode() == LinxISA::PSEUDO_VPAR_TADD) ||
            (PseudoMI->getOpcode() == LinxISA::PSEUDO_VTILE_ADD);
        const int64_t Size =
            (PseudoMI->getOpcode() == LinxISA::PSEUDO_VPAR_TADD ||
             PseudoMI->getOpcode() == LinxISA::PSEUDO_VPAR_TSUB)
                ? PseudoMI->getOperand(3).getImm()
                : 8; // 4KiB tiles (SizeCode=8)

        const unsigned DstID = tileRegIdFromReg(TRI, Dst);
        const auto &SourceCodes = TileSourceCodes.lookup(PseudoMI);
        const unsigned EncA = SourceCodes[0];
        const unsigned EncB = SourceCodes[1];
        const bool ReuseA = !PseudoMI->getOperand(1).isKill();
        const bool ReuseB = !PseudoMI->getOperand(2).isKill();

        // Derive a compact 2-D iteration space for the tile:
        // - LB0=64 elements (256B row stride => lc1<<8)
        // - LB1=bytes/256
        uint64_t Bytes = 0;
        if (Size >= 0 && Size < 60) {
          Bytes = 1ull << (static_cast<unsigned>(Size) + 4u);
        }
        if (Bytes == 0 || (Bytes & 3u) != 0 || Bytes > 4096u ||
            (Bytes % 256u) != 0) {
          report_fatal_error("Linx: VPAR tile binop requires 256B-aligned tile size <=4KB");
        }
        const int64_t LB0 = 64;
        const int64_t LB1 = static_cast<int64_t>(Bytes / 256u);

        MCSymbol *BodySym = IsAdd ? getOrCreateVTileAddBodySym()
                                  : getOrCreateVTileSubBodySym();

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_VPAR)).addImm(0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_TEXT)).addSym(BodySym);

        // Bind TA/TB and TO in one canonical, last-marked descriptor.
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
            .addImm(dstTileFieldFromRelRef(tileRelRefFromId(DstID))) // destination hand
            .addImm(ReuseA ? 1 : 0)        // S0R
            .addImm(0)                     // S0V (present)
            .addImm(ReuseB ? 1 : 0)        // S1R
            .addImm(0)                     // S1V (present)
            .addImm(EncA)                  // SrcTile0 (TA relative rank)
            .addImm(EncB)                  // SrcTile1 (TB relative rank)
            .addImm(Size)                  // SizeCode
            .addReg(SrcA, RegState::Implicit)
            .addReg(SrcB, RegState::Implicit)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        emitDim(MBB, InsertPt, /*LoopNest=*/0, LB0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, LB1);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_VBLOCK_LAUNCH:
      case LinxISA::PSEUDO_VBLOCK_LAUNCH_DYN1: {
        // Expand into a decoupled vector block header:
        //   BSTART.{MSEQ,MPAR,VSEQ,VPAR} + B.TEXT body + B.IOR(binds) +
        //   B.DIM(LB0..2)
        const bool DynDim1 =
            (PseudoMI->getOpcode() == LinxISA::PSEUDO_VBLOCK_LAUNCH_DYN1);
        const int64_t VKind = PseudoMI->getOperand(0).getImm();
        const int64_t Dim0 = PseudoMI->getOperand(1).getImm();
        const int64_t Dim1Imm = DynDim1 ? 0 : PseudoMI->getOperand(2).getImm();
        const Register Dim1Reg = DynDim1 ? PseudoMI->getOperand(2).getReg()
                                         : Register();
        const int64_t Dim2 = PseudoMI->getOperand(3).getImm();
        const int64_t AttrBits = PseudoMI->getOperand(4).getImm();

        const Register Bind0 = PseudoMI->getOperand(5).getReg();
        const Register Bind1 = PseudoMI->getOperand(6).getReg();
        const Register Bind2 = PseudoMI->getOperand(7).getReg();
        const Register Bind3 = PseudoMI->getOperand(8).getReg();
        const Register Bind4 = PseudoMI->getOperand(9).getReg();
        const Register Bind5 = PseudoMI->getOperand(10).getReg();
        const Register Bind6 = PseudoMI->getOperand(11).getReg();
        const Register Bind7 = PseudoMI->getOperand(12).getReg();
        const Register Bind8 = PseudoMI->getOperand(13).getReg();
        const Register Bind9 = PseudoMI->getOperand(14).getReg();
        const Register Bind10 = PseudoMI->getOperand(15).getReg();
        const Register Bind11 = PseudoMI->getOperand(16).getReg();

        const unsigned Mode = 0; // bring-up default
        const unsigned BStartOpc =
            (VKind == 0)   ? LinxISA::BSTART_MSEQ
            : (VKind == 1) ? LinxISA::BSTART_MPAR
            : (VKind == 2) ? LinxISA::BSTART_VSEQ
            : (VKind == 3) ? LinxISA::BSTART_VPAR
                           : 0;
        if (!BStartOpc)
          report_fatal_error("Linx: vblock.launch vkind must be 0(MSEQ), 1(MPAR), 2(VSEQ), or 3(VPAR)");

        BuildMI(MBB, InsertPt, DL, TII.get(BStartOpc)).addImm(Mode);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_TEXT))
            .addSym(getOrCreateVBlockBodySym());

        if (AttrBits < 0 || (static_cast<uint64_t>(AttrBits) & ~0x003fffffull))
          report_fatal_error("Linx: vblock.launch attr_bits must fit 22 bits");
        const uint32_t Attr = static_cast<uint32_t>(AttrBits);
        const uint32_t AttrAQRLMask = (1u << 18) | (1u << 21);
        bool EmitLocalScratch = false;
        unsigned LocalScratchSizeCode = 0;
        Attribute ScratchAttr =
            MF.getFunction().getFnAttribute("linx-vblock-ts-bytes");
        if (ScratchAttr.isStringAttribute()) {
          uint64_t ScratchBytes = 0;
          if (ScratchAttr.getValueAsString().getAsInteger(10, ScratchBytes)) {
            report_fatal_error(
                "Linx: linx-vblock-ts-bytes must be a decimal byte count");
          }
          if (ScratchBytes != 0) {
            auto SizeCode = tileBytesToSizeCode(ScratchBytes);
            if (!SizeCode) {
              report_fatal_error(
                  "Linx: linx-vblock-ts-bytes must be a power-of-two byte size in [16,4096]");
            }
            EmitLocalScratch = true;
            LocalScratchSizeCode = *SizeCode;
          }
        }
        if ((Attr & ~AttrAQRLMask) != 0u) {
          report_fatal_error(
              "Linx: vblock.launch only supports aq/rl B.CATR bits in canonical v0.57");
        }
        if (Attr != 0u) {
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_CATR))
              .addImm(0)                    // trap
              .addImm(0)                    // DR
              .addImm((Attr >> 18) & 0x1u) // aq
              .addImm(0)                    // atom
              .addImm(0)                    // far
              .addImm((Attr >> 21) & 0x1u); // rl
        }

        auto emitIOR = [&](Register A, Register B, Register C) {
          if (A == LinxISA::R0 && B == LinxISA::R0 && C == LinxISA::R0)
            return;
          // Canonical v0.4 contract: bind RI registers as an ordered namespace
          // via B.IOR sources (RegSrc1, RegSrc0, RegSrc2).
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
              .addReg(LinxISA::R0) // RegDst (unused in bring-up)
              .addReg(B)           // RegSrc0
              .addReg(A)           // RegSrc1
              .addReg(C);          // RegSrc2
        };

        emitIOR(Bind0, Bind1, Bind2);
        emitIOR(Bind3, Bind4, Bind5);
        emitIOR(Bind6, Bind7, Bind8);
        emitIOR(Bind9, Bind10, Bind11);

        if (EmitLocalScratch) {
          // Reserve the first two output descriptors for TO/TS so the body
          // can use the canonical `.local` output-tile order.
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
              .addImm(dstTileFieldFromHand(TileHand::T))
              .addImm(0)
              .addImm(1)
              .addImm(0)
              .addImm(1)
              .addImm(0)
              .addImm(0)
              .addImm(0);
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_SIZE_G1))
              .addImm(dstTileFieldFromHand(TileHand::U))
              .addImm(0)
              .addImm(1)
              .addImm(0)
              .addImm(1)
              .addImm(0)
              .addImm(8)
              .addImm(LocalScratchSizeCode);
        }

        emitDim(MBB, InsertPt, /*LoopNest=*/0, Dim0);
        if (DynDim1)
          emitDimReg(MBB, InsertPt, /*LoopNest=*/1, Dim1Reg);
        else
          emitDim(MBB, InsertPt, /*LoopNest=*/1, Dim1Imm);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, Dim2);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTOP));

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      default:
        break;
      }
    }

    // Discover physical registers that are constant at each MBB entry.
    //
    // This is used by late peepholes to recognize patterns like:
    //   sll tmp, shreg; srl tmp, shreg   (where shreg == 32)
    // even when the shift amount was materialized in a predecessor block.
    //
    // We keep this conservative: only track constants defined by a small set of
    // immediate materializations, and drop any register that is overwritten by
    // an unrecognized def.
    SmallVector<DenseMap<Register, int64_t>, 0> EntryConstRegs;

    auto tryGetConstDef = [&](const MachineInstr &MI) -> std::optional<int64_t> {
      auto isFromZero = [&](unsigned OpNo) -> bool {
        if (OpNo >= MI.getNumOperands())
          return false;
        const MachineOperand &MO = MI.getOperand(OpNo);
        return MO.isReg() && MO.getReg() == LinxISA::R0;
      };

      switch (MI.getOpcode()) {
      case LinxISA::ADDIri:
      case LinxISA::ADDIWri:
        if (!isFromZero(/*OpNo=*/1) || MI.getNumOperands() < 3 ||
            !MI.getOperand(2).isImm())
          return std::nullopt;
        return MI.getOperand(2).getImm();
      case LinxISA::SUBIri:
      case LinxISA::SUBIWri:
        if (!isFromZero(/*OpNo=*/1) || MI.getNumOperands() < 3 ||
            !MI.getOperand(2).isImm())
          return std::nullopt;
        return -MI.getOperand(2).getImm();
      case LinxISA::LUI:
        if (MI.getNumOperands() < 2 || !MI.getOperand(1).isImm())
          return std::nullopt;
        return MI.getOperand(1).getImm() << 12;
      default:
        return std::nullopt;
      }
    };

    unsigned MaxMBBNumber = 0;
    for (const MachineBasicBlock &MBB : MF)
      MaxMBBNumber = std::max(MaxMBBNumber, unsigned(MBB.getNumber()));
    EntryConstRegs.resize(MaxMBBNumber + 1);
    SmallVector<DenseMap<Register, int64_t>, 0> ExitConstRegs(MaxMBBNumber + 1);
    SmallVector<bool, 0> InWorklist(MaxMBBNumber + 1, false);

    auto meetPreds = [&](const MachineBasicBlock &MBB,
                         DenseMap<Register, int64_t> &Out) -> void {
      Out.clear();
      if (MBB.pred_empty())
        return;

      // Start from the first predecessor's exit constants.
      const MachineBasicBlock *FirstPred = *MBB.pred_begin();
      Out = ExitConstRegs[FirstPred->getNumber()];

      // Intersect with remaining preds.
      for (auto PI = std::next(MBB.pred_begin()), PE = MBB.pred_end(); PI != PE;
           ++PI) {
        const DenseMap<Register, int64_t> &POut = ExitConstRegs[(*PI)->getNumber()];
        SmallVector<Register, 16> ToErase;
        for (auto &KV : Out) {
          auto It = POut.find(KV.first);
          if (It == POut.end() || It->second != KV.second)
            ToErase.push_back(KV.first);
        }
        for (Register R : ToErase)
          Out.erase(R);
      }
    };

	    auto transferBlock = [&](const MachineBasicBlock &MBB,
	                             const DenseMap<Register, int64_t> &In,
	                             DenseMap<Register, int64_t> &Out) -> void {
	      Out = In;
	      for (const MachineInstr &MI : MBB) {
	        if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
	          continue;
	        for (const MachineOperand &MO : MI.operands()) {
	          if (!MO.isReg() || !MO.isDef() || MO.isImplicit())
	            continue;
	          Register Reg = MO.getReg();
	          if (!Reg.isPhysical())
	            continue;
	          if (Reserved.test(Reg))
	            continue;
	          if (auto V = tryGetConstDef(MI)) {
	            Out[Reg] = *V;
	            continue;
	          }

	          auto tryGetCopyConst = [&](const MachineInstr &MI) -> std::optional<int64_t> {
	            const unsigned Opc = MI.getOpcode();
	            const bool IsAddCopy = (Opc == LinxISA::ADDrr || Opc == LinxISA::ADDWrr);
	            if (!IsAddCopy)
	              return std::nullopt;
	            if (MI.getNumOperands() < 3 || !MI.getOperand(1).isReg() ||
	                !MI.getOperand(2).isReg())
	              return std::nullopt;
	            const Register A = MI.getOperand(1).getReg();
	            const Register B = MI.getOperand(2).getReg();
	            if (A == LinxISA::R0) {
	              auto It = Out.find(B);
	              if (It != Out.end())
	                return It->second;
	            }
	            if (B == LinxISA::R0) {
	              auto It = Out.find(A);
	              if (It != Out.end())
	                return It->second;
	            }
	            return std::nullopt;
	          };

	          if (auto V = tryGetCopyConst(MI)) {
	            Out[Reg] = *V;
	            continue;
	          }

	          Out.erase(Reg);
	        }
	      }
	    };

    // Worklist solver.
    SmallVector<const MachineBasicBlock *, 64> Worklist;
    for (const MachineBasicBlock &MBB : MF) {
      Worklist.push_back(&MBB);
      InWorklist[MBB.getNumber()] = true;
    }

    while (!Worklist.empty()) {
      const MachineBasicBlock *MBB = Worklist.pop_back_val();
      InWorklist[MBB->getNumber()] = false;

      DenseMap<Register, int64_t> NewIn;
      meetPreds(*MBB, NewIn);

      if (NewIn != EntryConstRegs[MBB->getNumber()]) {
        EntryConstRegs[MBB->getNumber()] = NewIn;
      }

      DenseMap<Register, int64_t> NewOut;
      transferBlock(*MBB, EntryConstRegs[MBB->getNumber()], NewOut);
      if (NewOut != ExitConstRegs[MBB->getNumber()]) {
        ExitConstRegs[MBB->getNumber()] = NewOut;
        for (const MachineBasicBlock *Succ : MBB->successors()) {
          unsigned N = Succ->getNumber();
          if (!InWorklist[N]) {
            Worklist.push_back(Succ);
            InWorklist[N] = true;
          }
        }
      }
    }

    auto findSetcInsertPt = [&](MachineBasicBlock &MBB, MachineInstr &Anchor,
                                Register LHS, Register RHS)
        -> MachineBasicBlock::iterator {
      MachineInstr *InsertAfter = nullptr;
      for (MachineInstr &MI : MBB) {
        if (&MI == &Anchor)
          break;
        if (MI.isDebugInstr() || isMarkerInstr(MI))
          continue;
        if ((LHS && MI.definesRegister(LHS, &TRI)) ||
            (RHS && MI.definesRegister(RHS, &TRI))) {
          InsertAfter = &MI;
        }
      }

      if (InsertAfter)
        return std::next(InsertAfter->getIterator());
      return Anchor.getIterator();
    };

    for (MachineBasicBlock &MBB : MF) {
      enum class ExitKind {
        Fall,
        Direct,
        Cond,
        Call,
        Ret,
        Ind,
        ICall,
      };

      // Decoupled out-of-line bodies are linear snippets referenced via B.TEXT
      // and must not be wrapped or rewritten by Blockify.
      if (DecoupledBodyBBs.contains(&MBB))
        continue;

      // Frame prologue/epilogue macros (FENTRY/FEXIT/FRET.*) are standalone
      // blocks in LinxISA: they already contain the required block markers and
      // micro-ops for stack/register management. Do not surround them with
      // BSTART/BSTOP or attempt to rewrite their control-flow.
		      if (isStandaloneFrameMacroBlock(MBB)) {
		        // If the pass runs twice, strip any stale explicit markers.
		        for (auto It = MBB.begin(); It != MBB.end();) {
		          if (isMarkerInstr(*It)) {
		            It = MBB.erase(It);
		            Changed = true;
		            continue;
		          }
		          ++It;
		        }
		        continue;
		      }

      // Tile blocks (TAU) have their own named-TMA/BSTART.CUBE headers and
      // must not be wrapped by standard BSTART.STD or have T/U-hand queue
      // remapping applied.
      auto isStdBStartOpcode = [&](unsigned Opc) -> bool {
        switch (Opc) {
        case LinxISA::CBSTART_STD:
        case LinxISA::BSTART_STD_FALL:
        case LinxISA::BSTART_STD_DIRECT:
        case LinxISA::BSTART_STD_COND:
        case LinxISA::BSTART_STD_CALL:
        case LinxISA::BSTART_STD_IND:
        case LinxISA::BSTART_STD_ICALL:
        case LinxISA::BSTART_STD_RET:
          return true;
        default:
          return false;
        }
      };

      bool IsTileBlock = false;
      for (MachineInstr &MI : MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction() || MI.isPHI())
          continue;
        // Be robust if the pass runs twice: skip any stale standard markers
        // and detect the tile header that follows.
        if (isStdBStartOpcode(MI.getOpcode()))
          continue;
        IsTileBlock = isTileBlockStartInstr(MI);
        break;
      }

			      auto isPhysRegLiveOutOfBlock = [&](Register Reg) -> bool {
		        // Physical register live-in sets only track "use before def" within a
		        // block, so a reg that is merely live-through a successor (not used
	        // until later) may not appear as live-in to an immediate successor.
	        // Conservatively walk successors and detect any reachable use of Reg
	        // before it is redefined.
	        SmallVector<const MachineBasicBlock *, 8> Worklist;
	        SmallPtrSet<const MachineBasicBlock *, 16> Visited;

	        for (const MachineBasicBlock *Succ : MBB.successors()) {
	          if (Succ)
	            Worklist.push_back(Succ);
	        }

	        while (!Worklist.empty()) {
	          const MachineBasicBlock *Succ = Worklist.pop_back_val();
	          if (!Visited.insert(Succ).second)
	            continue;

	          // If the successor explicitly records Reg as live-in, we are done.
	          if (Succ->isLiveIn(Reg))
	            return true;

	          bool DefinedInSucc = false;
	          for (const MachineInstr &MI : *Succ) {
	            if (MI.isDebugInstr() || MI.isCFIInstruction() ||
	                isMarkerInstr(MI))
	              continue;

	            if (MI.readsRegister(Reg, &TRI))
	              return true;

	            if (MI.definesRegister(Reg, &TRI)) {
	              DefinedInSucc = true;
	              break;
	            }
	          }

	          // No read and no def: Reg is live-through this successor, so keep
	          // searching down the CFG.
	          if (!DefinedInSucc) {
	            for (const MachineBasicBlock *Succ2 : Succ->successors()) {
	              if (Succ2)
	                Worklist.push_back(Succ2);
	            }
	          }
	        }

		        return false;
		      };

	      auto hasSingleNonDbgUseInMBB =
	          [&](Register Reg, const MachineInstr *UserMI,
	              const MachineInstr *IgnoreMI) -> bool {
	        // Cross-block users are not visible in the local MBB scan below.
	        // Guard virtual registers up front so we never fold a producer that
	        // still has uses in successor blocks.
	        if (Reg.isVirtual() && !MRI.hasOneNonDBGUse(Reg))
	          return false;
	        // After regalloc, physical registers may also have cross-block users.
	        // Be conservative: if the physreg is live-out of this block, don't
	        // claim it has a single use based on the local scan.
	        if (Reg.isPhysical() && isPhysRegLiveOutOfBlock(Reg))
	          return false;

	        unsigned Count = 0;
	        for (const MachineInstr &MI : MBB) {
	          if (MI.isDebugInstr() || isMarkerInstr(MI))
	            continue;
	          if (&MI == IgnoreMI)
	            continue;
	          for (const MachineOperand &MO : MI.operands()) {
	            if (!MO.isReg() || MO.isDef())
	              continue;
	            if (MO.getReg() != Reg)
	              continue;
	            ++Count;
	            if (&MI != UserMI || Count > 1)
	              return false;
	          }
	        }
	        return Count == 1;
	      };

	      auto getPhysRegConstAtMBBEntry = [&](Register Reg) -> std::optional<int64_t> {
	        if (!Reg || !Reg.isPhysical())
	          return std::nullopt;
	        auto &Map = EntryConstRegs[MBB.getNumber()];
	        auto It = Map.find(Reg);
	        if (It == Map.end())
	          return std::nullopt;
	        return It->second;
	      };

	      // Match a 32-bit zero-extend implemented as a shift pair:
	      //   tmp1 = sll x, shamt
	      //   tmp2 = srl tmp1, shamt
	      // with shamt == 32 (constant). Returns the original (pre-zext) source.
	      auto matchZextWByShiftPair =
	          [&](MachineInstr &UseMI, Register ZextReg, Register &OrigSrc,
	              MachineInstr *&SllMIOut, MachineInstr *&SrlMIOut) -> bool {
	        OrigSrc = Register();
	        SllMIOut = nullptr;
	        SrlMIOut = nullptr;
	        if (!ZextReg)
	          return false;

	        // Find defining SRLrr in this block.
	        MachineInstr *SrlMI = nullptr;
	        for (auto It = UseMI.getIterator(); It != MBB.begin();) {
	          --It;
	          MachineInstr &MI = *It;
	          if (MI.isDebugInstr() || isMarkerInstr(MI))
	            continue;
	          if (!MI.definesRegister(ZextReg, &TRI))
	            continue;
	          if (MI.getOpcode() != LinxISA::SRLrr || MI.getNumOperands() < 3 ||
	              !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
	            return false;
	          SrlMI = &MI;
	          break;
	        }
	        if (!SrlMI)
	          return false;

	        const Register Tmp1 = SrlMI->getOperand(1).getReg();
	        const Register ShAmtReg = SrlMI->getOperand(2).getReg();
	        if (!Tmp1 || !ShAmtReg)
	          return false;

	        // Resolve constant shift amount (local def in block; else function-constant).
	        auto getConstShiftAmt = [&](MachineInstr &Anchor) -> std::optional<int64_t> {
	          for (auto DI = Anchor.getIterator(); DI != MBB.begin();) {
	            --DI;
	            MachineInstr &DefMI = *DI;
	            if (DefMI.isDebugInstr() || isMarkerInstr(DefMI))
	              continue;
	            if (!DefMI.definesRegister(ShAmtReg, &TRI))
	              continue;
	            if (DefMI.getNumOperands() < 3 || !DefMI.getOperand(1).isReg() ||
	                DefMI.getOperand(1).getReg() != LinxISA::R0 ||
	                !DefMI.getOperand(2).isImm())
	              break;
	            switch (DefMI.getOpcode()) {
	            case LinxISA::ADDIri:
	            case LinxISA::ADDIWri:
	              return DefMI.getOperand(2).getImm();
	            default:
	              break;
	            }
	            break;
	          }
	          if (ShAmtReg.isPhysical())
	            return getPhysRegConstAtMBBEntry(ShAmtReg);
	          return std::nullopt;
	        };

	        auto ShAmtC = getConstShiftAmt(*SrlMI);
	        if (!ShAmtC || *ShAmtC != 32)
	          return false;

	        // Find defining SLLrr of Tmp1.
	        MachineInstr *SllMI = nullptr;
	        for (auto It = SrlMI->getIterator(); It != MBB.begin();) {
	          --It;
	          MachineInstr &MI = *It;
	          if (MI.isDebugInstr() || isMarkerInstr(MI))
	            continue;
	          if (!MI.definesRegister(Tmp1, &TRI))
	            continue;
	          if (MI.getOpcode() != LinxISA::SLLrr || MI.getNumOperands() < 3 ||
	              !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
	            return false;
	          if (MI.getOperand(2).getReg() != ShAmtReg)
	            return false;
	          SllMI = &MI;
	          break;
	        }
	        if (!SllMI)
	          return false;

	        const Register Src = SllMI->getOperand(1).getReg();
	        if (!Src)
	          return false;

	        if (!hasSingleNonDbgUseInMBB(Tmp1, SrlMI, SllMI))
	          return false;
	        if (!hasSingleNonDbgUseInMBB(ZextReg, &UseMI, SrlMI))
	          return false;
	        if ((Tmp1.isPhysical() && isPhysRegLiveOutOfBlock(Tmp1)) ||
	            (ZextReg.isPhysical() && isPhysRegLiveOutOfBlock(ZextReg)))
	          return false;

	        OrigSrc = Src;
	        SllMIOut = SllMI;
	        SrlMIOut = SrlMI;
	        return true;
	      };

	      // Pre-blockify peepholes (run before inserting block markers and T/U
	      // remapping).
	      //
	      // Fold `and/or` feeding a nonzero compare into CMP.AND/CMP.OR:
	      //   tmp = and/or x, y
	      //   tmp2 = addw tmp, zero        (optional)
	      //   dst = cmp.nei tmp2, 0
	      // =>
	      //   dst = cmp.and/or x, y
	      //
		      // and similarly for immediate ANDI/ORI.
		      for (auto It = MBB.begin(); It != MBB.end();) {
		        MachineInstr &LogicMI = *It;
	        if (LogicMI.isDebugInstr() || isMarkerInstr(LogicMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned LogicOpc = LogicMI.getOpcode();
	        const bool IsAnd =
	            (LogicOpc == LinxISA::ANDrr || LogicOpc == LinxISA::ANDWrr ||
	             LogicOpc == LinxISA::ANDIri || LogicOpc == LinxISA::ANDIWri ||
	             LogicOpc == LinxISA::HLANDIri || LogicOpc == LinxISA::HLANDIWri);
	        const bool IsOr =
	            (LogicOpc == LinxISA::ORrr || LogicOpc == LinxISA::ORWrr ||
	             LogicOpc == LinxISA::ORIri || LogicOpc == LinxISA::ORIWri ||
	             LogicOpc == LinxISA::HLORIri || LogicOpc == LinxISA::HLORIWri);
	        if (!IsAnd && !IsOr) {
	          ++It;
	          continue;
	        }
	        if (LogicMI.getNumOperands() < 3 || !LogicMI.getOperand(0).isReg() ||
	            !LogicMI.getOperand(0).isDef()) {
	          ++It;
	          continue;
	        }

	        const Register Tmp = LogicMI.getOperand(0).getReg();
	        if (!Tmp || !Tmp.isPhysical()) {
	          ++It;
	          continue;
	        }

	        auto nextNonMarker = [&](MachineBasicBlock::iterator Pos)
	            -> MachineBasicBlock::iterator {
	          auto NI = Pos;
	          while (NI != MBB.end() && (NI->isDebugInstr() || isMarkerInstr(*NI)))
	            ++NI;
	          return NI;
	        };

	        MachineInstr *CopyMI = nullptr;
	        Register CmpSrc = Tmp;
	        bool InPlaceCopy = false;

	        auto NI = nextNonMarker(std::next(It));
	        if (NI == MBB.end()) {
	          ++It;
	          continue;
	        }

	        if (NI->getOpcode() == LinxISA::ADDWrr && NI->getNumOperands() >= 3 &&
	            NI->getOperand(0).isReg() && NI->getOperand(0).isDef() &&
	            NI->getOperand(1).isReg() && NI->getOperand(2).isReg()) {
	          const Register CDst = NI->getOperand(0).getReg();
	          const Register A = NI->getOperand(1).getReg();
	          const Register B = NI->getOperand(2).getReg();
	          if (CDst && CDst.isPhysical() &&
	              ((A == Tmp && B == LinxISA::R0) ||
	               (B == Tmp && A == LinxISA::R0))) {
	            CopyMI = &*NI;
	            InPlaceCopy = (CDst == Tmp);
	            if (!InPlaceCopy)
	              CmpSrc = CDst;
	            NI = nextNonMarker(std::next(NI));
	            if (NI == MBB.end()) {
	              ++It;
	              continue;
	            }
	          }
	        }

	        MachineInstr &CmpMI = *NI;
	        if (CmpMI.getOpcode() != LinxISA::CMPNEI || CmpMI.getNumOperands() < 3 ||
	            !CmpMI.getOperand(1).isReg() || CmpMI.getOperand(1).getReg() != CmpSrc ||
	            !CmpMI.getOperand(2).isImm() || CmpMI.getOperand(2).getImm() != 0) {
	          ++It;
	          continue;
	        }

	        auto onlyUsedBy = [&](Register Reg, const MachineInstr *MI1,
	                              const MachineInstr *MI2,
	                              const MachineInstr *IgnoreMI) -> bool {
	          for (const MachineInstr &MI : MBB) {
	            if (MI.isDebugInstr() || isMarkerInstr(MI))
	              continue;
	            if (&MI == IgnoreMI)
	              continue;
	            for (const MachineOperand &MO : MI.operands()) {
	              if (!MO.isReg() || MO.isImplicit() || MO.isDef())
	                continue;
	              if (MO.getReg() != Reg)
	                continue;
	              if (&MI != MI1 && &MI != MI2)
	                return false;
	            }
	          }
	          return true;
	        };

	        if (CopyMI && InPlaceCopy) {
	          // tmp is used by both the in-place ADDW and the compare.
	          if (!onlyUsedBy(Tmp, CopyMI, &CmpMI, &LogicMI)) {
	            ++It;
	            continue;
	          }
	        } else {
	          if (!hasSingleNonDbgUseInMBB(CmpSrc, &CmpMI,
	                                       CopyMI ? CopyMI : &LogicMI)) {
	            ++It;
	            continue;
	          }
	          if (!hasSingleNonDbgUseInMBB(Tmp, CopyMI ? CopyMI : &CmpMI, &LogicMI)) {
	            ++It;
	            continue;
	          }
	        }

	        const Register Dst = CmpMI.getOperand(0).getReg();
	        unsigned NewOpc = 0;
	        if (LogicOpc == LinxISA::ANDrr || LogicOpc == LinxISA::ANDWrr)
	          NewOpc = LinxISA::CMPAND;
	        else if (LogicOpc == LinxISA::ORrr || LogicOpc == LinxISA::ORWrr)
	          NewOpc = LinxISA::CMPOR;
	        else if (LogicOpc == LinxISA::ANDIri || LogicOpc == LinxISA::ANDIWri)
	          NewOpc = LinxISA::CMPANDI;
	        else if (LogicOpc == LinxISA::ORIri || LogicOpc == LinxISA::ORIWri)
	          NewOpc = LinxISA::CMPORI;
	        else if (LogicOpc == LinxISA::HLANDIri || LogicOpc == LinxISA::HLANDIWri)
	          NewOpc = LinxISA::HLCMPANDI;
	        else if (LogicOpc == LinxISA::HLORIri || LogicOpc == LinxISA::HLORIWri)
	          NewOpc = LinxISA::HLCMPORI;
	        else {
	          ++It;
	          continue;
	        }

	        if (NewOpc == LinxISA::CMPAND || NewOpc == LinxISA::CMPOR) {
	          BuildMI(MBB, CmpMI.getIterator(), CmpMI.getDebugLoc(), TII.get(NewOpc), Dst)
	              .addReg(LogicMI.getOperand(1).getReg())
	              .addReg(LogicMI.getOperand(2).getReg());
	        } else {
	          if (!LogicMI.getOperand(2).isImm()) {
	            ++It;
	            continue;
	          }
	          BuildMI(MBB, CmpMI.getIterator(), CmpMI.getDebugLoc(), TII.get(NewOpc), Dst)
	              .addReg(LogicMI.getOperand(1).getReg())
	              .addImm(LogicMI.getOperand(2).getImm());
	        }

	        CmpMI.eraseFromParent();
	        if (CopyMI)
	          CopyMI->eraseFromParent();
	        LogicMI.eraseFromParent();
		        Changed = true;
		        It = MBB.begin();
		      }

		      // Pre-blockify peephole: fold a local `sext.w` legalization feeding a
		      // compressed commit condition into a single 32-bit SETC with a SrcR
		      // modifier. This reduces dynamic instruction count while keeping code
		      // size roughly flat (16b+16b -> 32b).
		      //
		      // Pattern (common in Linux):
		      //   tmp = addw src, zero    ; sext.w(src)
		      //   c.setc.{eq,ne} tmp, zero
		      // =>
		      //   setc.{eq,ne} zero, src.sw
		      for (auto It = MBB.begin(); It != MBB.end();) {
		        MachineInstr &SetcMI = *It;
		        if (SetcMI.isDebugInstr() || isMarkerInstr(SetcMI)) {
		          ++It;
		          continue;
		        }
		        const unsigned Opc = SetcMI.getOpcode();
			        if ((Opc != LinxISA::CSETC_EQ && Opc != LinxISA::CSETC_NE) ||
			            SetcMI.getNumOperands() < 2 || !SetcMI.getOperand(0).isReg() ||
			            !SetcMI.getOperand(1).isReg()) {
			          ++It;
			          continue;
			        }
			        if (!linxEnableSetcSrcRTypeFlags()) {
			          ++It;
			          continue;
			        }

		        const Register A = SetcMI.getOperand(0).getReg();
		        const Register B = SetcMI.getOperand(1).getReg();
		        const bool AIsZero = (A == LinxISA::R0);
		        const bool BIsZero = (B == LinxISA::R0);
		        if (AIsZero == BIsZero) { // require exactly one side is zero
		          ++It;
		          continue;
		        }

		        const Register SextReg = AIsZero ? B : A;

		        auto matchSextWByAddwZero =
		            [&](Register Reg, Register &OrigSrc,
		                MachineInstr *&DefMIOut) -> bool {
		          OrigSrc = Register();
		          DefMIOut = nullptr;
		          if (!Reg)
		            return false;

		          // Find defining ADDWrr in this block.
		          MachineInstr *DefMI = nullptr;
		          for (auto DI = SetcMI.getIterator(); DI != MBB.begin();) {
		            --DI;
		            MachineInstr &MI = *DI;
		            if (MI.isDebugInstr() || isMarkerInstr(MI))
		              continue;
		            if (!MI.definesRegister(Reg, &TRI))
		              continue;
		            DefMI = &MI;
		            break;
		          }
		          if (!DefMI)
		            return false;
		          if (DefMI->getOpcode() != LinxISA::ADDWrr ||
		              DefMI->getNumOperands() < 3 || !DefMI->getOperand(1).isReg() ||
		              !DefMI->getOperand(2).isReg())
		            return false;

		          const Register X = DefMI->getOperand(1).getReg();
		          const Register Y = DefMI->getOperand(2).getReg();
		          if (X == LinxISA::R0 && Y != LinxISA::R0)
		            OrigSrc = Y;
		          else if (Y == LinxISA::R0 && X != LinxISA::R0)
		            OrigSrc = X;
		          else
		            return false;

		          if (!hasSingleNonDbgUseInMBB(Reg, &SetcMI, DefMI))
		            return false;
		          if (Reg.isPhysical() && isPhysRegLiveOutOfBlock(Reg))
		            return false;

		          DefMIOut = DefMI;
		          return true;
		        };

		        Register OrigSrc = Register();
		        MachineInstr *AddwMI = nullptr;
		        if (!matchSextWByAddwZero(SextReg, OrigSrc, AddwMI)) {
		          ++It;
		          continue;
		        }

		        const unsigned NewSetcOpc =
		            (Opc == LinxISA::CSETC_EQ) ? LinxISA::SETC_EQ : LinxISA::SETC_NE;

		        MachineInstr *NewMI =
		            BuildMI(MBB, It, SetcMI.getDebugLoc(), TII.get(NewSetcOpc))
		                .addReg(LinxISA::R0)
		                .addReg(OrigSrc)
		                .getInstr();
		        (void)NewMI;

		        auto NextIt = std::next(It);
		        SetcMI.eraseFromParent();
		        AddwMI->eraseFromParent();
		        Changed = true;
		        It = NextIt;
		      }

		      ExitKind Kind = ExitKind::Fall;
		      MachineBasicBlock *TargetBB = nullptr;   // DIRECT/COND
		      MachineBasicBlock *ReturnBB = nullptr;   // CALL (return target)
	      std::optional<MachineOperand> CallTargetOp; // CALL (callee)
		      std::optional<Register> HeaderSetcTgtReg;   // inserted immediately after BSTART
		      std::optional<Register> ICallSetcTgtReg;    // inserted after callee is computed (ICALL)

      // Identify the last two non-debug, non-marker instructions.
      MachineInstr *Last = nullptr;
      MachineInstr *Prev = nullptr;
      for (auto It = MBB.rbegin(), E = MBB.rend(); It != E; ++It) {
        if (It->isDebugInstr() || isMarkerInstr(*It))
          continue;
        if (!Last) {
          Last = &*It;
          continue;
        }
        Prev = &*It;
        break;
      }

	      // Recognize exit shape from the end of the block.
	      if (Last) {
	        switch (Last->getOpcode()) {
	        case LinxISA::PSEUDO_TAILCALL: {
	          CallTargetOp = Last->getOperand(0);
	          if (CallTargetOp->isReg()) {
	            Kind = ExitKind::Ind;
	            ICallSetcTgtReg = CallTargetOp->getReg();
	          } else {
	            Kind = ExitKind::Direct;
	          }
	          Last->eraseFromParent();
	          Changed = true;
	          break;
	        }
        case LinxISA::PSEUDO_TAILICALL: {
          CallTargetOp = Last->getOperand(0);
          Kind = ExitKind::Ind;
          ICallSetcTgtReg = CallTargetOp->getReg();
          Last->eraseFromParent();
          Changed = true;
          break;
        }
		        case LinxISA::PSEUDO_CALL: {
		          CallTargetOp = Last->getOperand(0);
		          if (CallTargetOp->isReg()) {
		            Kind = ExitKind::ICall;
		            ICallSetcTgtReg = CallTargetOp->getReg();
	          } else {
	            Kind = ExitKind::Call;
	          }
          /*
           * CALL/ICALL blocks must always carry an adjacent SETRET target under
           * the strict call/ret contract. For no-successor (noreturn) blocks,
           * prefer the physical next block; if that is unavailable (or points at
           * the internal empty-body stub), fall back to this block's own label.
           * A true noreturn callee should never consume the return target, but a
           * concrete marker keeps the header encoding and emulator checks valid.
          */
          if (!MBB.succ_empty())
            ReturnBB = *MBB.succ_begin();
          if (ReturnBB && DecoupledBodyBBs.contains(ReturnBB))
            ReturnBB = nullptr;
          if (!ReturnBB)
            ReturnBB = &MBB;
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::PSEUDO_ICALL: {
          CallTargetOp = Last->getOperand(0);
          Kind = ExitKind::ICall;
          ICallSetcTgtReg = CallTargetOp->getReg();
          if (!MBB.succ_empty())
            ReturnBB = *MBB.succ_begin();
          if (ReturnBB && DecoupledBodyBBs.contains(ReturnBB))
            ReturnBB = nullptr;
          if (!ReturnBB)
            ReturnBB = &MBB;
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::PSEUDO_RET: {
          Kind = ExitKind::Ret;
          // Return target is always `ra`; place SETC.TGT right after the BSTART
          // marker for readability.
          HeaderSetcTgtReg = LinxISA::R10;
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::JR: {
          const Register Reg = Last->getOperand(0).getReg();
          Kind = (Reg == LinxISA::R10) ? ExitKind::Ret : ExitKind::Ind;
          if (Reg == LinxISA::R10) {
            HeaderSetcTgtReg = Reg;
          } else {
            BuildMI(MBB, Last->getIterator(), DebugLoc(),
                    TII.get(LinxISA::CSETC_TGT))
                .addReg(Reg);
          }
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::JUMP: {
          // Common lowering shape: `Bcc ...; JUMP ...` (no fallthrough).
          // In BlockISA we must pick a fallthrough block (the physically next
          // block) and encode the other successor in the BSTART header.
          if (Prev && (Prev->getOpcode() == LinxISA::BEQ ||
                       Prev->getOpcode() == LinxISA::BNE ||
                       Prev->getOpcode() == LinxISA::BLT ||
                       Prev->getOpcode() == LinxISA::BGE ||
                       Prev->getOpcode() == LinxISA::BLTU ||
                       Prev->getOpcode() == LinxISA::BGEU)) {
            MachineBasicBlock *BrTargetBB = Prev->getOperand(2).getMBB();
            MachineBasicBlock *JumpTargetBB = Last->getOperand(0).getMBB();
            MachineBasicBlock *FallthroughBB = MBB.getNextNode();
            auto makeTrampoline = [&](MachineBasicBlock *Target) -> MachineBasicBlock * {
              MachineFunction &MF = *MBB.getParent();
              auto *TrampBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
              MF.insert(std::next(MBB.getIterator()), TrampBB);
              TrampBB->addSuccessor(Target);
              BuildMI(*TrampBB, TrampBB->end(), DebugLoc(), TII.get(LinxISA::JUMP))
                  .addMBB(Target);
              return TrampBB;
            };

            unsigned SetcOpc = 0;
            Register LHSReg = Prev->getOperand(0).getReg();
            Register RHSReg = Prev->getOperand(1).getReg();
            auto pickSetc = [&](unsigned BrOpc) -> unsigned {
              switch (BrOpc) {
              case LinxISA::BEQ:
                return LinxISA::CSETC_EQ;
              case LinxISA::BNE:
                return LinxISA::CSETC_NE;
              case LinxISA::BLT:
                return LinxISA::SETC_LT;
              case LinxISA::BGE:
                return LinxISA::SETC_GE;
              case LinxISA::BLTU:
                return LinxISA::SETC_LTU;
              case LinxISA::BGEU:
                return LinxISA::SETC_GEU;
              default:
                llvm_unreachable("Unexpected branch opcode");
              }
            };
	            auto invertBranch = [&](unsigned BrOpc) -> unsigned {
	              switch (BrOpc) {
              case LinxISA::BEQ:
                return LinxISA::BNE;
              case LinxISA::BNE:
                return LinxISA::BEQ;
              case LinxISA::BLT:
                return LinxISA::BGE;
              case LinxISA::BGE:
                return LinxISA::BLT;
              case LinxISA::BLTU:
                return LinxISA::BGEU;
              case LinxISA::BGEU:
                return LinxISA::BLTU;
              default:
                llvm_unreachable("Unexpected branch opcode");
	              }
	            };

	            auto pickSetcImm = [&](unsigned BrOpc) -> unsigned {
	              switch (BrOpc) {
	              case LinxISA::BEQ:
	                return LinxISA::SETC_EQI;
	              case LinxISA::BNE:
	                return LinxISA::SETC_NEI;
	              case LinxISA::BLT:
	                return LinxISA::SETC_LTI;
	              case LinxISA::BGE:
	                return LinxISA::SETC_GEI;
	              case LinxISA::BLTU:
	                return LinxISA::SETC_LTUI;
	              case LinxISA::BGEU:
	                return LinxISA::SETC_GEUI;
	              default:
	                llvm_unreachable("Unexpected branch opcode");
	              }
	            };

	            auto getSingleUseImmFromZero = [&](MachineInstr &UseMI, Register Reg,
	                                               MachineInstr *&DefMIOut)
	                -> std::optional<int64_t> {
	              DefMIOut = nullptr;
	              if (!Reg || !Reg.isPhysical())
	                return std::nullopt;

	              for (auto It = UseMI.getIterator(); It != MBB.begin();) {
	                --It;
	                MachineInstr &MI = *It;
	                if (MI.isDebugInstr() || isMarkerInstr(MI))
	                  continue;
	                if (!MI.definesRegister(Reg, &TRI))
	                  continue;
	                DefMIOut = &MI;
	                break;
	              }
	              if (!DefMIOut)
	                return std::nullopt;

	              MachineInstr &DefMI = *DefMIOut;
	              auto isFromZero = [&](unsigned BaseOpNo) -> bool {
	                if (BaseOpNo >= DefMI.getNumOperands())
	                  return false;
	                const MachineOperand &MO = DefMI.getOperand(BaseOpNo);
	                return MO.isReg() && MO.getReg() == LinxISA::R0;
	              };

	              int64_t Val = 0;
	              switch (DefMI.getOpcode()) {
	              case LinxISA::ADDIri:
	              case LinxISA::ADDIWri:
	                if (!isFromZero(/*BaseOpNo=*/1) || DefMI.getNumOperands() < 3 ||
	                    !DefMI.getOperand(2).isImm())
	                  return std::nullopt;
	                Val = DefMI.getOperand(2).getImm();
	                break;
	              case LinxISA::SUBIri:
	              case LinxISA::SUBIWri:
	                if (!isFromZero(/*BaseOpNo=*/1) || DefMI.getNumOperands() < 3 ||
	                    !DefMI.getOperand(2).isImm())
	                  return std::nullopt;
	                Val = -DefMI.getOperand(2).getImm();
	                break;
	              case LinxISA::LUI:
	                if (DefMI.getNumOperands() < 2 || !DefMI.getOperand(1).isImm())
	                  return std::nullopt;
	                Val = DefMI.getOperand(1).getImm() << 12;
	                break;
		              default:
		                return std::nullopt;
		              }

		              if (!hasSingleNonDbgUseInMBB(Reg, &UseMI, &DefMI))
		                return std::nullopt;
		              if (isPhysRegLiveOutOfBlock(Reg))
		                return std::nullopt;
		              return Val;
		            };

            // Prefer using the already-laid-out next block as fallthrough.
            unsigned BrOpcForSetc = Prev->getOpcode();
            MachineBasicBlock *CondFallthroughBB = nullptr;
            if (FallthroughBB == JumpTargetBB) {
              Kind = ExitKind::Cond;
              TargetBB = BrTargetBB;
              CondFallthroughBB = JumpTargetBB;
              SetcOpc = pickSetc(BrOpcForSetc);
            } else if (FallthroughBB == BrTargetBB) {
              Kind = ExitKind::Cond;
              TargetBB = JumpTargetBB;
              CondFallthroughBB = BrTargetBB;
              BrOpcForSetc = invertBranch(BrOpcForSetc);
              SetcOpc = pickSetc(BrOpcForSetc);
            } else {
              // Neither successor is laid out as fallthrough. Insert a small
              // trampoline block so we can keep BlockISA's "conditional +
              // implicit fallthrough" shape without requiring global block
              // reordering.
              //
              //   Bcc BrTarget; JUMP JumpTarget
              // becomes:
              //   (block header encodes conditional jump to BrTarget)
              //   fallthrough -> tramp
              //   tramp: JUMP JumpTarget
              MachineBasicBlock *TrampBB = makeTrampoline(JumpTargetBB);

              // Fix up the Machine-CFG: JumpTarget is no longer reached
              // directly from MBB. Update PHIs and edge lists.
              if (MBB.isSuccessor(JumpTargetBB)) {
                JumpTargetBB->replacePhiUsesWith(&MBB, TrampBB);
                MBB.removeSuccessor(JumpTargetBB);
              }
              if (!MBB.isSuccessor(TrampBB))
                MBB.addSuccessor(TrampBB);

              Kind = ExitKind::Cond;
              TargetBB = BrTargetBB;
              CondFallthroughBB = TrampBB;
              SetcOpc = pickSetc(BrOpcForSetc);
            }

	            bool EmittedImmSetc = false;
	            auto tryEmitSetcImm = [&](unsigned BrOpc, Register SrcReg,
	                                      int64_t ImmVal, MachineInstr *DefMI) -> bool {
	              unsigned ImmOpc = pickSetcImm(BrOpc);
	              const bool UnsignedImm =
	                  (ImmOpc == LinxISA::SETC_LTUI) || (ImmOpc == LinxISA::SETC_GEUI);
	              if (UnsignedImm) {
	                if (!canEncodeShiftedUnsignedImm(ImmVal, /*BaseBits=*/12))
	                  return false;
	              } else {
	                if (!canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
	                  return false;
	              }

	              auto SetcIt = findSetcInsertPt(MBB, *Prev, SrcReg, Register());
	              BuildMI(MBB, SetcIt, DebugLoc(), TII.get(ImmOpc))
	                  .addReg(SrcReg)
	                  .addImm(ImmVal);
	              if (DefMI)
	                DefMI->eraseFromParent();
	              return true;
	            };

	            MachineInstr *RHSDefMI = nullptr;
	            if (auto Imm = getSingleUseImmFromZero(*Prev, RHSReg, RHSDefMI)) {
	              EmittedImmSetc =
	                  tryEmitSetcImm(BrOpcForSetc, /*SrcReg=*/LHSReg, *Imm, RHSDefMI);
	            } else {
	              MachineInstr *LHSDefMI = nullptr;
	              if (auto Imm = getSingleUseImmFromZero(*Prev, LHSReg, LHSDefMI)) {
	                // If the constant is on the LHS, rewrite to keep the variable
	                // operand as SrcL for the immediate SETC forms.
	                const int64_t C = *Imm;
	                switch (BrOpcForSetc) {
	                case LinxISA::BEQ:
	                case LinxISA::BNE:
	                  EmittedImmSetc = tryEmitSetcImm(BrOpcForSetc, /*SrcReg=*/RHSReg,
	                                                  C, LHSDefMI);
	                  break;
	                case LinxISA::BLT:
	                  if (C != std::numeric_limits<int64_t>::max())
	                    EmittedImmSetc =
	                        tryEmitSetcImm(LinxISA::BGE, /*SrcReg=*/RHSReg, C + 1,
	                                       LHSDefMI);
	                  break;
	                case LinxISA::BGE:
	                  if (C != std::numeric_limits<int64_t>::max())
	                    EmittedImmSetc =
	                        tryEmitSetcImm(LinxISA::BLT, /*SrcReg=*/RHSReg, C + 1,
	                                       LHSDefMI);
	                  break;
	                case LinxISA::BLTU: {
	                  const uint64_t CU = static_cast<uint64_t>(C);
	                  if (CU != std::numeric_limits<uint64_t>::max())
	                    EmittedImmSetc = tryEmitSetcImm(LinxISA::BGEU,
	                                                    /*SrcReg=*/RHSReg,
	                                                    static_cast<int64_t>(CU + 1),
	                                                    LHSDefMI);
	                  break;
	                }
	                case LinxISA::BGEU: {
	                  const uint64_t CU = static_cast<uint64_t>(C);
	                  if (CU != std::numeric_limits<uint64_t>::max())
	                    EmittedImmSetc = tryEmitSetcImm(LinxISA::BLTU,
	                                                    /*SrcReg=*/RHSReg,
	                                                    static_cast<int64_t>(CU + 1),
	                                                    LHSDefMI);
	                  break;
	                }
	                default:
	                  break;
	                }
	              }
		            }

			            if (!EmittedImmSetc) {
				              auto tryEmitZextWSetcUW = [&]() -> bool {
				                if (!linxEnableSetcSrcRTypeFlags())
				                  return false;
				                if (BrOpcForSetc != LinxISA::BEQ && BrOpcForSetc != LinxISA::BNE)
				                  return false;
			                if ((LHSReg == LinxISA::R0) == (RHSReg == LinxISA::R0))
			                  return false;

			                const Register ZextReg =
			                    (LHSReg == LinxISA::R0) ? RHSReg : LHSReg;
			                Register OrigSrc;
			                MachineInstr *SllMI = nullptr;
			                MachineInstr *SrlMI = nullptr;
			                if (!matchZextWByShiftPair(*Prev, ZextReg, OrigSrc, SllMI, SrlMI))
			                  return false;

			                const unsigned NewSetcOpc =
			                    (BrOpcForSetc == LinxISA::BEQ) ? LinxISA::SETC_EQ
			                                                  : LinxISA::SETC_NE;
			                auto SetcIt = findSetcInsertPt(MBB, *Prev, LinxISA::R0, OrigSrc);
		                MachineInstr *NewMI =
		                    BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                        .addReg(LinxISA::R0)
		                        .addReg(OrigSrc)
		                        .getInstr();
		                (void)NewMI;

			                SrlMI->eraseFromParent();
			                SllMI->eraseFromParent();
			                return true;
			              };

			              if (tryEmitZextWSetcUW()) {
			                EmittedImmSetc = true;
			              }

			              // Peephole: `and/or` feeding a branch against zero:
			              //   tmp = and/or x, y
			              //   {bne,beq} tmp, zero, label
			              // =>
			              //   setc.and/or x, y
			              //
			              // and similarly for immediate ANDI/ORI:
			              //   tmp = andi/ori x, imm
			              //   {bne,beq} tmp, zero, label
			              // =>
			              //   setc.andi/ori x, imm
		              auto tryEmitLogicSetcMask = [&]() -> bool {
		                if (!linxEnableMaskSetcFold())
		                  return false;
		                if (BrOpcForSetc != LinxISA::BNE && BrOpcForSetc != LinxISA::BEQ)
		                  return false;

		                Register ValSide = Register();
		                MachineInstr *ZeroDefMI = nullptr;
		                if (LHSReg == LinxISA::R0 && RHSReg != LinxISA::R0)
		                  ValSide = RHSReg;
		                else if (RHSReg == LinxISA::R0 && LHSReg != LinxISA::R0)
		                  ValSide = LHSReg;
		                else {
		                  auto isZeroFromR0 = [&](Register Reg,
		                                          MachineInstr *&DefMIOut) -> bool {
		                    if (!Reg)
		                      return false;
		                    if (auto Imm =
		                            getSingleUseImmFromZero(*Prev, Reg, DefMIOut))
		                      return *Imm == 0;
		                    return false;
		                  };

		                  MachineInstr *LZeroDefMI = nullptr;
		                  MachineInstr *RZeroDefMI = nullptr;
		                  const bool LZero = isZeroFromR0(LHSReg, LZeroDefMI);
		                  const bool RZero = isZeroFromR0(RHSReg, RZeroDefMI);
		                  if (LZero == RZero)
		                    return false;
		                  if (LZero) {
		                    ValSide = RHSReg;
		                    ZeroDefMI = LZeroDefMI;
		                  } else {
		                    ValSide = LHSReg;
		                    ZeroDefMI = RZeroDefMI;
		                  }
		                }

		                const bool NeedsInvert = (BrOpcForSetc == LinxISA::BEQ);
		                if (NeedsInvert && !CondFallthroughBB)
		                  return false;

		                // Find defining instruction of ValSide (nearest preceding def).
		                MachineInstr *DefMI = nullptr;
		                for (auto It = Prev->getIterator(); It != MBB.begin();) {
		                  --It;
		                  MachineInstr &MI = *It;
		                  if (MI.isDebugInstr() || isMarkerInstr(MI))
		                    continue;
		                  if (MI.definesRegister(ValSide, &TRI)) {
		                    DefMI = &MI;
		                    break;
		                  }
		                }
		                if (!DefMI)
		                  return false;
		                if (isPhysRegLiveOutOfBlock(ValSide))
		                  return false;

		                unsigned NewSetcOpc = 0;
		                Register SrcA = Register(), SrcB = Register();
		                int64_t ImmVal = 0;
		                bool IsImm = false;

		                switch (DefMI->getOpcode()) {
		                case LinxISA::ANDrr:
		                case LinxISA::ANDWrr:
		                  if (DefMI->getNumOperands() < 3)
		                    return false;
		                  NewSetcOpc = LinxISA::SETC_AND;
		                  SrcA = DefMI->getOperand(1).getReg();
		                  SrcB = DefMI->getOperand(2).getReg();
		                  break;
		                case LinxISA::ORrr:
		                case LinxISA::ORWrr:
		                  if (DefMI->getNumOperands() < 3)
		                    return false;
		                  NewSetcOpc = LinxISA::SETC_OR;
		                  SrcA = DefMI->getOperand(1).getReg();
		                  SrcB = DefMI->getOperand(2).getReg();
		                  break;
		                case LinxISA::ANDIri:
		                case LinxISA::ANDIWri:
		                case LinxISA::HLANDIri:
		                case LinxISA::HLANDIWri:
		                  if (DefMI->getNumOperands() < 3 || !DefMI->getOperand(2).isImm())
		                    return false;
		                  SrcA = DefMI->getOperand(1).getReg();
		                  ImmVal = DefMI->getOperand(2).getImm();
		                  if (canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
		                    NewSetcOpc = LinxISA::SETC_ANDI;
		                  else
		                    NewSetcOpc = LinxISA::HLSETC_ANDI;
		                  IsImm = true;
		                  break;
		                case LinxISA::ORIri:
		                case LinxISA::ORIWri:
		                case LinxISA::HLORIri:
		                case LinxISA::HLORIWri:
		                  if (DefMI->getNumOperands() < 3 || !DefMI->getOperand(2).isImm())
		                    return false;
		                  SrcA = DefMI->getOperand(1).getReg();
		                  ImmVal = DefMI->getOperand(2).getImm();
		                  if (canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
		                    NewSetcOpc = LinxISA::SETC_ORI;
		                  else
		                    NewSetcOpc = LinxISA::HLSETC_ORI;
		                  IsImm = true;
		                  break;
		                default:
		                  return false;
		                }

		                if (!hasSingleNonDbgUseInMBB(ValSide, Prev, DefMI))
		                  return false;

		                if (NeedsInvert) {
		                  BrOpcForSetc = LinxISA::BNE;
		                  SetcOpc = pickSetc(BrOpcForSetc);
		                  TargetBB = CondFallthroughBB;
		                }

		                auto SetcIt = findSetcInsertPt(MBB, *Prev, SrcA, IsImm ? Register() : SrcB);
		                if (IsImm) {
		                  BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                      .addReg(SrcA)
		                      .addImm(ImmVal);
		                } else {
		                  BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                      .addReg(SrcA)
		                      .addReg(SrcB);
		                }
		                if (ZeroDefMI)
		                  ZeroDefMI->eraseFromParent();
		                DefMI->eraseFromParent();
		                EmittedImmSetc = true;
		                return true;
		              };

			              if (!EmittedImmSetc && !tryEmitLogicSetcMask()) {
			                auto SetcIt = findSetcInsertPt(MBB, *Prev, LHSReg, RHSReg);
			                BuildMI(MBB, SetcIt, DebugLoc(), TII.get(SetcOpc))
			                    .addReg(LHSReg)
			                    .addReg(RHSReg);
			              }
			            }
	            Prev->eraseFromParent();
	            Last->eraseFromParent();
	            Changed = true;
	            break;
	          }

          Kind = ExitKind::Direct;
          TargetBB = Last->getOperand(0).getMBB();
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::BEQ:
        case LinxISA::BNE:
        case LinxISA::BLT:
        case LinxISA::BGE:
        case LinxISA::BLTU:
        case LinxISA::BGEU: {
          Kind = ExitKind::Cond;
          TargetBB = Last->getOperand(2).getMBB();

          unsigned SetcOpc = 0;
          switch (Last->getOpcode()) {
          case LinxISA::BEQ:
            SetcOpc = LinxISA::CSETC_EQ;
            break;
          case LinxISA::BNE:
            SetcOpc = LinxISA::CSETC_NE;
            break;
          case LinxISA::BLT:
            SetcOpc = LinxISA::SETC_LT;
            break;
          case LinxISA::BGE:
            SetcOpc = LinxISA::SETC_GE;
            break;
          case LinxISA::BLTU:
            SetcOpc = LinxISA::SETC_LTU;
            break;
          case LinxISA::BGEU:
            SetcOpc = LinxISA::SETC_GEU;
            break;
          default:
            llvm_unreachable("Unexpected branch opcode");
          }

	          Register LHSReg = Last->getOperand(0).getReg();
	          Register RHSReg = Last->getOperand(1).getReg();
	          bool EmittedImmSetc = false;
	          auto pickSetcImm = [&](unsigned BrOpc) -> unsigned {
	            switch (BrOpc) {
	            case LinxISA::BEQ:
	              return LinxISA::SETC_EQI;
	            case LinxISA::BNE:
	              return LinxISA::SETC_NEI;
	            case LinxISA::BLT:
	              return LinxISA::SETC_LTI;
	            case LinxISA::BGE:
	              return LinxISA::SETC_GEI;
	            case LinxISA::BLTU:
	              return LinxISA::SETC_LTUI;
	            case LinxISA::BGEU:
	              return LinxISA::SETC_GEUI;
	            default:
	              llvm_unreachable("Unexpected branch opcode");
	            }
	          };

	          auto getSingleUseImmFromZero = [&](MachineInstr &UseMI, Register Reg,
	                                             MachineInstr *&DefMIOut)
	              -> std::optional<int64_t> {
	            DefMIOut = nullptr;
	            if (!Reg || !Reg.isPhysical())
	              return std::nullopt;

	            for (auto It = UseMI.getIterator(); It != MBB.begin();) {
	              --It;
	              MachineInstr &MI = *It;
	              if (MI.isDebugInstr() || isMarkerInstr(MI))
	                continue;
	              if (!MI.definesRegister(Reg, &TRI))
	                continue;
	              DefMIOut = &MI;
	              break;
	            }
	            if (!DefMIOut)
	              return std::nullopt;

	            MachineInstr &DefMI = *DefMIOut;
	            auto isFromZero = [&](unsigned BaseOpNo) -> bool {
	              if (BaseOpNo >= DefMI.getNumOperands())
	                return false;
	              const MachineOperand &MO = DefMI.getOperand(BaseOpNo);
	              return MO.isReg() && MO.getReg() == LinxISA::R0;
	            };

	            int64_t Val = 0;
	            switch (DefMI.getOpcode()) {
	            case LinxISA::ADDIri:
	            case LinxISA::ADDIWri:
	              if (!isFromZero(/*BaseOpNo=*/1) || DefMI.getNumOperands() < 3 ||
	                  !DefMI.getOperand(2).isImm())
	                return std::nullopt;
	              Val = DefMI.getOperand(2).getImm();
	              break;
	            case LinxISA::SUBIri:
	            case LinxISA::SUBIWri:
	              if (!isFromZero(/*BaseOpNo=*/1) || DefMI.getNumOperands() < 3 ||
	                  !DefMI.getOperand(2).isImm())
	                return std::nullopt;
	              Val = -DefMI.getOperand(2).getImm();
	              break;
	            case LinxISA::LUI:
	              if (DefMI.getNumOperands() < 2 || !DefMI.getOperand(1).isImm())
	                return std::nullopt;
	              Val = DefMI.getOperand(1).getImm() << 12;
	              break;
		            default:
		              return std::nullopt;
		            }

		            if (!hasSingleNonDbgUseInMBB(Reg, &UseMI, &DefMI))
		              return std::nullopt;
		            if (isPhysRegLiveOutOfBlock(Reg))
		              return std::nullopt;
		            return Val;
		          };

	          auto tryEmitSetcImm = [&](unsigned BrOpc, Register SrcReg, int64_t ImmVal,
	                                    MachineInstr *DefMI) -> bool {
	            unsigned ImmOpc = pickSetcImm(BrOpc);
	            const bool UnsignedImm =
	                (ImmOpc == LinxISA::SETC_LTUI) || (ImmOpc == LinxISA::SETC_GEUI);
	            if (UnsignedImm) {
	              if (!canEncodeShiftedUnsignedImm(ImmVal, /*BaseBits=*/12))
	                return false;
	            } else {
	              if (!canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
	                return false;
	            }

	            auto SetcIt = findSetcInsertPt(MBB, *Last, SrcReg, Register());
	            BuildMI(MBB, SetcIt, DebugLoc(), TII.get(ImmOpc))
	                .addReg(SrcReg)
	                .addImm(ImmVal);
	            if (DefMI)
	              DefMI->eraseFromParent();
	            return true;
	          };

	          MachineInstr *RHSDefMI = nullptr;
	          if (auto Imm = getSingleUseImmFromZero(*Last, RHSReg, RHSDefMI)) {
	            EmittedImmSetc =
	                tryEmitSetcImm(Last->getOpcode(), /*SrcReg=*/LHSReg, *Imm, RHSDefMI);
	          } else {
	            MachineInstr *LHSDefMI = nullptr;
	            if (auto Imm = getSingleUseImmFromZero(*Last, LHSReg, LHSDefMI)) {
	              const int64_t C = *Imm;
	              switch (Last->getOpcode()) {
	              case LinxISA::BEQ:
	              case LinxISA::BNE:
	                EmittedImmSetc =
	                    tryEmitSetcImm(Last->getOpcode(), /*SrcReg=*/RHSReg, C, LHSDefMI);
	                break;
	              case LinxISA::BLT:
	                if (C != std::numeric_limits<int64_t>::max())
	                  EmittedImmSetc = tryEmitSetcImm(
	                      LinxISA::BGE, /*SrcReg=*/RHSReg, C + 1, LHSDefMI);
	                break;
	              case LinxISA::BGE:
	                if (C != std::numeric_limits<int64_t>::max())
	                  EmittedImmSetc = tryEmitSetcImm(
	                      LinxISA::BLT, /*SrcReg=*/RHSReg, C + 1, LHSDefMI);
	                break;
	              case LinxISA::BLTU: {
	                const uint64_t CU = static_cast<uint64_t>(C);
	                if (CU != std::numeric_limits<uint64_t>::max())
	                  EmittedImmSetc = tryEmitSetcImm(
	                      LinxISA::BGEU, /*SrcReg=*/RHSReg,
	                      static_cast<int64_t>(CU + 1), LHSDefMI);
	                break;
	              }
	              case LinxISA::BGEU: {
	                const uint64_t CU = static_cast<uint64_t>(C);
	                if (CU != std::numeric_limits<uint64_t>::max())
	                  EmittedImmSetc = tryEmitSetcImm(
	                      LinxISA::BLTU, /*SrcReg=*/RHSReg,
	                      static_cast<int64_t>(CU + 1), LHSDefMI);
	                break;
	              }
	              default:
	                break;
	              }
	            }
	          }

			          if (!EmittedImmSetc) {
				            auto tryEmitZextWSetcUW = [&]() -> bool {
				              if (!linxEnableSetcSrcRTypeFlags())
				                return false;
				              if (Last->getOpcode() != LinxISA::BEQ &&
				                  Last->getOpcode() != LinxISA::BNE)
				                return false;
			              if ((LHSReg == LinxISA::R0) == (RHSReg == LinxISA::R0))
			                return false;

			              const Register ZextReg =
			                  (LHSReg == LinxISA::R0) ? RHSReg : LHSReg;
			              Register OrigSrc;
			              MachineInstr *SllMI = nullptr;
			              MachineInstr *SrlMI = nullptr;
			              if (!matchZextWByShiftPair(*Last, ZextReg, OrigSrc, SllMI, SrlMI))
			                return false;

			              const unsigned NewSetcOpc =
			                  (Last->getOpcode() == LinxISA::BEQ) ? LinxISA::SETC_EQ
			                                                     : LinxISA::SETC_NE;
			              auto SetcIt = findSetcInsertPt(MBB, *Last, LinxISA::R0, OrigSrc);
				              MachineInstr *NewMI =
				                  BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
				                      .addReg(LinxISA::R0)
				                      .addReg(OrigSrc)
				                      .getInstr();
				              (void)NewMI;

			              SrlMI->eraseFromParent();
			              SllMI->eraseFromParent();
			              return true;
			            };

			            if (tryEmitZextWSetcUW())
			              EmittedImmSetc = true;

			            auto tryEmitLogicSetcMask = [&]() -> bool {
		              if (!linxEnableMaskSetcFold())
			                return false;
		              if (Last->getOpcode() != LinxISA::BNE)
			                return false;

		              Register ValSide = Register();
		              MachineInstr *ZeroDefMI = nullptr;
		              if (LHSReg == LinxISA::R0 && RHSReg != LinxISA::R0)
		                ValSide = RHSReg;
		              else if (RHSReg == LinxISA::R0 && LHSReg != LinxISA::R0)
		                ValSide = LHSReg;
		              else {
		                auto isZeroFromR0 = [&](Register Reg,
		                                        MachineInstr *&DefMIOut) -> bool {
		                  if (!Reg)
		                    return false;
		                  if (auto Imm =
		                          getSingleUseImmFromZero(*Last, Reg, DefMIOut))
		                    return *Imm == 0;
		                  return false;
		                };
		                MachineInstr *LZeroDefMI = nullptr;
		                MachineInstr *RZeroDefMI = nullptr;
		                const bool LZero = isZeroFromR0(LHSReg, LZeroDefMI);
		                const bool RZero = isZeroFromR0(RHSReg, RZeroDefMI);
		                if (LZero == RZero)
		                  return false;
		                if (LZero) {
		                  ValSide = RHSReg;
		                  ZeroDefMI = LZeroDefMI;
		                } else {
		                  ValSide = LHSReg;
		                  ZeroDefMI = RZeroDefMI;
		                }
		              }

		              MachineInstr *DefMI = nullptr;
		              for (auto It = Last->getIterator(); It != MBB.begin();) {
		                --It;
		                MachineInstr &MI = *It;
		                if (MI.isDebugInstr() || isMarkerInstr(MI))
		                  continue;
		                if (MI.definesRegister(ValSide, &TRI)) {
		                  DefMI = &MI;
		                  break;
		                }
		              }
		              if (!DefMI)
		                return false;
		              if (!hasSingleNonDbgUseInMBB(ValSide, Last, DefMI))
		                return false;
		              if (isPhysRegLiveOutOfBlock(ValSide))
		                return false;

		              unsigned NewSetcOpc = 0;
		              Register SrcA = Register(), SrcB = Register();
		              int64_t ImmVal = 0;
		              bool IsImm = false;

		              switch (DefMI->getOpcode()) {
		              case LinxISA::ANDrr:
		              case LinxISA::ANDWrr:
		                NewSetcOpc = LinxISA::SETC_AND;
		                SrcA = DefMI->getOperand(1).getReg();
		                SrcB = DefMI->getOperand(2).getReg();
		                break;
		              case LinxISA::ORrr:
		              case LinxISA::ORWrr:
		                NewSetcOpc = LinxISA::SETC_OR;
		                SrcA = DefMI->getOperand(1).getReg();
		                SrcB = DefMI->getOperand(2).getReg();
		                break;
		              case LinxISA::ANDIri:
		              case LinxISA::ANDIWri:
		              case LinxISA::HLANDIri:
		              case LinxISA::HLANDIWri:
		                if (!DefMI->getOperand(2).isImm())
		                  return false;
		                SrcA = DefMI->getOperand(1).getReg();
		                ImmVal = DefMI->getOperand(2).getImm();
		                if (canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
		                  NewSetcOpc = LinxISA::SETC_ANDI;
		                else
		                  NewSetcOpc = LinxISA::HLSETC_ANDI;
		                IsImm = true;
		                break;
		              case LinxISA::ORIri:
		              case LinxISA::ORIWri:
		              case LinxISA::HLORIri:
		              case LinxISA::HLORIWri:
		                if (!DefMI->getOperand(2).isImm())
		                  return false;
		                SrcA = DefMI->getOperand(1).getReg();
		                ImmVal = DefMI->getOperand(2).getImm();
		                if (canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
		                  NewSetcOpc = LinxISA::SETC_ORI;
		                else
		                  NewSetcOpc = LinxISA::HLSETC_ORI;
		                IsImm = true;
		                break;
		              default:
		                return false;
		              }

		              auto SetcIt =
		                  findSetcInsertPt(MBB, *Last, SrcA, IsImm ? Register() : SrcB);
		              if (IsImm) {
		                BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                    .addReg(SrcA)
		                    .addImm(ImmVal);
		              } else {
		                BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                    .addReg(SrcA)
		                    .addReg(SrcB);
		              }
		              if (ZeroDefMI)
		                ZeroDefMI->eraseFromParent();
		              DefMI->eraseFromParent();
		              return true;
		            };

			            if (!EmittedImmSetc && !tryEmitLogicSetcMask()) {
			              auto SetcIt = findSetcInsertPt(MBB, *Last, LHSReg, RHSReg);
			              BuildMI(MBB, SetcIt, DebugLoc(), TII.get(SetcOpc))
			                  .addReg(LHSReg)
			                  .addReg(RHSReg);
			            }
			          }
	          Last->eraseFromParent();
	          Changed = true;
	          break;
	        }
	        default:
	          break;
	        }
		      }

	      auto isAbiTransferReg = [&](Register Reg) -> bool {
	        switch (Reg) {
	        case LinxISA::R2:
	        case LinxISA::R3:
	        case LinxISA::R4:
	        case LinxISA::R5:
	        case LinxISA::R6:
	        case LinxISA::R7:
	        case LinxISA::R8:
	        case LinxISA::R9:
	          return true;
	        default:
	          return false;
	        }
	      };

	      const bool ExitMayUseAbiRegs =
	          Kind == ExitKind::Call || Kind == ExitKind::ICall ||
	          Kind == ExitKind::Ret ||
	          ((Kind == ExitKind::Direct || Kind == ExitKind::Ind) && CallTargetOp);

	      auto isImplicitAbiUseAtExit = [&](Register Reg) -> bool {
	        return ExitMayUseAbiRegs && isAbiTransferReg(Reg);
	      };

	      // Peephole after BlockISA exit lowering (but before inserting block
	      // markers and T/U remapping): fold a word zero-extend shift-pair
	      // feeding a compare against zero into a SrcR `.uw` conversion
	      // modifier. The ISA only supports conversions on the right operand,
		      // so we flip operands when needed:
		      //
		      //   tmp1 = sll x, 32
			      //   tmp2 = srl tmp1, 32
			      //   c.setc.ne tmp2, zero
			      // =>
			      //   setc.ne zero, x<.uw>
			      auto prevNonMarkerMI = [&](MachineBasicBlock::iterator Pos) -> MachineInstr * {
			        auto It = Pos;
			        while (It != MBB.begin()) {
			          --It;
			          if (It->isDebugInstr() || isMarkerInstr(*It))
			            continue;
			          return &*It;
			        }
			        return nullptr;
			      };

			      auto hasUseBeforeDefFrom = [&](Register Reg,
			                                    MachineBasicBlock::iterator Start) -> bool {
			        if (!Reg)
			          return false;
			        for (auto It = Start; It != MBB.end(); ++It) {
			          MachineInstr &MI = *It;
			          if (MI.isDebugInstr() || isMarkerInstr(MI))
			            continue;
			          if (MI.definesRegister(Reg, &TRI))
			            return false;
			          for (const MachineOperand &MO : MI.operands()) {
			            if (!MO.isReg() || MO.isImplicit() || MO.isDef())
			              continue;
			            if (MO.getReg() == Reg)
			              return true;
			          }
			        }
			        return false;
			      };

			      auto matchAdjacentZextWByShiftPair =
			          [&](MachineInstr &UseMI, Register ZextReg, Register &OrigSrc,
			              MachineInstr *&SllMIOut, MachineInstr *&SrlMIOut) -> bool {
			        OrigSrc = Register();
			        SllMIOut = nullptr;
			        SrlMIOut = nullptr;
			        if (!ZextReg)
			          return false;

			        MachineInstr *SrlMI = prevNonMarkerMI(UseMI.getIterator());
			        if (!SrlMI || SrlMI->getOpcode() != LinxISA::SRLrr ||
			            SrlMI->getNumOperands() < 3 || !SrlMI->getOperand(1).isReg() ||
			            !SrlMI->getOperand(2).isReg())
			          return false;
			        if (!SrlMI->definesRegister(ZextReg, &TRI))
			          return false;

			        const Register Tmp1 = SrlMI->getOperand(1).getReg();
			        const Register ShAmtReg = SrlMI->getOperand(2).getReg();
			        if (!Tmp1 || !ShAmtReg)
			          return false;

			        MachineInstr *SllMI = prevNonMarkerMI(SrlMI->getIterator());
			        if (!SllMI || SllMI->getOpcode() != LinxISA::SLLrr ||
			            SllMI->getNumOperands() < 3 || !SllMI->getOperand(1).isReg() ||
			            !SllMI->getOperand(2).isReg())
			          return false;
			        if (!SllMI->definesRegister(Tmp1, &TRI))
			          return false;
			        if (SllMI->getOperand(2).getReg() != ShAmtReg)
			          return false;

			        auto getConstShiftAmt = [&](MachineInstr &Anchor) -> std::optional<int64_t> {
			          for (auto DI = Anchor.getIterator(); DI != MBB.begin();) {
			            --DI;
			            MachineInstr &DefMI = *DI;
			            if (DefMI.isDebugInstr() || isMarkerInstr(DefMI))
			              continue;
			            if (!DefMI.definesRegister(ShAmtReg, &TRI))
			              continue;
			            if (DefMI.getNumOperands() < 3 || !DefMI.getOperand(1).isReg() ||
			                DefMI.getOperand(1).getReg() != LinxISA::R0 ||
			                !DefMI.getOperand(2).isImm())
			              return std::nullopt;
			            switch (DefMI.getOpcode()) {
			            case LinxISA::ADDIri:
			            case LinxISA::ADDIWri:
			              return DefMI.getOperand(2).getImm();
			            default:
			              return std::nullopt;
			            }
			          }
			          if (ShAmtReg.isPhysical())
			            return getPhysRegConstAtMBBEntry(ShAmtReg);
			          return std::nullopt;
			        };

			        auto ShAmtC = getConstShiftAmt(*SrlMI);
			        if (!ShAmtC || *ShAmtC != 32)
			          return false;

			        const Register Src = SllMI->getOperand(1).getReg();
			        if (!Src)
			          return false;

			        // Ensure the shift results are not used later (otherwise removing
			        // the shifts would break the block).
			        if (hasUseBeforeDefFrom(ZextReg, std::next(UseMI.getIterator())))
			          return false;
			        if (hasUseBeforeDefFrom(Tmp1, std::next(SrlMI->getIterator())))
			          return false;

			        OrigSrc = Src;
			        SllMIOut = SllMI;
			        SrlMIOut = SrlMI;
			        return true;
			      };

			      for (auto It = MBB.begin(); It != MBB.end();) {
			        MachineInstr &MI = *It;
			        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
			          ++It;
		          continue;
		        }

		        const unsigned Opc = MI.getOpcode();
		        const bool IsEq =
		            (Opc == LinxISA::CSETC_EQ || Opc == LinxISA::SETC_EQ);
		        const bool IsNe =
		            (Opc == LinxISA::CSETC_NE || Opc == LinxISA::SETC_NE);
			        if (!IsEq && !IsNe) {
			          ++It;
			          continue;
			        }
			        if (!linxEnableSetcSrcRTypeFlags()) {
			          ++It;
			          continue;
			        }

		        if (MI.getNumOperands() < 2 || !MI.getOperand(0).isReg() ||
		            !MI.getOperand(1).isReg()) {
		          ++It;
		          continue;
		        }

		        const Register A = MI.getOperand(0).getReg();
		        const Register B = MI.getOperand(1).getReg();
		        if (!A || !B) {
		          ++It;
		          continue;
		        }
			        if (A != LinxISA::R0 && B != LinxISA::R0) {
			          ++It;
			          continue;
			        }

			        const Register ZextReg = (A == LinxISA::R0) ? B : A;
			        Register OrigSrc;
			        MachineInstr *SllMI = nullptr;
			        MachineInstr *SrlMI = nullptr;
			        if (!matchAdjacentZextWByShiftPair(MI, ZextReg, OrigSrc, SllMI, SrlMI)) {
			          ++It;
			          continue;
			        }

		        const unsigned NewOpc = IsEq ? LinxISA::SETC_EQ : LinxISA::SETC_NE;
		        MachineInstr *NewMI =
		            BuildMI(MBB, MI.getIterator(), MI.getDebugLoc(), TII.get(NewOpc))
		                .addReg(LinxISA::R0)
		                .addReg(OrigSrc)
		                .getInstr();
		        (void)NewMI;

		        auto NextIt = std::next(It);
		        MI.eraseFromParent();
		        SrlMI->eraseFromParent();
		        SllMI->eraseFromParent();
		        Changed = true;
		        It = NextIt;
		      }

		      for (auto It = MBB.begin(); It != MBB.end();) {
		        MachineInstr &MI = *It;
		        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
		          ++It;
		          continue;
		        }

		        const unsigned Opc = MI.getOpcode();
			        const bool IsEq = (Opc == LinxISA::CMPEQ);
			        const bool IsNe = (Opc == LinxISA::CMPNE);
			        if (!IsEq && !IsNe) {
			          ++It;
			          continue;
			        }
			        if (!linxEnableSetcSrcRTypeFlags()) {
			          ++It;
			          continue;
			        }

		        if (MI.getNumOperands() < 3 || !MI.getOperand(1).isReg() ||
		            !MI.getOperand(2).isReg()) {
		          ++It;
		          continue;
		        }

		        const Register SrcL = MI.getOperand(1).getReg();
		        const Register SrcR = MI.getOperand(2).getReg();
		        if (!SrcL || !SrcR) {
		          ++It;
		          continue;
		        }
		        if (SrcL != LinxISA::R0 && SrcR != LinxISA::R0) {
		          ++It;
		          continue;
		        }

			        const Register ZextReg = (SrcL == LinxISA::R0) ? SrcR : SrcL;
			        Register OrigSrc;
			        MachineInstr *SllMI = nullptr;
			        MachineInstr *SrlMI = nullptr;
			        if (!matchAdjacentZextWByShiftPair(MI, ZextReg, OrigSrc, SllMI, SrlMI)) {
			          ++It;
			          continue;
			        }

		        MI.getOperand(1).setReg(LinxISA::R0);
		        MI.getOperand(2).setReg(OrigSrc);

		        auto NextIt = std::next(It);
		        SrlMI->eraseFromParent();
		        SllMI->eraseFromParent();
		        Changed = true;
		        It = NextIt;
		      }

		      // Pre-blockify peephole: fuse MUL + ADD into MADD when the MUL result
		      // is single-use in the block.
		      //
		      //   tmp = mul  a, b
	      //   dst = add  tmp, c
	      // =>
	      //   dst = madd a, b, c
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &MulMI = *It;
	        if (MulMI.isDebugInstr() || isMarkerInstr(MulMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned MulOpc = MulMI.getOpcode();
	        const bool IsMul64 = (MulOpc == LinxISA::MULrr);
	        const bool IsMul32 = (MulOpc == LinxISA::MULWrr);
	        if (!IsMul64 && !IsMul32) {
	          ++It;
	          continue;
	        }

	        if (MulMI.getNumOperands() < 3 || !MulMI.getOperand(0).isReg() ||
	            !MulMI.getOperand(0).isDef() || !MulMI.getOperand(1).isReg() ||
	            !MulMI.getOperand(2).isReg()) {
	          ++It;
	          continue;
	        }

	        const Register Tmp = MulMI.getOperand(0).getReg();
	        const Register A = MulMI.getOperand(1).getReg();
	        const Register B = MulMI.getOperand(2).getReg();
	        if (!Tmp.isPhysical() || !A.isPhysical() || !B.isPhysical()) {
	          ++It;
	          continue;
	        }
	        if (isPhysRegLiveOutOfBlock(Tmp)) {
	          ++It;
	          continue;
	        }

	        // Find the next real instruction that reads Tmp and try to match ADD.
	        auto NextIt = std::next(It);
	        while (NextIt != MBB.end() &&
	               (NextIt->isDebugInstr() || isMarkerInstr(*NextIt)))
	          ++NextIt;
	        if (NextIt == MBB.end()) {
	          ++It;
	          continue;
	        }

	        MachineInstr &AddMI = *NextIt;
	        const unsigned AddOpc = AddMI.getOpcode();
	        const bool IsAdd64 = (AddOpc == LinxISA::ADDrr);
	        const bool IsAdd32 = (AddOpc == LinxISA::ADDWrr);
	        if (!((IsMul64 && IsAdd64) || (IsMul32 && IsAdd32))) {
	          ++It;
	          continue;
	        }

	        if (AddMI.getNumOperands() < 3 || !AddMI.getOperand(0).isReg() ||
	            !AddMI.getOperand(0).isDef() || !AddMI.getOperand(1).isReg() ||
	            !AddMI.getOperand(2).isReg()) {
	          ++It;
	          continue;
	        }

	        const Register Dst = AddMI.getOperand(0).getReg();
	        if (!Dst.isPhysical()) {
	          ++It;
	          continue;
	        }

	        const Register Op1 = AddMI.getOperand(1).getReg();
	        const Register Op2 = AddMI.getOperand(2).getReg();
	        Register C = Register();
	        if (Op1 == Tmp && Op2.isPhysical())
	          C = Op2;
	        else if (Op2 == Tmp && Op1.isPhysical())
	          C = Op1;
	        else {
	          ++It;
	          continue;
	        }

	        if (!hasSingleNonDbgUseInMBB(Tmp, &AddMI, &MulMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned MaddOpc = IsMul64 ? LinxISA::MADD : LinxISA::MADDW;
	        MachineInstr *NewMI =
	            BuildMI(MBB, AddMI.getIterator(), AddMI.getDebugLoc(),
	                    TII.get(MaddOpc), Dst)
	                .addReg(A)
	                .addReg(B)
	                .addReg(C)
	                .getInstr();

	        AddMI.eraseFromParent();
	        MulMI.eraseFromParent();
	        It = std::next(NewMI->getIterator());
	        Changed = true;
	      }

	      // Peephole: sink simple address calculations into immediate-offset
	      // loads/stores when the combined offset fits the instruction encoding.
	      //
	      // This favors using complex addressing modes over sharing an AGEN
	      // temporary across multiple memory ops:
	      //   addi tmp, base, C
	      //   lw   rd, [tmp + off]
	      // =>
	      //   lw   rd, [base + (C+off)]
	      //
	      // When all uses of `tmp` in the block are foldable, the AGEN is removed.
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &DefMI = *It;
	        if (DefMI.isDebugInstr() || isMarkerInstr(DefMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned DefOpc = DefMI.getOpcode();
	        const bool IsAddI = (DefOpc == LinxISA::ADDIri || DefOpc == LinxISA::ADDIWri);
	        const bool IsSubI = (DefOpc == LinxISA::SUBIri || DefOpc == LinxISA::SUBIWri);
	        if (!IsAddI && !IsSubI) {
	          ++It;
	          continue;
	        }
	        if (DefMI.getNumOperands() < 3 || !DefMI.getOperand(0).isReg() ||
	            !DefMI.getOperand(0).isDef() || !DefMI.getOperand(1).isReg() ||
	            !DefMI.getOperand(2).isImm()) {
	          ++It;
	          continue;
	        }

	        const Register Tmp = DefMI.getOperand(0).getReg();
	        const Register Base = DefMI.getOperand(1).getReg();
	        const int64_t Addend = IsSubI ? -DefMI.getOperand(2).getImm()
	                                      : DefMI.getOperand(2).getImm();
	        if (!Tmp || !Tmp.isPhysical() || !Base || !Base.isPhysical()) {
	          ++It;
	          continue;
	        }
	        // This peephole is intended for the "address-generation temporary"
	        // shape `addi tmp, base, C` where `tmp` is a distinct register. If we
	        // try to fold an in-place update (tmp == base), we would need to
	        // *remove* the defining instruction to preserve semantics; doing so
	        // is generally not possible for reserved/live-out regs like `sp`.
	        if (Tmp == Base) {
	          ++It;
	          continue;
	        }
	        const bool CanEraseDef = !isPhysRegLiveOutOfBlock(Tmp);

	        struct UseRef {
	          MachineInstr *MI;
	          unsigned BaseOpNo;
	          unsigned OffOpNo;
	        };
	        SmallVector<UseRef, 4> Uses;

	        auto isFoldableMem = [&](MachineInstr &MI, unsigned &BaseOpNo,
	                                 unsigned &OffOpNo) -> bool {
	          const unsigned Opc = MI.getOpcode();
	          switch (Opc) {
	          case LinxISA::LBI:
	          case LinxISA::LBUI:
	          case LinxISA::LHI:
	          case LinxISA::LHUI:
	          case LinxISA::LWI:
	          case LinxISA::LWUI:
	          case LinxISA::LDI:
	          case LinxISA::SBI:
	          case LinxISA::SHI:
	          case LinxISA::SWI:
	          case LinxISA::SDI:
	            BaseOpNo = 1;
	            OffOpNo = 2;
	            return true;
	          default:
	            return false;
	          }
	        };
	        auto memImmScale = [&](unsigned Opc) -> int64_t {
	          switch (Opc) {
	          case LinxISA::LBI:
	          case LinxISA::LBUI:
	          case LinxISA::SBI:
	            return 1;
	          case LinxISA::LHI:
	          case LinxISA::LHUI:
	          case LinxISA::SHI:
	            return 2;
	          case LinxISA::LWI:
	          case LinxISA::LWUI:
	          case LinxISA::SWI:
	            return 4;
	          case LinxISA::LDI:
	          case LinxISA::SDI:
	            return 8;
	          default:
	            return 1;
	          }
	        };

	        // Collect all uses from DefMI onwards until Tmp is redefined.
	        bool Bad = false;
	        bool StoppedByBaseDef = false;
	        for (auto UI = std::next(It), UE = MBB.end(); UI != UE; ++UI) {
	          MachineInstr &MI = *UI;
	          if (MI.isDebugInstr() || isMarkerInstr(MI))
	            continue;
	          if (MI.definesRegister(Tmp, &TRI))
	            break;
	          // Folding `tmp = base + C` into uses of `tmp` requires that `base`
	          // still holds the same value. If `base` is redefined, stop
	          // collecting fold candidates to avoid rewriting memory ops to use
	          // an updated base.
	          if (MI.definesRegister(Base, &TRI)) {
	            StoppedByBaseDef = true;
	            break;
	          }

	          unsigned BaseOpNo = 0, OffOpNo = 0;
	          if (!isFoldableMem(MI, BaseOpNo, OffOpNo)) {
	            if (MI.readsRegister(Tmp, &TRI)) {
	              Bad = true;
	              break;
	            }
	            continue;
	          }

	          if (BaseOpNo >= MI.getNumOperands() || OffOpNo >= MI.getNumOperands()) {
	            Bad = true;
	            break;
	          }
	          MachineOperand &BaseMO = MI.getOperand(BaseOpNo);
	          MachineOperand &OffMO = MI.getOperand(OffOpNo);
	          if (!BaseMO.isReg() || BaseMO.getReg() != Tmp) {
	            if (MI.readsRegister(Tmp, &TRI)) {
	              Bad = true;
	              break;
	            }
	            continue;
	          }
	          if (!OffMO.isImm()) {
	            Bad = true;
	            break;
	          }

	          const int64_t OldOff = OffMO.getImm();
	          // The machine-level mem-immediate is in *scaled units* (AArch64
	          // style): the final byte offset is `imm * access_size`. Convert the
	          // address-generation addend (bytes) into the same unit system.
	          const int64_t Scale = memImmScale(MI.getOpcode());
	          if (Scale <= 0 || (Addend % Scale) != 0) {
	            Bad = true;
	            break;
	          }
	          const int64_t NewOff = OldOff + (Addend / Scale);
	          if (!isInt<12>(NewOff)) {
	            Bad = true;
	            break;
	          }

	          Uses.push_back(UseRef{&MI, BaseOpNo, OffOpNo});
	        }

	        if (Bad || Uses.empty()) {
	          ++It;
	          continue;
	        }

	        for (const UseRef &U : Uses) {
	          U.MI->getOperand(U.BaseOpNo).setReg(Base);
	          const int64_t OldOff = U.MI->getOperand(U.OffOpNo).getImm();
	          const int64_t Scale = memImmScale(U.MI->getOpcode());
	          U.MI->getOperand(U.OffOpNo).setImm(OldOff + (Addend / Scale));
	        }

	        auto Next = std::next(It);
	        if (CanEraseDef && !StoppedByBaseDef) {
	          DefMI.eraseFromParent();
	          It = Next;
	        } else {
	          It = Next;
	        }
	        Changed = true;
	      }

	      // Peephole: fold `slli tmp, x, k; add dst, base, tmp` into
	      // `add base, x<<k, ->dst` (uses the ISA shamt field).
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &ShiftMI = *It;
	        if (ShiftMI.isDebugInstr() || isMarkerInstr(ShiftMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned ShiftOpc = ShiftMI.getOpcode();

	        // Peephole: fold `sll tmp, x, sh; srl/sra dst, tmp, sh` into
	        // `bxu/bxs x, M=0, N=<width>`.
	        if (ShiftOpc == LinxISA::SLLrr && ShiftMI.getNumOperands() >= 3 &&
	            ShiftMI.getOperand(2).isReg()) {
	          const Register ShDst = ShiftMI.getOperand(0).getReg();
	          const Register ShSrc = ShiftMI.getOperand(1).getReg();
	          const Register ShAmtReg = ShiftMI.getOperand(2).getReg();

	          auto NextIt = std::next(It);
	          while (NextIt != MBB.end() &&
	                 (NextIt->isDebugInstr() || isMarkerInstr(*NextIt)))
	            ++NextIt;
	          if (NextIt == MBB.end()) {
	            ++It;
	            continue;
	          }

	          MachineInstr &ShrMI = *NextIt;
	          const unsigned ShrOpc = ShrMI.getOpcode();
	          const bool IsShiftR = (ShrOpc == LinxISA::SRLrr || ShrOpc == LinxISA::SRArr);
	          if (!IsShiftR || ShrMI.getNumOperands() < 3) {
	            ++It;
	            continue;
	          }

	          if (ShrMI.getOperand(1).getReg() != ShDst ||
	              !ShrMI.getOperand(2).isReg() ||
	              ShrMI.getOperand(2).getReg() != ShAmtReg) {
	            ++It;
	            continue;
	          }

	          // The shifted temporary must be single-use.
	          if (isImplicitAbiUseAtExit(ShDst) ||
	              !hasSingleNonDbgUseInMBB(ShDst, &ShrMI, &ShiftMI)) {
	            ++It;
	            continue;
	          }

	          // Recover the constant shift amount from its defining instruction.
	          MachineInstr *ShAmtDefMI = nullptr;
	          auto getConstShiftAmt = [&](MachineInstr &UseMI) -> std::optional<int64_t> {
	            if (!ShAmtReg || !ShAmtReg.isPhysical())
	              return std::nullopt;
	            for (auto DI = UseMI.getIterator(); DI != MBB.begin();) {
	              --DI;
	              MachineInstr &DefMI = *DI;
	              if (DefMI.isDebugInstr() || isMarkerInstr(DefMI))
	                continue;
	              if (!DefMI.definesRegister(ShAmtReg, &TRI))
	                continue;
	              ShAmtDefMI = &DefMI;
	              break;
	            }
	            if (!ShAmtDefMI)
	              return std::nullopt;

	            MachineInstr &DefMI = *ShAmtDefMI;
	            if (DefMI.getNumOperands() < 3 || !DefMI.getOperand(1).isReg() ||
	                DefMI.getOperand(1).getReg() != LinxISA::R0 ||
	                !DefMI.getOperand(2).isImm())
	              return std::nullopt;
	            switch (DefMI.getOpcode()) {
	            case LinxISA::ADDIri:
	            case LinxISA::ADDIWri:
	              return DefMI.getOperand(2).getImm();
	            default:
	              return std::nullopt;
	            }
	          };

	          auto ShAmtC = getConstShiftAmt(ShiftMI);
	          if (!ShAmtC) {
	            ++It;
	            continue;
	          }
	          const int64_t ShAmt = *ShAmtC;
	          if (ShAmt <= 0 || ShAmt >= 64) {
	            ++It;
	            continue;
	          }

	          const int64_t Width = 64 - ShAmt;
	          const int64_t Imml = Width - 1; // N-1
	          const int64_t Imms = 0;         // M=0
	          if (!isUInt<6>(static_cast<uint64_t>(Imml))) {
	            ++It;
	            continue;
	          }

	          const Register ShrDst = ShrMI.getOperand(0).getReg();
	          const unsigned NewOpc = (ShrOpc == LinxISA::SRArr) ? LinxISA::BXS
	                                                             : LinxISA::BXU;
	          MachineInstr *NewMI =
	              BuildMI(MBB, ShrMI.getIterator(), ShrMI.getDebugLoc(),
	                      TII.get(NewOpc), ShrDst)
	                  .addReg(ShSrc)
	                  .addImm(Imml)
	                  .addImm(Imms)
	                  .getInstr();

	          // Remove the original two shifts.
	          ShrMI.eraseFromParent();
	          ShiftMI.eraseFromParent();

	          // If the shift-amount materialization is now dead and not live-out,
	          // remove it too.
	          if (ShAmtDefMI && !isPhysRegLiveOutOfBlock(ShAmtReg)) {
	            bool AnyUse = false;
	            for (const MachineInstr &MI : MBB) {
	              if (MI.isDebugInstr() || isMarkerInstr(MI))
	                continue;
	              if (MI.readsRegister(ShAmtReg, &TRI)) {
	                AnyUse = true;
	                break;
	              }
	            }
	            if (!AnyUse)
	              ShAmtDefMI->eraseFromParent();
	          }

	          Changed = true;
	          It = std::next(NewMI->getIterator());
	          continue;
	        }

	        const bool IsSLLI =
	            (ShiftOpc == LinxISA::SLLIri || ShiftOpc == LinxISA::SLLIWri);
	        if (!IsSLLI || ShiftMI.getNumOperands() < 3 || !ShiftMI.getOperand(2).isImm()) {
	          ++It;
	          continue;
        }

        const Register ShDst = ShiftMI.getOperand(0).getReg();
        const Register ShSrc = ShiftMI.getOperand(1).getReg();
        const int64_t ShAmt = ShiftMI.getOperand(2).getImm();
        if (ShAmt == 0) {
          ++It;
          continue;
        }

	        auto NextIt = std::next(It);
	        while (NextIt != MBB.end() &&
	               (NextIt->isDebugInstr() || isMarkerInstr(*NextIt)))
	          ++NextIt;
        if (NextIt == MBB.end()) {
          ++It;
          continue;
        }

	        MachineInstr &BinMI = *NextIt;
	        const unsigned BinOpc = BinMI.getOpcode();

	        // Peephole: fold `slli tmp, x, k; srli/srai dst, tmp, k` into BXU/BXS.
	        if (ShiftOpc == LinxISA::SLLIri &&
	            (BinOpc == LinxISA::SRLIri || BinOpc == LinxISA::SRAIri) &&
	            BinMI.getNumOperands() >= 3 && BinMI.getOperand(2).isImm() &&
	            BinMI.getOperand(1).isReg() && BinMI.getOperand(1).getReg() == ShDst &&
	            BinMI.getOperand(2).getImm() == ShAmt) {
	          if (hasSingleNonDbgUseInMBB(ShDst, &BinMI, &ShiftMI)) {
	            const int64_t Width = 64 - ShAmt;
	            const int64_t Imml = Width - 1;
	            const int64_t Imms = 0;
	            if (ShAmt > 0 && ShAmt < 64 &&
	                isUInt<6>(static_cast<uint64_t>(Imml))) {
	              const Register Dst = BinMI.getOperand(0).getReg();
	              const unsigned NewOpc = (BinOpc == LinxISA::SRAIri) ? LinxISA::BXS
	                                                                  : LinxISA::BXU;
	              MachineInstr *NewMI =
	                  BuildMI(MBB, BinMI.getIterator(), BinMI.getDebugLoc(),
	                          TII.get(NewOpc), Dst)
	                      .addReg(ShSrc)
	                      .addImm(Imml)
	                      .addImm(Imms)
	                      .getInstr();
	              BinMI.eraseFromParent();
	              ShiftMI.eraseFromParent();
	              Changed = true;
	              It = std::next(NewMI->getIterator());
	              continue;
	            }
	          }
	        }
        unsigned NewOpc = 0;

        if (ShiftOpc == LinxISA::SLLIri) {
          switch (BinOpc) {
          case LinxISA::ADDrr:
            NewOpc = LinxISA::ADDrr_SH;
            break;
          case LinxISA::SUBrr:
            NewOpc = LinxISA::SUBrr_SH;
            break;
          case LinxISA::ANDrr:
            NewOpc = LinxISA::ANDrr_SH;
            break;
          case LinxISA::ORrr:
            NewOpc = LinxISA::ORrr_SH;
            break;
          case LinxISA::XORrr:
            NewOpc = LinxISA::XORrr_SH;
            break;
          default:
            break;
          }
        } else if (ShiftOpc == LinxISA::SLLIWri) {
          switch (BinOpc) {
          case LinxISA::ADDWrr:
            NewOpc = LinxISA::ADDWrr_SH;
            break;
          case LinxISA::SUBWrr:
            NewOpc = LinxISA::SUBWrr_SH;
            break;
          case LinxISA::ANDWrr:
            NewOpc = LinxISA::ANDWrr_SH;
            break;
          case LinxISA::ORWrr:
            NewOpc = LinxISA::ORWrr_SH;
            break;
          case LinxISA::XORWrr:
            NewOpc = LinxISA::XORWrr_SH;
            break;
          default:
            break;
          }
        }

        if (!NewOpc || BinMI.getNumOperands() < 3) {
          ++It;
          continue;
        }

        const Register BinDst = BinMI.getOperand(0).getReg();
        Register BinOp1 = BinMI.getOperand(1).getReg();
        Register BinOp2 = BinMI.getOperand(2).getReg();

        Register Other;
        if (BinOp1 == ShDst)
          Other = BinOp2;
        else if (BinOp2 == ShDst)
          Other = BinOp1;
        else {
          ++It;
          continue;
        }

        // SUB is not commutative; only fold when the shifted value is the RHS.
        const bool IsSub =
            (BinOpc == LinxISA::SUBrr) || (BinOpc == LinxISA::SUBWrr);
        if (IsSub && BinOp1 == ShDst) {
          ++It;
          continue;
        }

        // Ignore ShiftMI itself: register allocation may legally coalesce
        // `tmp` with `x`, yielding an in-place shift (e.g. `r3 = slli r3, k`).
	        if (isImplicitAbiUseAtExit(ShDst) ||
	            !hasSingleNonDbgUseInMBB(ShDst, &BinMI, &ShiftMI)) {
          ++It;
          continue;
        }

        MachineInstr *NewMI =
            BuildMI(MBB, BinMI.getIterator(), BinMI.getDebugLoc(),
                    TII.get(NewOpc), BinDst)
                .addReg(Other)
                .addReg(ShSrc)
                .addImm(ShAmt)
                .getInstr();
        BinMI.eraseFromParent();
        ShiftMI.eraseFromParent();
        Changed = true;
        It = std::next(NewMI->getIterator());
      }

      if (IsTileBlock) {
        // If the pass runs twice, strip any stale standard start marker(s)
        // and keep the tile header intact.
        auto It = MBB.begin();
        while (It != MBB.end() && It->isPHI())
          ++It;
        while (It != MBB.end() && isStdBStartOpcode(It->getOpcode())) {
          It = MBB.erase(It);
          Changed = true;
        }
      } else {
	      // Insert `BSTART.STD <kind>` after PHIs.
	      auto InsertBStart = MBB.begin();
	      while (InsertBStart != MBB.end() && InsertBStart->isPHI())
	        ++InsertBStart;

      // Remove any existing start marker (in case the pass runs twice).
      if (InsertBStart != MBB.end() &&
          (InsertBStart->getOpcode() == LinxISA::CBSTART_STD ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_FALL ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_DIRECT ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_COND ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_CALL ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_IND ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_ICALL ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_RET)) {
        InsertBStart = MBB.erase(InsertBStart);
        Changed = true;
      }

	      MachineInstr *BStartMI = nullptr;
	      MachineInstr *SetRetMI = nullptr;
	      switch (Kind) {
      case ExitKind::Fall:
        // Prefer the compressed BrType marker: C.BSTART (FALL).
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::CBSTART_STD))
                       .addImm(1) // BrType = FALL
                       .getInstr();
        break;
      case ExitKind::Direct:
        if (CallTargetOp) {
          BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                             TII.get(LinxISA::BSTART_STD_DIRECT))
                         .add(*CallTargetOp)
                         .getInstr();
        } else {
          if (!TargetBB)
            report_fatal_error("Linx: missing direct branch target");
          TargetBB->setLabelMustBeEmitted();
          BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                             TII.get(LinxISA::BSTART_STD_DIRECT))
                         .addMBB(TargetBB)
                         .getInstr();
        }
        break;
      case ExitKind::Cond:
        if (TargetBB)
          TargetBB->setLabelMustBeEmitted();
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::BSTART_STD_COND))
                       .addMBB(TargetBB)
                       .getInstr();
        break;
	      case ExitKind::Call: {
	        if (!CallTargetOp)
	          report_fatal_error("Linx: missing call target operand");
	        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
	                           TII.get(LinxISA::BSTART_STD_CALL))
	                       .add(*CallTargetOp)
	                       .getInstr();
	        // Set return target for the call (ra = PC + imm20<<1). The ISA
	        // requires SETRET to be immediately after the CALL BSTART header.
	        if (ReturnBB) {
	          ReturnBB->setLabelMustBeEmitted();
	          auto InsertSetRet = std::next(BStartMI->getIterator());
	          SetRetMI =
	              BuildMI(MBB, InsertSetRet, DebugLoc(), TII.get(LinxISA::SETRET))
	                  .addMBB(ReturnBB)
	                  .getInstr();
	        }
	        break;
	      }
      case ExitKind::Ret:
        // Prefer the compressed BrType marker: C.BSTART (RET).
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::CBSTART_STD))
                       .addImm(7) // BrType = RET
                       .getInstr();
        break;
      case ExitKind::Ind:
        // Prefer the compressed BrType marker: C.BSTART (IND).
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::CBSTART_STD))
                       .addImm(5) // BrType = IND
                       .getInstr();
        break;
	      case ExitKind::ICall:
	        // Prefer the compressed BrType marker: C.BSTART (ICALL).
	        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
	                           TII.get(LinxISA::CBSTART_STD))
	                       .addImm(6) // BrType = ICALL
	                       .getInstr();
	        // Indirect calls behave like CALL blocks but select the callee via
	        // SETC.TGT. Emit SETRET so the continuation block is reachable after
	        // the callee returns. SETRET must be immediately after the BSTART
	        // header.
	        if (ReturnBB) {
	          ReturnBB->setLabelMustBeEmitted();
	          auto InsertSetRet = std::next(BStartMI->getIterator());
	          SetRetMI =
	              BuildMI(MBB, InsertSetRet, DebugLoc(), TII.get(LinxISA::SETRET))
	                  .addMBB(ReturnBB)
	                  .getInstr();
	        }
	        break;
	      }
      Changed = true;

		      if (HeaderSetcTgtReg) {
		        auto InsertSetcTgt = std::next(BStartMI->getIterator());
		        if (SetRetMI)
		          InsertSetcTgt = std::next(SetRetMI->getIterator());
		        BuildMI(MBB, InsertSetcTgt, DebugLoc(), TII.get(LinxISA::CSETC_TGT))
		            .addReg(*HeaderSetcTgtReg);
		        Changed = true;
		      }

		      if (ICallSetcTgtReg) {
		        auto InsertSetcTgt = std::next(BStartMI->getIterator());
		        if (SetRetMI)
		          InsertSetcTgt = std::next(SetRetMI->getIterator());

		        auto InsertAfterCalleeDef = InsertSetcTgt;
		        for (auto It = InsertSetcTgt, E = MBB.instr_end(); It != E; ++It) {
		          if (It->isDebugInstr() || isMarkerInstr(*It))
		            continue;
		          if (It->modifiesRegister(*ICallSetcTgtReg, &TRI)) {
		            InsertAfterCalleeDef = std::next(It);
		          }
		        }

		        BuildMI(MBB, InsertAfterCalleeDef, DebugLoc(),
		                TII.get(LinxISA::CSETC_TGT))
		            .addReg(*ICallSetcTgtReg);
		        Changed = true;
		      }

	      // Assign block-local values to the hand queues.
	      //
	      // Hardware semantics: every definition to `t` shifts older values into
	      // `t#1..t#4` (similarly for `u` -> `u#1..u#4`). We only rewrite values
	      // whose uses occur within the next 4 queued defs for a chosen hand.
	      //
	      // Peepholes:
	      // - Remove redundant `addw X, zero, ->Y` when Y is used only by an
	      //   immediate SETC operation. This keeps the compare closer to the
	      //   original producer (often a 32-bit load), and exposes more values
	      //   as candidates for T/U-hand remapping.
	      struct UseSite {
	        MachineInstr *MI = nullptr;
	        unsigned OpNo = 0;
	        unsigned UseIdx = 0;
	      };
	      struct Segment {
	        Register Reg;
	        MachineInstr *DefMI = nullptr;
	        unsigned DefOpNo = 0;
	        unsigned DefIdx = 0;
	        SmallVector<UseSite, 4> Uses;
	        bool ClosedByRedef = false;
	        bool TouchesInlineAsm = false;
	      };

      SmallVector<Segment, 32> Segs;
      DenseMap<unsigned, unsigned> ActiveSeg; // PhysReg.id() -> Segs index
	      unsigned InstIdx = 0;

        auto getNextNonMarker = [&](MachineBasicBlock::iterator It)
            -> MachineBasicBlock::iterator {
          auto E = MBB.end();
          while (It != E) {
            if (!It->isDebugInstr() && !isMarkerInstr(*It))
              break;
            ++It;
          }
          return It;
        };

        auto isSetcImmediate = [&](const MachineInstr &MI) -> bool {
          switch (MI.getOpcode()) {
          case LinxISA::SETC_EQI:
          case LinxISA::SETC_NEI:
          case LinxISA::SETC_LTI:
          case LinxISA::SETC_GEI:
          case LinxISA::SETC_LTUI:
          case LinxISA::SETC_GEUI:
            return true;
          default:
            return false;
          }
        };

        auto hasAnyUseAfter = [&](Register Reg, MachineBasicBlock::iterator From)
            -> bool {
          for (auto It = From, E = MBB.end(); It != E; ++It) {
            MachineInstr &MI = *It;
            if (MI.isDebugInstr() || isMarkerInstr(MI))
              continue;
            for (const MachineOperand &MO : MI.operands()) {
              if (!MO.isReg() || MO.isImplicit())
                continue;
              if (MO.isUse() && MO.getReg() == Reg)
                return true;
            }
          }
          return false;
        };

        // Identify whether this block (MachineBasicBlock) contains any BlockISA
        // headers that may implicitly consume ABI argument/return registers
        // (CALL/ICALL/RET). If so, be conservative about remapping a0-a7 since
        // those uses are not modeled as explicit register operands on the block
        // header itself.
        //
        // Note: a single MachineBasicBlock can contain multiple BlockISA blocks
        // (markers like C.BSTART ...). We must therefore scan all marker
        // instructions, not just the first header.
        auto markerHasImplicitAbiUses = [&](const MachineInstr &MI) -> bool {
          const unsigned Opc = MI.getOpcode();
          if (Opc == LinxISA::BSTART_STD_CALL || Opc == LinxISA::BSTART_STD_ICALL ||
              Opc == LinxISA::BSTART_STD_RET) {
            return true;
          }
          if (Opc == LinxISA::CBSTART_STD && MI.getNumOperands() >= 1 &&
              MI.getOperand(0).isImm()) {
            const int64_t BrType = MI.getOperand(0).getImm() & 0x7;
            return BrType == 4 /*CALL*/ || BrType == 6 /*ICALL*/ ||
                   BrType == 7 /*RET*/;
          }
          return false;
        };

        bool BlockHasImplicitAbiUses = false;
        for (const MachineInstr &MI : MBB) {
          if (!isMarkerInstr(MI))
            continue;
          if (markerHasImplicitAbiUses(MI)) {
            BlockHasImplicitAbiUses = true;
            break;
          }
        }

        // Peephole: ADDWrr dst, src, zero; SETC_*I dst, imm  ==> SETC_*I src, imm
        for (auto It = MBB.begin(), E = MBB.end(); It != E;) {
          MachineInstr &MI = *It;
          ++It;
          if (MI.isDebugInstr() || isMarkerInstr(MI))
            continue;
          if (MI.getOpcode() != LinxISA::ADDWrr)
            continue;
          if (MI.getNumOperands() < 3)
            continue;
          if (!MI.getOperand(0).isReg() || !MI.getOperand(0).isDef())
            continue;
          if (!MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
            continue;
          const Register Dst = MI.getOperand(0).getReg();
          const Register A = MI.getOperand(1).getReg();
          const Register B = MI.getOperand(2).getReg();
          if (!Dst.isPhysical())
            continue;

          Register Src;
          if (A == LinxISA::R0 && B != LinxISA::R0)
            Src = B;
          else if (B == LinxISA::R0 && A != LinxISA::R0)
            Src = A;
          else
            continue;

          auto NextIt = getNextNonMarker(MI.getIterator());
          if (NextIt == E)
            continue;
          // NextIt currently points to MI itself; advance one.
          NextIt = getNextNonMarker(std::next(MI.getIterator()));
          if (NextIt == E)
            continue;
          MachineInstr &NextMI = *NextIt;
          if (!isSetcImmediate(NextMI))
            continue;
          if (NextMI.getNumOperands() < 2 || !NextMI.getOperand(0).isReg())
            continue;
          if (NextMI.getOperand(0).getReg() != Dst)
            continue;

          // If Dst is used again later, or is live-out to a successor, keep
          // the ADDW. The SETC immediate fold only rewrites the local compare;
          // removing the defining copy would strand any cross-block uses of
          // the original architectural register.
          if (hasAnyUseAfter(Dst, std::next(NextIt)) ||
              isPhysRegLiveOutOfBlock(Dst))
            continue;

          NextMI.getOperand(0).setReg(Src);
          MI.eraseFromParent();
          Changed = true;
        }

        if (linxEnableT1Motion()) {
          auto isPureSingleDefCandidate = [&](const MachineInstr &MI) -> bool {
            if (MI.isDebugInstr() || isMarkerInstr(MI) || MI.isCFIInstruction())
              return false;
            if (MI.isInlineAsm() || MI.isCall() || MI.isTerminator())
              return false;
            if (MI.mayLoadOrStore() || MI.hasUnmodeledSideEffects())
              return false;

            switch (MI.getOpcode()) {
            case LinxISA::ADDrr:
            case LinxISA::SUBrr:
            case LinxISA::ANDrr:
            case LinxISA::ORrr:
            case LinxISA::XORrr:
            case LinxISA::ADDWrr:
            case LinxISA::SUBWrr:
            case LinxISA::ANDWrr:
            case LinxISA::ORWrr:
            case LinxISA::XORWrr:
            case LinxISA::ADDIri:
            case LinxISA::SUBIri:
            case LinxISA::ANDIri:
            case LinxISA::ORIri:
            case LinxISA::XORIri:
            case LinxISA::ADDIWri:
            case LinxISA::SUBIWri:
            case LinxISA::ANDIWri:
            case LinxISA::ORIWri:
            case LinxISA::XORIWri:
            case LinxISA::SLLIri:
            case LinxISA::SRLIri:
            case LinxISA::SRAIri:
            case LinxISA::SLLIWri:
            case LinxISA::SRLIWri:
            case LinxISA::SRAIWri:
              return true;
            default:
              return false;
            }
          };

          auto getSingleDefReg = [&](MachineInstr &MI) -> Register {
            Register DefReg;
            for (const MachineOperand &MO : MI.operands()) {
              if (!MO.isReg() || MO.isImplicit() || !MO.isDef())
                continue;
              if (!MO.getReg().isPhysical())
                return Register();
              if (DefReg)
                return Register();
              DefReg = MO.getReg();
            }
            return DefReg;
          };

          auto findSingleUseMI = [&](MachineInstr &DefMI,
                                     Register DefReg) -> MachineInstr * {
            MachineInstr *UseMI = nullptr;
            for (auto UI = std::next(DefMI.getIterator()), UE = MBB.instr_end();
                 UI != UE; ++UI) {
              MachineInstr &MI = *UI;
              if (MI.isDebugInstr() || isMarkerInstr(MI))
                continue;
              for (const MachineOperand &MO : MI.operands()) {
                if (!MO.isReg() || MO.isImplicit() || MO.isDef())
                  continue;
                if (MO.getReg() != DefReg)
                  continue;
                if (UseMI && UseMI != &MI)
                  return nullptr;
                UseMI = &MI;
                break;
              }
            }
            return UseMI;
          };

          auto canSinkBeforeUse = [&](MachineInstr &DefMI, MachineInstr &UseMI,
                                      Register DefReg) -> bool {
            if (&DefMI == &UseMI)
              return false;
            if (!hasSingleNonDbgUseInMBB(DefReg, &UseMI, &DefMI))
              return false;
            if (isPhysRegLiveOutOfBlock(DefReg))
              return false;

            SmallVector<Register, 4> SrcRegs;
            for (const MachineOperand &MO : DefMI.operands()) {
              if (!MO.isReg() || MO.isImplicit() || MO.isDef())
                continue;
              Register R = MO.getReg();
              if (R)
                SrcRegs.push_back(R);
            }

            for (auto It = std::next(DefMI.getIterator()); &*It != &UseMI; ++It) {
              MachineInstr &Mid = *It;
              if (Mid.isDebugInstr() || Mid.isCFIInstruction())
                continue;
              if (isMarkerInstr(Mid))
                return false;
              if (Mid.isInlineAsm() || Mid.isCall() || Mid.isTerminator())
                return false;
              if (Mid.mayLoadOrStore() || Mid.hasUnmodeledSideEffects())
                return false;
              if (Mid.readsRegister(DefReg, &TRI) || Mid.definesRegister(DefReg, &TRI))
                return false;
              for (Register SrcReg : SrcRegs)
                if (SrcReg && Mid.definesRegister(SrcReg, &TRI))
                  return false;
            }
            return true;
          };

          SmallPtrSet<MachineInstr *, 8> BlockedUseMIs;
          for (auto It = MBB.begin(), E = MBB.end(); It != E;) {
            MachineInstr &MI = *It;
            ++It;
            if (!isPureSingleDefCandidate(MI))
              continue;

            Register DefReg = getSingleDefReg(MI);
            if (!DefReg)
              continue;

            MachineInstr *UseMI = findSingleUseMI(MI, DefReg);
            if (!UseMI || std::next(MI.getIterator()) == UseMI->getIterator())
              continue;
            if (BlockedUseMIs.contains(UseMI))
              continue;
            if (!canSinkBeforeUse(MI, *UseMI, DefReg))
              continue;

            MI.moveBefore(UseMI);
            BlockedUseMIs.insert(UseMI);
            Changed = true;
          }
        }

		      auto isCandidatePhysReg = [&](Register Reg) -> bool {
		        if (!Reg || !Reg.isPhysical())
		          return false;
            // Only remap architectural GPR values. Never rewrite non-GPR
            // physical registers (e.g. tile regs) into the T/U hand queues.
            if (!LinxISA::GPRRegClass.contains(Reg))
              return false;
		        if (Reg.id() >= Reserved.size())
	          return false;
		        if (Reserved.test(Reg.id()))
		          return false;
	        // The a0-a7 argument registers are ABI-visible at CALL/ICALL/RET
	        // boundaries. Only allow remapping them inside blocks that do not
	        // implicitly consume ABI regs.
	        if (BlockHasImplicitAbiUses) {
	          switch (Reg) {
	          case LinxISA::R2:
	          case LinxISA::R3:
	          case LinxISA::R4:
	          case LinxISA::R5:
	          case LinxISA::R6:
	          case LinxISA::R7:
	          case LinxISA::R8:
	          case LinxISA::R9:
	            return false;
	          default:
	            break;
	          }
	        }
	        return true;
	      };

      for (MachineInstr &MI : MBB) {
        if (MI.isDebugInstr() || isMarkerInstr(MI))
          continue;

        // Process uses before defs to handle read-modify-write forms.
        for (unsigned OpNo = 0; OpNo < MI.getNumOperands(); ++OpNo) {
          MachineOperand &MO = MI.getOperand(OpNo);
          if (!MO.isReg() || MO.isImplicit() || MO.isDef())
            continue;

          Register Reg = MO.getReg();
          if (!isCandidatePhysReg(Reg))
            continue;

          auto It = ActiveSeg.find(Reg.id());
          if (It == ActiveSeg.end())
            continue;

	          Segment &S = Segs[It->second];
	          S.Uses.push_back(UseSite{&MI, OpNo, InstIdx});
	          if (MI.isInlineAsm())
	            S.TouchesInlineAsm = true;
	        }

        for (unsigned OpNo = 0; OpNo < MI.getNumOperands(); ++OpNo) {
          MachineOperand &MO = MI.getOperand(OpNo);
          if (!MO.isReg() || MO.isImplicit() || !MO.isDef())
            continue;

          Register Reg = MO.getReg();
          if (!isCandidatePhysReg(Reg))
            continue;

          // Close the previous segment (if any) for this physical register.
          auto It = ActiveSeg.find(Reg.id());
          if (It != ActiveSeg.end()) {
            Segs[It->second].ClosedByRedef = true;
            ActiveSeg.erase(It);
          }

          Segment S;
          S.Reg = Reg;
          S.DefMI = &MI;
          S.DefOpNo = OpNo;
          S.DefIdx = InstIdx;
          S.TouchesInlineAsm = MI.isInlineAsm();
          ActiveSeg[Reg.id()] = Segs.size();
          Segs.push_back(S);
        }

        ++InstIdx;
      }

	      SmallVector<unsigned, 32> CandidateSegs;
	      CandidateSegs.reserve(Segs.size());
	      for (unsigned I = 0; I < Segs.size(); ++I) {
	        const Segment &S = Segs[I];
	        if (!S.DefMI || S.Uses.empty())
	          continue;
	        // Never remap values that touch inline asm. Inline asm operand
	        // constraints expect architectural registers; rewriting defs/uses to
	        // the T/U hand queues breaks the ABI-visible semantics (notably
	        // syscall/ACR entry/exit sequences).
	        if (S.TouchesInlineAsm)
	          continue;
	        unsigned LastUseIdx = 0;
	        for (const UseSite &U : S.Uses)
	          LastUseIdx = std::max(LastUseIdx, U.UseIdx);
	        if (LastUseIdx <= S.DefIdx)
	          continue;
	        // If the value is live-out, we can't remap it to the hand queue.
	        if (!S.ClosedByRedef && isPhysRegLiveOutOfBlock(S.Reg))
	          continue;
	        CandidateSegs.push_back(I);
	      }

      if (!CandidateSegs.empty()) {
        enum class Hand : uint8_t { None, T, U };

        auto isTCompressibleDef = [&](const MachineInstr &MI) -> bool {
          switch (MI.getOpcode()) {
          case LinxISA::ADDrr:
          case LinxISA::SUBrr:
          case LinxISA::ANDrr:
          case LinxISA::ORrr:
            return true;
          case LinxISA::ADDIri:
          case LinxISA::SUBIri: {
            if (MI.getNumOperands() >= 3 && MI.getOperand(2).isImm())
              return isInt<5>(MI.getOperand(2).getImm());
            return false;
          }
          case LinxISA::LWI:
          case LinxISA::LDI: {
            if (MI.getNumOperands() >= 3 && MI.getOperand(2).isImm())
              return isInt<5>(MI.getOperand(2).getImm());
            return false;
          }
          case LinxISA::SLLIri:
          case LinxISA::SRLIri:
            if (!linxEnableCShift16())
              return false;
            if (MI.getNumOperands() >= 3 && MI.getOperand(2).isImm())
              return isUInt<5>(MI.getOperand(2).getImm());
            return false;
          default:
            return false;
          }
        };

        // Greedy assignment in reverse def order. For each candidate, choose a
        // hand where the value is still within the 4-deep queue at its use.
        SmallVector<unsigned, 32> Sorted = CandidateSegs;
        llvm::sort(Sorted, [&](unsigned A, unsigned B) {
          if (Segs[A].DefIdx != Segs[B].DefIdx)
            return Segs[A].DefIdx > Segs[B].DefIdx;
          // Visit the shallowest result first: later def operands are pushed
          // later and therefore sit nearer the top of the LIFO hand queue.
          return Segs[A].DefOpNo > Segs[B].DefOpNo;
        });

	        SmallVector<unsigned, 32> AssignedT;
	        SmallVector<unsigned, 32> AssignedU;
	        SmallVector<Hand, 32> AssignedHand(Segs.size(), Hand::None);
	        DenseMap<const MachineInstr *, unsigned> UsedHandReads; // bit0=T, bit1=U
	
	        auto countBetweenAt = [&](ArrayRef<unsigned> Assigned,
	                                  const Segment &S, unsigned UseIdx) {
	          unsigned Between = 0;
	          for (unsigned J : Assigned) {
	            const Segment &B = Segs[J];
	            // Multi-def instructions push results in operand order. A later
	            // def operand from the same instruction therefore occupies a
	            // shallower queue slot and must count just like a later
	            // instruction definition.
	            const bool PushedAfter =
	                B.DefIdx > S.DefIdx ||
	                (B.DefIdx == S.DefIdx && B.DefOpNo > S.DefOpNo);
	            if (PushedAfter && B.DefIdx < UseIdx)
	              ++Between;
	          }
	          return Between;
	        };
	
	        auto getLastUseIdx = [&](const Segment &S) -> unsigned {
	          unsigned LastUseIdx = 0;
	          for (const UseSite &U : S.Uses)
	            LastUseIdx = std::max(LastUseIdx, U.UseIdx);
	          return LastUseIdx;
	        };
	
	        auto hasMultiUseInSameMI = [&](const Segment &S) -> bool {
	          SmallPtrSet<const MachineInstr *, 4> Seen;
	          for (const UseSite &U : S.Uses) {
	            if (!Seen.insert(U.MI).second)
	              return true;
	          }
	          return false;
	        };
	
	        auto canReadHandInAllUses = [&](Hand H, const Segment &S) -> bool {
	          const unsigned Bit = (H == Hand::T) ? 0x1u : 0x2u;
	          for (const UseSite &U : S.Uses) {
	            const unsigned Mask = UsedHandReads.lookup(U.MI);
	            if ((Mask & Bit) != 0)
	              return false;
	          }
	          return true;
	        };
	
	        for (unsigned I : Sorted) {
	          const Segment &S = Segs[I];
	          const unsigned LastUseIdx = getLastUseIdx(S);
	          unsigned BetweenT = countBetweenAt(AssignedT, S, LastUseIdx);
	          unsigned BetweenU = countBetweenAt(AssignedU, S, LastUseIdx);
	
	          // Per-queue port rule: at most one T read and one U read per
	          // instruction. Avoid mapping multiple operands in the same MI to the
	          // same hand (even if they are the same physical register).
	          if (hasMultiUseInSameMI(S))
	            continue;
	
	          const bool CanT = BetweenT <= 3 && canReadHandInAllUses(Hand::T, S);
	          const bool CanU = BetweenU <= 3 && canReadHandInAllUses(Hand::U, S);
	          if (!CanT && !CanU)
	            continue;
	
	          Hand H = Hand::None;

	          // Prefer mapping defs that can become 16-bit ops to the T-hand.
	          const bool PreferT = isTCompressibleDef(*S.DefMI);

	          if (PreferT && CanT) {
	            H = Hand::T;
	          } else if (CanT && CanU) {
	            H = (BetweenT <= BetweenU) ? Hand::T : Hand::U;
	          } else if (CanT) {
	            H = Hand::T;
	          } else {
	            H = Hand::U;
	          }
	
	          AssignedHand[I] = H;
	          if (H == Hand::T)
	            AssignedT.push_back(I);
	          else if (H == Hand::U)
	            AssignedU.push_back(I);
	
	          const unsigned Bit = (H == Hand::T) ? 0x1u : 0x2u;
	          for (const UseSite &U : S.Uses)
	            UsedHandReads[U.MI] |= Bit;
	        }
	
	        for (unsigned I : Sorted) {
	          const Segment &S = Segs[I];
	          Hand H = AssignedHand[I];
	          if (H == Hand::None)
	            continue;
	
	          MachineOperand &DefMO = S.DefMI->getOperand(S.DefOpNo);
	          DefMO.setReg(H == Hand::T ? LinxISA::U4 : LinxISA::U3); // "->t"/"->u"
	
		          for (const UseSite &U : S.Uses) {
		            const unsigned Between = (H == Hand::T)
		                                         ? countBetweenAt(AssignedT, S, U.UseIdx)
		                                         : countBetweenAt(AssignedU, S, U.UseIdx);
		            const unsigned Index = Between + 1;
		            Register UseReg = (H == Hand::T) ? getTQueueUseReg(Index)
		                                             : getUQueueUseReg(Index);
		            if (!UseReg)
		              continue;
		            MachineOperand &UseMO = U.MI->getOperand(U.OpNo);
		            UseMO.setReg(UseReg); // "t#k"/"u#k"
		          }
		          Changed = true;
		        }
			      }
	      } // end !IsTileBlock

	      // Post-remap peephole: use block-private T-hand for simple SETC
	      // conditions that consume a single-use PC-relative load result.
	      //
	      // This improves code size and scheduling by keeping the loaded value in
	      // the block-private queue:
	      //   lw.pcr [sym], ->aX
	      //   setc.*i aX, imm
	      // =>
	      //   lw.pcr [sym], ->t
	      //   setc.*i t#1, imm
	      auto isPcrLoadOpc = [&](unsigned Opc) -> bool {
	        switch (Opc) {
	        case LinxISA::LB_PCR:
	        case LinxISA::LBU_PCR:
	        case LinxISA::LH_PCR:
	        case LinxISA::LHU_PCR:
	        case LinxISA::LW_PCR:
	        case LinxISA::LWU_PCR:
	        case LinxISA::LD_PCR:
	        case LinxISA::HL_LB_PCR:
	        case LinxISA::HL_LBU_PCR:
	        case LinxISA::HL_LH_PCR:
	        case LinxISA::HL_LHU_PCR:
	        case LinxISA::HL_LW_PCR:
	        case LinxISA::HL_LWU_PCR:
	        case LinxISA::HL_LD_PCR:
	          return true;
	        default:
	          return false;
	        }
	      };
	      auto isSetcImmOpcode = [&](unsigned Opc) -> bool {
	        switch (Opc) {
	        case LinxISA::SETC_EQI:
	        case LinxISA::SETC_NEI:
	        case LinxISA::SETC_LTI:
	        case LinxISA::SETC_GEI:
	        case LinxISA::SETC_LTUI:
	        case LinxISA::SETC_GEUI:
	        case LinxISA::SETC_ANDI:
	        case LinxISA::SETC_ORI:
	        case LinxISA::HLSETC_ANDI:
	        case LinxISA::HLSETC_ORI:
	          return true;
	        default:
	          return false;
	        }
	      };
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &LdMI = *It;
	        if (LdMI.isDebugInstr() || isMarkerInstr(LdMI)) {
	          ++It;
	          continue;
	        }
	        if (!isPcrLoadOpc(LdMI.getOpcode()) || LdMI.getNumOperands() < 2 ||
	            !LdMI.getOperand(0).isReg() || !LdMI.getOperand(0).isDef()) {
	          ++It;
	          continue;
	        }
	        const Register Dst = LdMI.getOperand(0).getReg();
	        if (!Dst.isPhysical() || Dst == LinxISA::U4 || Dst == LinxISA::U3) {
	          ++It;
	          continue;
	        }
	        auto NextIt = std::next(It);
	        while (NextIt != MBB.end() &&
	               (NextIt->isDebugInstr() || isMarkerInstr(*NextIt)))
	          ++NextIt;
	        if (NextIt == MBB.end()) {
	          ++It;
	          continue;
	        }
	        MachineInstr &SetcMI = *NextIt;
	        if (!isSetcImmOpcode(SetcMI.getOpcode()) || SetcMI.getNumOperands() < 2 ||
	            !SetcMI.getOperand(0).isReg() || SetcMI.getOperand(0).getReg() != Dst) {
	          ++It;
	          continue;
	        }

	        if (!hasSingleNonDbgUseInMBB(Dst, &SetcMI, &LdMI)) {
	          ++It;
	          continue;
	        }
	        // The T/U hand queues are block-private: values pushed into the queue
	        // do not survive control-flow edges. Only rewrite when the loaded
	        // value is guaranteed not to be live-out of this MachineBasicBlock.
	        if (isPhysRegLiveOutOfBlock(Dst)) {
	          ++It;
	          continue;
	        }

	        LdMI.getOperand(0).setReg(LinxISA::U4);   // "->t"
	        SetcMI.getOperand(0).setReg(LinxISA::T1); // "t#1"
	        Changed = true;
	        It = std::next(SetcMI.getIterator());
	      }

	      // Post-remap peephole: use 16-bit C.ZEXT.* when extracting low bits into
	      // the T-hand implicit destination.
	      //
	      // The earlier shift-folding peephole produces `BXU src, M=0, N=<width>`.
	      // When the result is block-private (`->t`), we can encode common widths
	      // with compressed zext forms.
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &MI = *It;
	        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
	          ++It;
	          continue;
	        }
	        if (MI.getOpcode() != LinxISA::BXU || MI.getNumOperands() < 4) {
	          ++It;
	          continue;
	        }
	        if (!MI.getOperand(0).isReg() || !MI.getOperand(0).isDef() ||
	            !MI.getOperand(1).isReg() || !MI.getOperand(2).isImm() ||
	            !MI.getOperand(3).isImm()) {
	          ++It;
	          continue;
	        }

	        const Register Dst = MI.getOperand(0).getReg();
	        if (Dst != LinxISA::U4) { // compressed form writes implicit `->t`
	          ++It;
	          continue;
	        }

	        const Register Src = MI.getOperand(1).getReg();
	        const int64_t Imml = MI.getOperand(2).getImm();
	        const int64_t Imms = MI.getOperand(3).getImm();
	        if (Imms != 0) {
	          ++It;
	          continue;
	        }

	        unsigned NewOpc = 0;
	        if (Imml == 7)
	          NewOpc = LinxISA::C_ZEXT_B;
	        else if (Imml == 15)
	          NewOpc = LinxISA::C_ZEXT_H;
	        else if (Imml == 31)
	          NewOpc = LinxISA::C_ZEXT_W;
	        else {
	          ++It;
	          continue;
	        }

	        MachineInstr *NewMI =
	            BuildMI(MBB, It, MI.getDebugLoc(), TII.get(NewOpc), Dst)
	                .addReg(Src)
	                .getInstr();
	        MI.eraseFromParent();
	        It = std::next(NewMI->getIterator());
	        Changed = true;
	      }

	      // Post-remap peephole: compress common 32->64 sign-extends into C.SEXT.W
	      // when the destination is the T-hand implicit register.
	      //
	      // `addw src, zero, ->t` is a common legalization pattern for `sext.w`
	      // (truncate to 32 and sign-extend back to 64). Prefer the 16-bit
	      // encoding when block-private.
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &MI = *It;
	        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
	          ++It;
	          continue;
	        }
	        if (MI.getOpcode() != LinxISA::ADDWrr || MI.getNumOperands() < 3) {
	          ++It;
	          continue;
	        }
	        if (!MI.getOperand(0).isReg() || !MI.getOperand(0).isDef() ||
	            !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg()) {
	          ++It;
	          continue;
	        }
	        const Register Dst = MI.getOperand(0).getReg();
	        if (Dst != LinxISA::U4) {
	          ++It;
	          continue;
	        }
	        Register Src = Register();
	        const Register A = MI.getOperand(1).getReg();
	        const Register B = MI.getOperand(2).getReg();
	        if (A == LinxISA::R0 && B != LinxISA::R0)
	          Src = B;
	        else if (B == LinxISA::R0 && A != LinxISA::R0)
	          Src = A;
	        else {
	          ++It;
	          continue;
	        }

	        MachineInstr *NewMI =
	            BuildMI(MBB, It, MI.getDebugLoc(), TII.get(LinxISA::C_SEXT_W), Dst)
	                .addReg(Src)
	                .getInstr();
	        MI.eraseFromParent();
	        It = std::next(NewMI->getIterator());
	        Changed = true;
	      }

	      // Post-remap peephole: use 16-bit C.SEXT.* when sign-extending low bits
	      // into the T-hand implicit destination.
	      //
	      // The earlier shift-folding peephole may produce `BXS src, M=0, N=<width>`.
	      // When the result is block-private (`->t`), we can encode common widths
	      // with compressed sext forms.
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &MI = *It;
	        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
	          ++It;
	          continue;
	        }
	        if (MI.getOpcode() != LinxISA::BXS || MI.getNumOperands() < 4) {
	          ++It;
	          continue;
	        }
	        if (!MI.getOperand(0).isReg() || !MI.getOperand(0).isDef() ||
	            !MI.getOperand(1).isReg() || !MI.getOperand(2).isImm() ||
	            !MI.getOperand(3).isImm()) {
	          ++It;
	          continue;
	        }

	        const Register Dst = MI.getOperand(0).getReg();
	        if (Dst != LinxISA::U4) { // compressed form writes implicit `->t`
	          ++It;
	          continue;
	        }

	        const Register Src = MI.getOperand(1).getReg();
	        const int64_t Imml = MI.getOperand(2).getImm();
	        const int64_t Imms = MI.getOperand(3).getImm();
	        if (Imms != 0) {
	          ++It;
	          continue;
	        }

	        unsigned NewOpc = 0;
	        if (Imml == 7)
	          NewOpc = LinxISA::C_SEXT_B;
	        else if (Imml == 15)
	          NewOpc = LinxISA::C_SEXT_H;
	        else if (Imml == 31)
	          NewOpc = LinxISA::C_SEXT_W;
	        else {
	          ++It;
	          continue;
	        }

	        MachineInstr *NewMI =
	            BuildMI(MBB, It, MI.getDebugLoc(), TII.get(NewOpc), Dst)
	                .addReg(Src)
	                .getInstr();
	        MI.eraseFromParent();
	        It = std::next(NewMI->getIterator());
	        Changed = true;
	      }

	      // Insert `BSTOP` only for the final laid-out block. When a `BSTART.*`
	      // follows, it already terminates the previous block.
	      auto InsertBStop = MBB.end();
      while (InsertBStop != MBB.begin() && std::prev(InsertBStop)->isDebugInstr())
        --InsertBStop;

      if (MBB.getNextNode()) {
        if (InsertBStop != MBB.begin() &&
            std::prev(InsertBStop)->getOpcode() == LinxISA::BSTOP) {
          std::prev(InsertBStop)->eraseFromParent();
          Changed = true;
        }
      } else {
        if (InsertBStop == MBB.begin() ||
            std::prev(InsertBStop)->getOpcode() != LinxISA::BSTOP) {
          BuildMI(MBB, InsertBStop, DebugLoc(), TII.get(LinxISA::BSTOP));
          Changed = true;
        }
      }
    }

    // Enforce decoupled SIMT body contract: body-style headers must carry
    // a B.TEXT pointer before the next marker.
    for (MachineBasicBlock &MBB : MF) {
      for (auto It = MBB.begin(), E = MBB.end(); It != E; ++It) {
        if (!isSimtBodyHeaderOpcode(It->getOpcode()))
          continue;

        bool HasBodyPtr = false;
        auto J = std::next(It);
        for (; J != E; ++J) {
          if (J->isDebugInstr())
            continue;
          if (J->getOpcode() == LinxISA::B_TEXT) {
            HasBodyPtr = true;
            break;
          }
          if (isMarkerInstr(*J) || isFrameMacroInstr(*J))
            break;
          if (!isHeaderDescriptorOpcode(J->getOpcode()))
            break;
        }

        if (!HasBodyPtr) {
          report_fatal_error(
              "Linx: BSTART.{MPAR,MSEQ,VPAR,VSEQ} must include B.TEXT in the "
              "decoupled header");
        }
      }
    }

    // Jump tables take the address of their destination blocks. Ensure the
    // targets' labels are emitted even if they are fallthrough blocks with no
    // direct CFG edge pointing at them.
    if (const MachineJumpTableInfo *JTI = MF.getJumpTableInfo()) {
      for (const MachineJumpTableEntry &Entry : JTI->getJumpTables()) {
        for (MachineBasicBlock *JTBB : Entry.MBBs) {
          if (!JTBB || JTBB->hasLabelMustBeEmitted())
            continue;
          JTBB->setLabelMustBeEmitted();
          Changed = true;
        }
      }
    }

    return Changed;
  }
};

} // end anonymous namespace

char LinxISABlockify::ID = 0;

INITIALIZE_PASS(LinxISABlockify, "linx-blockify", "Linx Blockify", false,
                false)

FunctionPass *llvm::createLinxISABlockifyPass() { return new LinxISABlockify(); }
