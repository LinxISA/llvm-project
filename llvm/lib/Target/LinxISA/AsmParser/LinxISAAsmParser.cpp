//===-- LinxISAAsmParser.cpp - Parse Linx assembly to MCInsts -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "TargetInfo/LinxISATargetInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include <cctype>
#include <optional>
#include <string>

using namespace llvm;

namespace {

static std::string toUpperStr(StringRef S) {
  std::string Out;
  Out.reserve(S.size());
  for (char C : S)
    Out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(C))));
  return Out;
}

static bool isArchivedRawVectorOperandName(StringRef Name) {
  const std::string Upper = toUpperStr(Name.trim());
  return StringSwitch<bool>(Upper)
      .Cases({"TE", "TF", "TG", "TH"}, true)
      .Cases({"TO1", "TO2", "TO3"}, true)
      .Default(false);
}

static std::optional<unsigned> parseRegCode(StringRef Name) {
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

  // v0.4 vector/SIMT register namespaces (10-bit register codes).
  //
  // V.* instructions use 10-bit register fields (split across the 64-bit
  // 32x2 encoding). Scalar instructions use 5-bit register fields. The
  // assembler validates that register codes fit the field width during
  // encoding, so these extended names can be parsed unconditionally here.
  //
  // Encoding: (Class << 5) | Index
  //   Class 1:  ri0..ri31     (ordered argument namespace from B.IOR)
  //   Class 2:  p             (kernel EXEC mask; only index 28 is canonical)
  //   Class 3:  lc0..lc2      (hardware loop counters)
  //   Class 4:  vt / vt#n     (vector T-hand queue; index 0 is the push selector)
  //   Class 5:  vu / vu#n     (vector U-hand queue)
  //   Class 6:  vm / vm#n     (vector M-hand queue)
  //   Class 7:  vn / vn#n     (vector N-hand queue)
  //   Class 8:  ta/tb/tc/td/to/ts (vector tile bases)
  auto parsePrefixedIndex = [&](StringRef Prefix, unsigned Class,
                                unsigned MaxIndex) -> std::optional<unsigned> {
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
      Index = 0; // push selector
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
  if (Upper == "P")
    return (2u << 5) | 28u;
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

  return StringSwitch<std::optional<unsigned>>(Upper)
      .Case("ZERO", 0u)
      // Block-dimension destination selectors (C.B.DIM/C.B.DIMI).
      .Case("LB0", 0u)
      .Case("LB1", 1u)
      .Case("LB2", 2u)
      .Case("SP", 1u)
      .Case("A0", 2u)
      .Case("A1", 3u)
      .Case("A2", 4u)
      .Case("A3", 5u)
      .Case("A4", 6u)
      .Case("A5", 7u)
      .Case("A6", 8u)
      .Case("A7", 9u)
      .Case("RA", 10u)
      .Case("S0", 11u)
      .Case("S1", 12u)
      .Case("S2", 13u)
      .Case("S3", 14u)
      .Case("S4", 15u)
      .Case("S5", 16u)
      .Case("S6", 17u)
      .Case("S7", 18u)
      .Case("S8", 19u)
      .Case("X0", 20u)
      .Case("X1", 21u)
      .Case("X2", 22u)
      .Case("X3", 23u)
      .Case("T#1", 24u)
      .Case("T#2", 25u)
      .Case("T#3", 26u)
      .Case("T#4", 27u)
      .Case("U#1", 28u)
      .Case("U#2", 29u)
      .Case("U#3", 30u)
      .Case("U#4", 31u)
      // Queue aliases.
      .Case("U", 30u)
      .Case("T", 31u)
      .Default(std::nullopt);
}

static std::optional<unsigned> parseTileRef(StringRef Name, bool &Reuse) {
  Reuse = false;
  StringRef N = Name.trim();
  StringRef Base = N;
  StringRef Suffix;
  if (size_t Dot = N.find('.'); Dot != StringRef::npos) {
    Base = N.take_front(Dot);
    Suffix = N.drop_front(Dot + 1);
  }

  if (!Suffix.empty()) {
    std::string Up = toUpperStr(Suffix);
    if (Up == "REUSE") {
      Reuse = true;
    } else {
      return std::nullopt;
    }
  }

  if (Base.size() < 3)
    return std::nullopt;
  const char HandChar =
      static_cast<char>(std::tolower(static_cast<unsigned char>(Base[0])));
  if (Base[1] != '#')
    return std::nullopt;

  unsigned Hand = 0;
  switch (HandChar) {
  case 't':
    Hand = 0;
    break;
  case 'u':
    Hand = 1;
    break;
  case 'm':
    Hand = 2;
    break;
  case 'n':
    Hand = 3;
    break;
  default:
    return std::nullopt;
  }

  unsigned Depth = 0;
  StringRef Tail = Base.drop_front(2);
  if (Tail.getAsInteger(10, Depth) || Depth == 0 || Depth > 16)
    return std::nullopt;
  return (Hand << 4) | ((Depth - 1u) & 0xfu);
}

static std::optional<unsigned> parseDataTypeKeyword(StringRef Name) {
  const std::string Up = toUpperStr(Name.trim());
  return StringSwitch<std::optional<unsigned>>(Up)
      .Case("FP64", 0u)
      .Case("FP32", 1u)
      .Case("FP16", 2u)
      .Case("FP8", 3u)
      .Case("BF16", 6u)
      .Case("FPL8", 7u)
      .Case("FP4", 11u)
      .Case("FPL4", 12u)
      .Case("INT64", 16u)
      .Case("S64", 16u)
      .Case("INT32", 17u)
      .Case("S32", 17u)
      .Case("INT16", 18u)
      .Case("S16", 18u)
      .Case("INT8", 19u)
      .Case("S8", 19u)
      .Case("INT4", 20u)
      .Case("S4", 20u)
      .Case("UINT64", 24u)
      .Case("U64", 24u)
      .Case("UINT32", 25u)
      .Case("U32", 25u)
      .Case("UINT16", 26u)
      .Case("U16", 26u)
      .Case("UINT8", 27u)
      .Case("U8", 27u)
      .Case("UINT4", 28u)
      .Case("U4", 28u)
      .Default(std::nullopt);
}

static std::optional<unsigned> parseTMAFunctionKeyword(StringRef Name) {
  const std::string Up = toUpperStr(Name.trim());
  return StringSwitch<std::optional<unsigned>>(Up)
      .Case("TLOAD", 0u)
      .Case("TSTORE", 1u)
      .Case("TMOV", 2u)
      .Default(std::nullopt);
}

static std::optional<unsigned> parseCubeFunctionKeyword(StringRef Name) {
  const std::string Up = toUpperStr(Name.trim());
  return StringSwitch<std::optional<unsigned>>(Up)
      .Case("MAMULB", 0u)
      .Case("TMATMUL", 0u)
      .Case("MAMULB.ACC", 2u)
      .Case("TMATMUL.ACC", 2u)
      .Case("ACCCVT", 8u)
      .Default(std::nullopt);
}

static std::optional<unsigned> parseTEPLTileOpcodeKeyword(StringRef Name) {
  const std::string Up = toUpperStr(Name.trim());
  return StringSwitch<std::optional<unsigned>>(Up)
      // Mode 0: base TEPL function set.
      .Case("TADD", 0x000u)
      .Case("TSUB", 0x001u)
      .Case("TMUL", 0x002u)
      .Case("TDIV", 0x003u)
      .Case("TMAX", 0x004u)
      .Case("TMIN", 0x005u)
      .Case("TAND", 0x006u)
      .Case("TOR", 0x007u)
      .Case("TXOR", 0x008u)
      .Case("TSHL", 0x009u)
      .Case("TSHR", 0x00au)
      .Case("TRELU", 0x00bu)
      .Case("TPRELU", 0x00cu)
      .Case("TCVT", 0x00du)
      .Case("TEXP", 0x00eu)
      .Case("TLOG", 0x00fu)
      .Case("TSQRT", 0x010u)
      .Case("TRSQRT", 0x011u)
      .Case("TROWMAX", 0x012u)
      .Case("TROWMIN", 0x013u)
      .Case("TROWSUM", 0x014u)
      .Case("TCOLMAX", 0x015u)
      .Case("TCOLMIN", 0x016u)
      .Case("TCOLSUM", 0x017u)
      .Case("TRECIP", 0x018u)
      .Case("TEXPANDS", 0x019u)
      .Case("TGATHER", 0x01au)
      .Case("TSCATTER", 0x01bu)
      .Case("TRESHAPE", 0x01cu)
      .Case("TTRANSPOSE", 0x01du)
      .Case("TCOLEXPAND", 0x01eu)
      .Case("TROWEXPAND", 0x01fu)
      // Mode 1: scalar-RHS extension window.
      .Case("TADDS", 0x020u)
      .Case("TSUBS", 0x021u)
      .Case("TMULS", 0x022u)
      .Case("TDIVS", 0x023u)
      .Case("TMAXS", 0x024u)
      .Case("TMINS", 0x025u)
      .Case("TANDS", 0x026u)
      .Case("TORS", 0x027u)
      .Case("TXORS", 0x028u)
      .Case("TSHLS", 0x029u)
      .Case("TSHRS", 0x02au)
      .Default(std::nullopt);
}

struct TileBlockAlias {
  const char *CanonicalMnemonic;
  unsigned OpSel;
};

static std::optional<TileBlockAlias> parseTileBlockAliasMnemonic(StringRef Name) {
  const std::string Up = toUpperStr(Name.trim());
  return StringSwitch<std::optional<TileBlockAlias>>(Up)
      .Case("BSTART.TLOAD", TileBlockAlias{"BSTART.TMA", 0u})
      .Case("BSTART.TSTORE", TileBlockAlias{"BSTART.TMA", 1u})
      .Case("BSTART.TMOV", TileBlockAlias{"BSTART.TMA", 2u})
      .Case("BSTART.MAMULB", TileBlockAlias{"BSTART.CUBE", 0u})
      .Case("BSTART.TMATMUL", TileBlockAlias{"BSTART.CUBE", 0u})
      .Case("BSTART.MAMULB.ACC", TileBlockAlias{"BSTART.CUBE", 2u})
      .Case("BSTART.TMATMUL.ACC", TileBlockAlias{"BSTART.CUBE", 2u})
      .Case("BSTART.ACCCVT", TileBlockAlias{"BSTART.CUBE", 8u})
      .Case("BSTART.TADD", TileBlockAlias{"BSTART.TEPL", 0x000u})
      .Case("BSTART.TSUB", TileBlockAlias{"BSTART.TEPL", 0x001u})
      .Case("BSTART.TMUL", TileBlockAlias{"BSTART.TEPL", 0x002u})
      .Case("BSTART.TDIV", TileBlockAlias{"BSTART.TEPL", 0x003u})
      .Case("BSTART.TMAX", TileBlockAlias{"BSTART.TEPL", 0x004u})
      .Case("BSTART.TMIN", TileBlockAlias{"BSTART.TEPL", 0x005u})
      .Case("BSTART.TAND", TileBlockAlias{"BSTART.TEPL", 0x006u})
      .Case("BSTART.TOR", TileBlockAlias{"BSTART.TEPL", 0x007u})
      .Case("BSTART.TXOR", TileBlockAlias{"BSTART.TEPL", 0x008u})
      .Case("BSTART.TSHL", TileBlockAlias{"BSTART.TEPL", 0x009u})
      .Case("BSTART.TSHR", TileBlockAlias{"BSTART.TEPL", 0x00au})
      .Case("BSTART.TRELU", TileBlockAlias{"BSTART.TEPL", 0x00bu})
      .Case("BSTART.TPRELU", TileBlockAlias{"BSTART.TEPL", 0x00cu})
      .Case("BSTART.TCVT", TileBlockAlias{"BSTART.TEPL", 0x00du})
      .Case("BSTART.TEXP", TileBlockAlias{"BSTART.TEPL", 0x00eu})
      .Case("BSTART.TLOG", TileBlockAlias{"BSTART.TEPL", 0x00fu})
      .Case("BSTART.TSQRT", TileBlockAlias{"BSTART.TEPL", 0x010u})
      .Case("BSTART.TRSQRT", TileBlockAlias{"BSTART.TEPL", 0x011u})
      .Case("BSTART.TROWMAX", TileBlockAlias{"BSTART.TEPL", 0x012u})
      .Case("BSTART.TROWMIN", TileBlockAlias{"BSTART.TEPL", 0x013u})
      .Case("BSTART.TROWSUM", TileBlockAlias{"BSTART.TEPL", 0x014u})
      .Case("BSTART.TCOLMAX", TileBlockAlias{"BSTART.TEPL", 0x015u})
      .Case("BSTART.TCOLMIN", TileBlockAlias{"BSTART.TEPL", 0x016u})
      .Case("BSTART.TCOLSUM", TileBlockAlias{"BSTART.TEPL", 0x017u})
      .Case("BSTART.TRECIP", TileBlockAlias{"BSTART.TEPL", 0x018u})
      .Case("BSTART.TEXPANDS", TileBlockAlias{"BSTART.TEPL", 0x019u})
      .Case("BSTART.TGATHER", TileBlockAlias{"BSTART.TEPL", 0x01au})
      .Case("BSTART.TSCATTER", TileBlockAlias{"BSTART.TEPL", 0x01bu})
      .Case("BSTART.TRESHAPE", TileBlockAlias{"BSTART.TEPL", 0x01cu})
      .Case("BSTART.TTRANSPOSE", TileBlockAlias{"BSTART.TEPL", 0x01du})
      .Case("BSTART.TCOLEXPAND", TileBlockAlias{"BSTART.TEPL", 0x01eu})
      .Case("BSTART.TROWEXPAND", TileBlockAlias{"BSTART.TEPL", 0x01fu})
      .Case("BSTART.TADDS", TileBlockAlias{"BSTART.TEPL", 0x020u})
      .Case("BSTART.TSUBS", TileBlockAlias{"BSTART.TEPL", 0x021u})
      .Case("BSTART.TMULS", TileBlockAlias{"BSTART.TEPL", 0x022u})
      .Case("BSTART.TDIVS", TileBlockAlias{"BSTART.TEPL", 0x023u})
      .Case("BSTART.TMAXS", TileBlockAlias{"BSTART.TEPL", 0x024u})
      .Case("BSTART.TMINS", TileBlockAlias{"BSTART.TEPL", 0x025u})
      .Case("BSTART.TANDS", TileBlockAlias{"BSTART.TEPL", 0x026u})
      .Case("BSTART.TORS", TileBlockAlias{"BSTART.TEPL", 0x027u})
      .Case("BSTART.TXORS", TileBlockAlias{"BSTART.TEPL", 0x028u})
      .Case("BSTART.TSHLS", TileBlockAlias{"BSTART.TEPL", 0x029u})
      .Case("BSTART.TSHRS", TileBlockAlias{"BSTART.TEPL", 0x02au})
      .Default(std::nullopt);
}

static std::optional<uint32_t> parseSSRIdName(StringRef Name) {
  std::string Up = toUpperStr(Name.trim());

  // Normalize common suffixes: *_ACRn, *_ACR<n>.
  StringRef UpRef(Up);
  if (UpRef.ends_with("_ACRN"))
    UpRef = UpRef.drop_back(StringRef("_ACRN").size());
  if (size_t Pos = UpRef.rfind("_ACR"); Pos != StringRef::npos) {
    StringRef Tail = UpRef.drop_front(Pos + 4);
    bool AllDigits = !Tail.empty() && Tail.size() <= 2;
    for (char C : Tail)
      if (!std::isdigit((unsigned char)C))
        AllDigits = false;
    if (AllDigits) {
      UpRef = UpRef.take_front(Pos);
    }
  }

  // EBARG group (v0.2): ACR-scoped, 0xF40+ (low 12 bits).
  if (UpRef == "EBARG0")
    return 0x0f40u;
  if (UpRef == "EBARG_BPC_CUR")
    return 0x0f41u;
  if (UpRef == "EBARG_BPC_TGT")
    return 0x0f42u;
  if (UpRef == "EBARG_TPC")
    return 0x0f43u;
  if (UpRef == "EBARG_LRA")
    return 0x0f44u;
  if (UpRef == "EBARG_TQ0")
    return 0x0f45u;
  if (UpRef == "EBARG_TQ1")
    return 0x0f46u;
  if (UpRef == "EBARG_TQ2")
    return 0x0f47u;
  if (UpRef == "EBARG_TQ3")
    return 0x0f48u;
  if (UpRef == "EBARG_UQ0")
    return 0x0f49u;
  if (UpRef == "EBARG_UQ1")
    return 0x0f4au;
  if (UpRef == "EBARG_UQ2")
    return 0x0f4bu;
  if (UpRef == "EBARG_UQ3")
    return 0x0f4cu;
  if (UpRef == "EBARG_LB")
    return 0x0f4du;
  if (UpRef == "EBARG_LC")
    return 0x0f4eu;
  if (UpRef == "EBARG_EXTCTX_PTR")
    return 0x0f4fu;
  if (UpRef == "EBARG_EXTCTX_META")
    return 0x0f50u;

  // Debug SSRs (v0.2 bring-up subset): ACR-scoped, 0xF80+.
  if (UpRef == "DBGID")
    return 0x0f80u;

  auto parseIndexed = [&](StringRef Prefix, uint32_t Base, uint32_t Stride,
                          unsigned MaxN) -> std::optional<uint32_t> {
    if (!UpRef.starts_with(Prefix))
      return std::nullopt;
    StringRef Tail = UpRef.drop_front(Prefix.size());
    unsigned N = 0;
    if (Tail.empty() || Tail.getAsInteger(10, N) || N >= MaxN)
      return std::nullopt;
    return Base + Stride * N;
  };

  if (auto V = parseIndexed("DBCR", 0x0f90u, 2u, 4))
    return *V;
  if (auto V = parseIndexed("DBVR", 0x0f91u, 2u, 4))
    return *V;
  if (auto V = parseIndexed("DWCR", 0x0fb0u, 2u, 4))
    return *V;
  if (auto V = parseIndexed("DWVR", 0x0fb1u, 2u, 4))
    return *V;
  if (UpRef == "DCCR0")
    return 0x0fa0u;
  if (UpRef == "DCVR0")
    return 0x0fa1u;

  return StringSwitch<std::optional<uint32_t>>(UpRef)
      .Case("TP", 0x0000u)
      .Case("GP", 0x0001u)
      .Case("TIME", 0x0010u)
      .Case("CYCLE", 0x0c00u)
      .Case("CSTATE", 0x0020u)
      .Case("LXLCID", 0x0021u)
      .Case("VENDOR", 0x0022u)
      .Case("VERSION", 0x0023u)
      .Case("LCFR", 0x0024u)
      .Case("LCFR_EN", 0x0025u)
      .Case("TR1", 0x0800u)
      .Case("TR2", 0x0801u)
      .Case("SYSCNT", 0x0810u)
      .Case("CW", 0x0820u)
      .Case("MSGBCR", 0x0830u)
      .Case("MSGBD1", 0x0831u)
      .Case("MSGBD2", 0x0832u)
      .Case("MSGBD3", 0x0833u)
      .Case("MSGBD4", 0x0834u)
      .Case("MSGBD5", 0x0835u)
      .Case("MSGBD6", 0x0836u)
      .Case("MSGBD7", 0x0837u)
      .Case("MSGBD8", 0x0838u)
      .Case("MSGBD9", 0x0839u)
      .Case("MSGBD10", 0x083au)
      // ACR-scoped (privileged) SSR families from isa.txt.
      //
      // NOTE: Base SSR access instructions encode only SSR_ID[11:0]. These
      // names map to the low-12-bit IDs (0xFxx) and may be interpreted within a
      // privileged ACR namespace.
      .Case("ECSTATE", 0x0f00u)
      .Case("ECSTATE_ACRN", 0x0f00u)
      .Case("EVBASE", 0x0f01u)
      .Case("EVBASE_ACRN", 0x0f01u)
      .Case("TRAPNO", 0x0f02u)
      .Case("TRAPNO_ACRN", 0x0f02u)
      .Case("TRAPARG0", 0x0f03u)
      .Case("TRAPARG0_ACRN", 0x0f03u)
      .Case("ETEMP", 0x0f05u)
      .Case("ETEMP_ACRN", 0x0f05u)
      .Case("ETEMP0", 0x0f06u)
      .Case("ETEMP0_ACRN", 0x0f06u)
      .Case("ECONFIG", 0x0f07u)
      .Case("ECONFIG_ACRN", 0x0f07u)
      .Case("IPENDING", 0x0f08u)
      .Case("IPENDING_ACRN", 0x0f08u)
      .Case("TOPEI", 0x0f09u)
      .Case("TOPEI_ACRN", 0x0f09u)
      .Case("EOIEI", 0x0f0au)
      .Case("EOIEI_ACRN", 0x0f0au)
      // MMU/IOMMU (ACR1) bring-up profile.
      .Case("TTBR0", 0x0f10u)
      .Case("TTBR0_ACR1", 0x0f10u)
      .Case("TTBR1", 0x0f11u)
      .Case("TTBR1_ACR1", 0x0f11u)
      .Case("TCR", 0x0f12u)
      .Case("TCR_ACR1", 0x0f12u)
      .Case("MAIR", 0x0f13u)
      .Case("MAIR_ACR1", 0x0f13u)
      .Case("IOTTBR", 0x0f14u)
      .Case("IOTTBR_ACR1", 0x0f14u)
      .Case("IOTCR", 0x0f15u)
      .Case("IOTCR_ACR1", 0x0f15u)
      .Case("IOMAIR", 0x0f16u)
      .Case("IOMAIR_ACR1", 0x0f16u)
      .Case("TIMER_TIME", 0x0f20u)
      .Case("TIMER_TIME_ACRN", 0x0f20u)
      .Case("TIMER_TIMECMP", 0x0f21u)
      .Case("TIMER_TIMECMP_ACRN", 0x0f21u)
      .Case("XBINFO", 0x0f30u)
      .Case("XBINFO_ACRN", 0x0f30u)
      .Case("ACR_PARAM", 0x0f31u)
      .Case("ACR_PARAM_ACRN", 0x0f31u)
      .Default(std::nullopt);
}

static MCRegister regCodeToMCReg(unsigned Code) {
  static const MCRegister Regs[32] = {
      LinxISA::R0,  LinxISA::R1,  LinxISA::R2,  LinxISA::R3,
      LinxISA::R4,  LinxISA::R5,  LinxISA::R6,  LinxISA::R7,
      LinxISA::R8,  LinxISA::R9,  LinxISA::R10, LinxISA::R11,
      LinxISA::R12, LinxISA::R13, LinxISA::R14, LinxISA::R15,
      LinxISA::R16, LinxISA::R17, LinxISA::R18, LinxISA::R19,
      LinxISA::R20, LinxISA::R21, LinxISA::R22, LinxISA::R23,
      LinxISA::T1,  LinxISA::T2,  LinxISA::T3,  LinxISA::T4,
      LinxISA::U1,  LinxISA::U2,  LinxISA::U3,  LinxISA::U4,
  };
  if (Code < 32)
    return Regs[Code];
  return MCRegister();
}

static bool isConstExpr(const MCExpr *E, int64_t &Out) {
  if (auto *CE = dyn_cast<MCConstantExpr>(E)) {
    Out = CE->getValue();
    return true;
  }
  return false;
}

static std::optional<unsigned> parseSrcRTypeSuffix(StringRef Suffix) {
  std::string Up = toUpperStr(Suffix);
  if (Up == "SW")
    return 0u;
  if (Up == "UW")
    return 1u;
  if (Up == "NEG" || Up == "NOT")
    return 2u;
  return std::nullopt;
}

static std::optional<unsigned> parseBrType(StringRef Tok) {
  std::string Up = toUpperStr(Tok);
  return StringSwitch<std::optional<unsigned>>(Up)
      .Case("FALL", 1u)
      .Case("DIRECT", 2u)
      .Case("COND", 3u)
      .Case("CALL", 4u)
      .Case("IND", 5u)
      .Case("ICALL", 6u)
      .Case("RET", 7u)
      .Default(std::nullopt);
}

static std::optional<unsigned> parseFenceImmKeyword(StringRef Tok) {
  std::string Up = toUpperStr(Tok.trim());
  unsigned Mask = 0;

  for (char C : Up) {
    switch (C) {
    case 'I':
      Mask |= 0x8u;
      break;
    case 'O':
      Mask |= 0x4u;
      break;
    case 'R':
      Mask |= 0x2u;
      break;
    case 'W':
      Mask |= 0x1u;
      break;
    default:
      return std::nullopt;
    }
  }

  return Mask;
}

static std::optional<std::string> getLegacyAliasDiag(StringRef Mnemonic) {
  const std::string Up = toUpperStr(Mnemonic);
  const StringRef Key(Up);

  if (Key == "L.BSTOP")
    return "legacy alias 'L.BSTOP' is not allowed in canonical v0.56; use 'C.BSTOP'";

  const bool IsCanonicalLongBStart =
      Key == "L.BSTART.STD" || Key == "L.BSTART.FP" ||
      Key == "L.BSTART.SYS";
  if (Key.starts_with("L.") && !IsCanonicalLongBStart)
    return "legacy 'L.*' mnemonics are not allowed in canonical v0.56; use canonical "
           "mnemonics (for example 'V.*' and typed BSTART.* forms)";

  return std::nullopt;
}

enum class BStartKind {
  Unknown,
  Fall,
  Direct,
  Cond,
  Call,
  Ind,
  ICall,
  Ret,
  DirectOrCall, // "BSTART {DIRECT, CALL}"
};

static BStartKind brTypeToBStartKind(unsigned BrType) {
  switch (BrType & 0x7u) {
  case 1:
    return BStartKind::Fall;
  case 2:
    return BStartKind::Direct;
  case 3:
    return BStartKind::Cond;
  case 4:
    return BStartKind::Call;
  case 5:
    return BStartKind::Ind;
  case 6:
    return BStartKind::ICall;
  case 7:
    return BStartKind::Ret;
  default:
    return BStartKind::Unknown;
  }
}

static BStartKind bstartKindFromAsmFmt(StringRef AsmFmt) {
  // Order matters: ICALL contains "CALL" as a substring.
  if (AsmFmt.contains_insensitive("{DIRECT, CALL}"))
    return BStartKind::DirectOrCall;
  if (AsmFmt.contains_insensitive(" ICALL"))
    return BStartKind::ICall;
  if (AsmFmt.contains_insensitive(" IND"))
    return BStartKind::Ind;
  if (AsmFmt.contains_insensitive(" RET"))
    return BStartKind::Ret;
  if (AsmFmt.contains_insensitive(" COND"))
    return BStartKind::Cond;
  if (AsmFmt.contains_insensitive(" DIRECT"))
    return BStartKind::Direct;
  if (AsmFmt.contains_insensitive(" CALL"))
    return BStartKind::Call;
  if (AsmFmt.contains_insensitive(" FALL"))
    return BStartKind::Fall;
  return BStartKind::Unknown;
}

static std::optional<unsigned> parseBlockTypeFromMnemonic(StringRef Mnemonic) {
  std::string Up = toUpperStr(Mnemonic);
  size_t Dot = Up.find('.');
  if (Dot == std::string::npos)
    return std::nullopt;
  std::string Suffix = Up.substr(Dot + 1);
  if (Suffix == "STD")
    return 0u;
  if (Suffix == "SYS")
    return 1u;
  if (Suffix == "FP")
    return 2u;
  return std::nullopt;
}

static int64_t memScaleFromMnemonic(StringRef Mnemonic) {
  std::string UpStr = toUpperStr(Mnemonic);
  StringRef Up(UpStr);

  if (Up.ends_with(".LOCAL"))
    Up = Up.drop_back(StringRef(".LOCAL").size());

  // Unscaled byte offsets.
  if (Up.ends_with(".U") || Up.ends_with(".UPO") || Up.ends_with(".UPR"))
    return 1;

  // Strip common prefixes and suffixes so we can classify by the base opcode.
  // Examples:
  //   HL.LWI.PO   -> LWI
  //   HL.LWIP.U   -> LWIP
  //   C.SWI       -> SWI
  //   LDI.U       -> LDI (handled above as unscaled)
  if (Up.starts_with("HL."))
    Up = Up.drop_front(3);
  if (Up.starts_with("C."))
    Up = Up.drop_front(2);
  StringRef Op = Up.split('.').first;

  // Byte.
  if (Op == "LBI" || Op == "LBUI" || Op == "SBI" || Op == "LBIP" ||
      Op == "LBUIP" || Op == "SBIP")
    return 1;
  // Halfword.
  if (Op == "LHI" || Op == "LHUI" || Op == "SHI" || Op == "LHIP" ||
      Op == "LHUIP" || Op == "SHIP")
    return 2;
  // Word.
  if (Op == "LWI" || Op == "LWUI" || Op == "SWI" || Op == "LWIP" ||
      Op == "LWUIP" || Op == "SWIP")
    return 4;
  // Doubleword.
  if (Op == "LDI" || Op == "SDI" || Op == "LDIP" || Op == "SDIP")
    return 8;

  return 1;
}

static std::optional<unsigned> parseScalarFpTypeSuffix(StringRef Suffix) {
  if (Suffix.equals_insensitive("fd"))
    return 0u;
  if (Suffix.equals_insensitive("fs"))
    return 1u;
  if (Suffix.equals_insensitive("fh"))
    return 2u;
  if (Suffix.equals_insensitive("fb"))
    return 3u;
  return std::nullopt;
}

static std::optional<unsigned> parseScalarIntDstTypeSuffix(StringRef Suffix) {
  if (Suffix.equals_insensitive("ud"))
    return 0u;
  if (Suffix.equals_insensitive("uw"))
    return 1u;
  if (Suffix.equals_insensitive("uh"))
    return 2u;
  if (Suffix.equals_insensitive("ub"))
    return 3u;
  if (Suffix.equals_insensitive("sd"))
    return 8u;
  if (Suffix.equals_insensitive("sw"))
    return 9u;
  if (Suffix.equals_insensitive("sh"))
    return 10u;
  if (Suffix.equals_insensitive("sb"))
    return 11u;
  return std::nullopt;
}

static std::optional<unsigned> parseScalarIntSrcTypeSuffix(StringRef Suffix,
                                                           bool Signed) {
  if ((Signed && Suffix.equals_insensitive("sd")) ||
      (!Signed && Suffix.equals_insensitive("ud")))
    return 0u;
  if ((Signed && Suffix.equals_insensitive("sw")) ||
      (!Signed && Suffix.equals_insensitive("uw")))
    return 1u;
  if ((Signed && Suffix.equals_insensitive("sh")) ||
      (!Signed && Suffix.equals_insensitive("uh")))
    return 2u;
  if ((Signed && Suffix.equals_insensitive("sb")) ||
      (!Signed && Suffix.equals_insensitive("ub")))
    return 3u;
  return std::nullopt;
}

static std::optional<unsigned> parsePrefetchModelSuffix(StringRef Suffix) {
  if (Suffix.equals_insensitive("l1"))
    return 0u;
  if (Suffix.equals_insensitive("l2"))
    return 1u;
  if (Suffix.equals_insensitive("l3"))
    return 2u;
  if (Suffix.size() > 1 && (Suffix.front() == 'm' || Suffix.front() == 'M')) {
    unsigned Value = 0;
    if (!Suffix.drop_front().getAsInteger(10, Value))
      return Value;
  }
  return std::nullopt;
}

static bool parseQueueFlagSuffix(StringRef Suffix, bool AllowH, unsigned &E,
                                 unsigned &H, unsigned &I, unsigned &R,
                                 unsigned &S) {
  E = 0;
  H = 0;
  I = 0;
  R = 0;
  S = 0;
  while (!Suffix.empty()) {
    if (Suffix.consume_front_insensitive("ier")) {
      I = 1;
      E = 1;
      R = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("ies")) {
      I = 1;
      E = 1;
      S = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("ie")) {
      I = 1;
      E = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("is")) {
      I = 1;
      S = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("ir")) {
      I = 1;
      R = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("es")) {
      E = 1;
      S = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("er")) {
      E = 1;
      R = 1;
      continue;
    }
    if (AllowH && Suffix.consume_front_insensitive("her")) {
      H = 1;
      E = 1;
      R = 1;
      continue;
    }
    if (AllowH && Suffix.consume_front_insensitive("he")) {
      H = 1;
      E = 1;
      continue;
    }
    if (AllowH && Suffix.consume_front_insensitive("hr")) {
      H = 1;
      R = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("i")) {
      I = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("e")) {
      E = 1;
      continue;
    }
    if (AllowH && Suffix.consume_front_insensitive("h")) {
      H = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("r")) {
      R = 1;
      continue;
    }
    if (Suffix.consume_front_insensitive("s")) {
      S = 1;
      continue;
    }
    return false;
  }
  return true;
}

static std::optional<unsigned> parseVectorBlockModeKeyword(StringRef Name) {
  if (Name.equals_insensitive("VS8"))
    return 0u;
  if (Name.equals_insensitive("VS16"))
    return 1u;
  return std::nullopt;
}

static std::optional<unsigned> parsePadValueKeyword(StringRef Name) {
  if (Name.equals_insensitive("Null"))
    return 0u;
  if (Name.equals_insensitive("Zero"))
    return 1u;
  if (Name.equals_insensitive("Max"))
    return 2u;
  if (Name.equals_insensitive("Min"))
    return 3u;
  return std::nullopt;
}

static bool decodeAtomicMnemonicSuffix(StringRef Suffix, unsigned &Aq,
                                       unsigned &Far, unsigned &Rl,
                                       unsigned &Rd) {
  if (Suffix.equals_insensitive("aq")) {
    Aq = 1;
    return true;
  }
  if (Suffix.equals_insensitive("rl")) {
    Rl = 1;
    return true;
  }
  if (Suffix.equals_insensitive("f")) {
    Far = 1;
    return true;
  }
  if (Suffix.equals_insensitive("rd")) {
    Rd = 1;
    return true;
  }
  if (Suffix.equals_insensitive("aqrl")) {
    Aq = 1;
    Rl = 1;
    return true;
  }
  if (Suffix.equals_insensitive("aqf")) {
    Aq = 1;
    Far = 1;
    return true;
  }
  if (Suffix.equals_insensitive("rlf")) {
    Rl = 1;
    Far = 1;
    return true;
  }
  if (Suffix.equals_insensitive("aqrlf")) {
    Aq = 1;
    Rl = 1;
    Far = 1;
    return true;
  }
  if (Suffix.equals_insensitive("rdf")) {
    Rd = 1;
    Far = 1;
    return true;
  }
  if (Suffix.equals_insensitive("rlrd")) {
    Rl = 1;
    Rd = 1;
    return true;
  }
  if (Suffix.equals_insensitive("rlrdf")) {
    Rl = 1;
    Rd = 1;
    Far = 1;
    return true;
  }
  return false;
}

struct ParsedReg {
  unsigned Code = 0;
  unsigned SrcRType = 3; // default: no modifier
  unsigned Shamt = 0;
  bool HasExplicitType = false;
  bool HasExplicitShift = false;
  bool HasAngleSize = false;
  unsigned AngleSize = 0;
  bool HasAngleReg = false;
  unsigned AngleReg = 0;
  SMLoc Loc;
};

struct ParsedMem {
  ParsedReg Base;
  bool HasLane = false;
  ParsedReg Lane;
  bool HasIndex = false;
  ParsedReg Index;
  const MCExpr *OffExpr = nullptr;
  SMLoc StartLoc;
  SMLoc EndLoc;
};

struct ParsedImm {
  const MCExpr *Expr = nullptr;
  SMLoc Loc;
};

struct ParsedKeyword {
  std::string TextUpper;
  SMLoc Loc;
};

struct ParsedInst {
  SmallVector<ParsedReg, 8> Regs;
  SmallVector<ParsedImm, 4> Imms;
  SmallVector<ParsedKeyword, 2> Keywords;
  std::optional<ParsedMem> Mem;
  SmallVector<ParsedReg, 2> ArrowDests;
  std::optional<ParsedImm> SetRetTarget;
  unsigned LocalBit = 0;
  unsigned AtomicAq = 0;
  unsigned AtomicFar = 0;
  unsigned AtomicRl = 0;
  unsigned AtomicRd = 0;
};

class LinxOperand : public MCParsedAsmOperand {
public:
  enum Kind { Token, Reg, Imm, Mem, Keyword, ArrowDest, SetRetTarget };

private:
  Kind K;
  SMLoc StartLoc;
  SMLoc EndLoc;

  std::string Tok;
  ParsedReg R;
  const MCExpr *E = nullptr;
  ParsedMem M;
  ParsedKeyword KW;

public:
  explicit LinxOperand(Kind K, SMLoc Start, SMLoc End)
      : K(K), StartLoc(Start), EndLoc(End) {}

  static std::unique_ptr<LinxOperand> createToken(StringRef Tok, SMLoc Loc) {
    auto Op = std::make_unique<LinxOperand>(Token, Loc, Loc);
    Op->Tok = Tok.str();
    return Op;
  }

  static std::unique_ptr<LinxOperand> createReg(const ParsedReg &R, SMLoc End) {
    auto Op = std::make_unique<LinxOperand>(Reg, R.Loc, End);
    Op->R = R;
    return Op;
  }

  static std::unique_ptr<LinxOperand> createImm(const MCExpr *E, SMLoc Loc,
                                                SMLoc End) {
    auto Op = std::make_unique<LinxOperand>(Imm, Loc, End);
    Op->E = E;
    return Op;
  }

  static std::unique_ptr<LinxOperand> createSetRetTarget(const MCExpr *E,
                                                         SMLoc Loc,
                                                         SMLoc End) {
    auto Op = std::make_unique<LinxOperand>(SetRetTarget, Loc, End);
    Op->E = E;
    return Op;
  }

  static std::unique_ptr<LinxOperand> createMem(const ParsedMem &M) {
    auto Op = std::make_unique<LinxOperand>(Mem, M.StartLoc, M.EndLoc);
    Op->M = M;
    return Op;
  }

  static std::unique_ptr<LinxOperand> createKeyword(StringRef Text, SMLoc Loc,
                                                    SMLoc End) {
    auto Op = std::make_unique<LinxOperand>(Keyword, Loc, End);
    Op->KW.TextUpper = toUpperStr(Text);
    Op->KW.Loc = Loc;
    return Op;
  }

  static std::unique_ptr<LinxOperand> createArrowDest(const ParsedReg &D,
                                                      SMLoc End) {
    auto Op = std::make_unique<LinxOperand>(ArrowDest, D.Loc, End);
    Op->R = D;
    return Op;
  }

  bool isToken() const override { return K == Token || K == Keyword; }
  bool isImm() const override { return K == Imm; }
  bool isReg() const override { return K == Reg || K == ArrowDest; }
  MCRegister getReg() const override {
    if (!isReg())
      return MCRegister();
    return regCodeToMCReg(R.Code);
  }
  bool isMem() const override { return K == Mem; }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (K) {
    case Token:
      OS << "Tok(" << Tok << ")";
      break;
    case Keyword:
      OS << "Kw(" << KW.TextUpper << ")";
      break;
    case Reg:
      OS << "Reg(" << R.Code << ")";
      break;
    case ArrowDest:
      OS << "ArrowDest(" << R.Code << ")";
      break;
    case Imm:
    case SetRetTarget:
      OS << "Imm(";
      if (E)
        MAI.printExpr(OS, *E);
      OS << ")";
      break;
    case Mem:
      OS << "Mem";
      break;
    }
  }

  Kind getKind() const { return K; }
  StringRef getToken() const { return Tok; }
  const ParsedReg &getParsedReg() const { return R; }
  const MCExpr *getExpr() const { return E; }
  const ParsedMem &getMem() const { return M; }
  const ParsedKeyword &getKeyword() const { return KW; }
};

class LinxISAAsmParser : public MCTargetAsmParser {
public:
  LinxISAAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                   const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII) {
    MCAsmParserExtension::Initialize(Parser);
    Parser.addAliasForDirective(".half", ".2byte");
    Parser.addAliasForDirective(".hword", ".2byte");
    Parser.addAliasForDirective(".word", ".4byte");
    Parser.addAliasForDirective(".dword", ".8byte");
  }

  void Initialize(MCAsmParser &Parser) override {
    MCTargetAsmParser::Initialize(Parser);
    // Allow tokens like `t#1`/`u#2` to be lexed as identifiers.
    getLexer().setAllowHashInIdentifier(true);
  }

  bool parsePrimaryExpr(const MCExpr *&Res, SMLoc &EndLoc) override;

  ParseStatus parseDirective(AsmToken DirectiveID) override;

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                     SMLoc &EndLoc) override;

  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;
  void convertToMapAndConstraints(unsigned Kind,
                                  const OperandVector &Operands) override {}

private:
  bool parseRegOperand(ParsedReg &Out);
  bool parseImmOperand(const MCExpr *&OutExpr, SMLoc &Start, SMLoc &End);
  bool parseMemOperand(ParsedMem &OutMem);
  bool parseArrowDestOperand(ParsedReg &OutDest);

  bool buildParsedInst(OperandVector &Operands, ParsedInst &Out);
  bool buildMCInstForForm(unsigned FormIndex, const ParsedInst &PI,
                          MCInst &OutInst, std::string &Err);
};

static const StringMap<SmallVector<unsigned, 4>> &getMnemonicMap() {
  static StringMap<SmallVector<unsigned, 4>> Map;
  if (!Map.empty())
    return Map;

  auto addKey = [&](StringRef Key, unsigned Index) {
    std::string Up = toUpperStr(Key);
    Map[Up].push_back(Index);
  };

  for (unsigned i = 0; i < linxisa_inst_forms_count; ++i) {
    const linxisa_inst_form &F = linxisa_inst_forms[i];
    if (F.mnemonic && F.mnemonic[0])
      addKey(F.mnemonic, i);

    if (F.asm_fmt && F.asm_fmt[0]) {
      StringRef Fmt(F.asm_fmt);
      StringRef First = Fmt.split(' ').first;
      First = First.rtrim(",");
      if (!First.empty() && First[0] != '#')
        addKey(First, i);
    }

    // Aliases for readability: treat `*.STD` as the default `*`.
    if (F.mnemonic) {
      StringRef M(F.mnemonic);
      if (M.starts_with("BSTART.STD"))
        addKey("BSTART", i);
      if (M.starts_with("C.BSTART.STD"))
        addKey("C.BSTART", i);
    }
  }

  return Map;
}

ParseStatus LinxISAAsmParser::tryParseRegister(MCRegister &Reg,
                                               SMLoc &StartLoc,
                                               SMLoc &EndLoc) {
  if (!getTok().is(AsmToken::Identifier))
    return ParseStatus::NoMatch;

  StringRef Tok = getTok().getString();
  auto Code = parseRegCode(Tok);
  if (!Code)
    return ParseStatus::NoMatch;

  StartLoc = getTok().getLoc();
  EndLoc = getTok().getEndLoc();
  Reg = regCodeToMCReg(*Code);
  Lex();
  return ParseStatus::Success;
}

bool LinxISAAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                     SMLoc &EndLoc) {
  ParseStatus S = tryParseRegister(Reg, StartLoc, EndLoc);
  if (S.isSuccess())
    return false;
  if (getTok().is(AsmToken::Identifier) &&
      isArchivedRawVectorOperandName(getTok().getString())) {
    return Error(getTok().getLoc(),
                 "archived raw vector operand name is not allowed in canonical "
                 "v0.4; use TA/TB/TC/TD/TO/TS");
  }
  return Error(getTok().getLoc(), "expected register");
}

bool LinxISAAsmParser::parseImmOperand(const MCExpr *&OutExpr, SMLoc &Start,
                                       SMLoc &End) {
  Start = getTok().getLoc();
  if (getTok().is(AsmToken::Percent)) {
    Lex();
    if (!getTok().is(AsmToken::Identifier))
      return Error(getTok().getLoc(), "expected relocation name after '%'");

    StringRef Reloc = getTok().getString();
    const bool IsTPCRel =
        Reloc.equals_insensitive("tpcrel_hi") ||
        Reloc.equals_insensitive("tpcrel_lo");
    if (!IsTPCRel)
      return Error(getTok().getLoc(), "unsupported '%' relocation syntax");
    Lex();

    if (parseToken(AsmToken::LParen, "expected '(' after relocation name"))
      return true;
    if (getParser().parseExpression(OutExpr, End))
      return true;
    if (parseToken(AsmToken::RParen, "expected ')' after relocation expression"))
      return true;
    return false;
  }
  if (getParser().parseExpression(OutExpr, End))
    return true;
  return false;
}

bool LinxISAAsmParser::parsePrimaryExpr(const MCExpr *&Res, SMLoc &EndLoc) {
  if (getTok().is(AsmToken::Percent)) {
    Lex();
    if (!getTok().is(AsmToken::Identifier))
      return Error(getTok().getLoc(), "expected relocation name after '%'");

    StringRef Reloc = getTok().getString();
    const bool IsTPCRel =
        Reloc.equals_insensitive("tpcrel_hi") ||
        Reloc.equals_insensitive("tpcrel_lo");
    if (!IsTPCRel)
      return Error(getTok().getLoc(), "unsupported '%' relocation syntax");
    Lex();

    if (parseToken(AsmToken::LParen, "expected '(' after relocation name"))
      return true;
    if (getParser().parseExpression(Res, EndLoc))
      return true;
    if (parseToken(AsmToken::RParen, "expected ')' after relocation expression"))
      return true;
    return false;
  }

  return MCTargetAsmParser::parsePrimaryExpr(Res, EndLoc);
}

ParseStatus LinxISAAsmParser::parseDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getString();
  if (IDVal == ".option") {
    MCAsmParser &Parser = getParser();
    AsmToken Tok = Parser.getTok();
    if (parseToken(AsmToken::Identifier, "expected identifier"))
      return ParseStatus::Failure;
    StringRef Option = Tok.getIdentifier();
    if (Option == "push" || Option == "pop" || Option == "norelax") {
      if (Parser.parseEOL())
        return ParseStatus::Failure;
      return ParseStatus::Success;
    }
  }
  return ParseStatus::NoMatch;
}

bool LinxISAAsmParser::parseRegOperand(ParsedReg &Out) {
  if (!getTok().is(AsmToken::Identifier))
    return Error(getTok().getLoc(), "expected register");

  StringRef Tok = getTok().getString();
  StringRef Base = Tok;
  StringRef Suffix;
  if (size_t Dot = Tok.rfind('.'); Dot != StringRef::npos) {
    Base = Tok.take_front(Dot);
    Suffix = Tok.drop_front(Dot + 1);
  }
  if (Base.ends_with_insensitive(".reuse"))
    Base = Base.drop_back(strlen(".reuse"));
  if (isArchivedRawVectorOperandName(Base))
    return Error(getTok().getLoc(),
                 "archived raw vector operand name is not allowed in canonical "
                 "v0.4; use TA/TB/TC/TD/TO/TS");

  auto Code = parseRegCode(Base);
  if (!Code)
    return Error(getTok().getLoc(), "invalid register name");

  Out = ParsedReg();
  Out.Code = *Code;
  Out.Loc = getTok().getLoc();

  if (!Suffix.empty()) {
    if (auto T = parseSrcRTypeSuffix(Suffix)) {
      Out.SrcRType = *T;
      Out.HasExplicitType = true;
    }
  }

  Lex(); // consume reg token

  // Optional shift: `<< N`.
  if (getTok().is(AsmToken::LessLess)) {
    Lex();
    const MCExpr *ShiftExpr = nullptr;
    SMLoc ShiftLoc, ShiftEnd;
    if (parseImmOperand(ShiftExpr, ShiftLoc, ShiftEnd))
      return true;
    int64_t ShiftVal = 0;
    if (!isConstExpr(ShiftExpr, ShiftVal) || ShiftVal < 0 || ShiftVal > 63)
      return Error(ShiftLoc, "expected constant shift amount");
    Out.Shamt = static_cast<unsigned>(ShiftVal);
    Out.HasExplicitShift = true;
  }

  return false;
}

bool LinxISAAsmParser::parseMemOperand(ParsedMem &OutMem) {
  SMLoc Start = getTok().getLoc();
  if (parseToken(AsmToken::LBrac, "expected '['"))
    return true;

  ParsedReg Base;
  if (parseRegOperand(Base))
    return true;

  if (parseToken(AsmToken::Comma, "expected ',' in memory operand"))
    return true;

  ParsedMem M;
  M.Base = Base;
  M.StartLoc = Start;

  // Offset can be a register (index) or an expression (immediate).
  if (getTok().is(AsmToken::Identifier)) {
    // Try index register first.
    StringRef Tok = getTok().getString();
    StringRef BaseTok = Tok;
    if (size_t Dot = Tok.find('.'); Dot != StringRef::npos)
      BaseTok = Tok.take_front(Dot);
    if (parseRegCode(BaseTok)) {
      ParsedReg Index;
      if (parseRegOperand(Index))
        return true;
      M.HasIndex = true;
      M.Index = Index;
    } else {
      const MCExpr *Expr = nullptr;
      SMLoc ExprLoc, ExprEnd;
      if (parseImmOperand(Expr, ExprLoc, ExprEnd))
        return true;
      M.OffExpr = Expr;
    }
  } else {
    const MCExpr *Expr = nullptr;
    SMLoc ExprLoc, ExprEnd;
    if (parseImmOperand(Expr, ExprLoc, ExprEnd))
      return true;
    M.OffExpr = Expr;
  }

  if (parseToken(AsmToken::RBrac, "expected ']'"))
    return true;

  M.EndLoc = getTok().getLoc();
  OutMem = M;
  return false;
}

bool LinxISAAsmParser::parseArrowDestOperand(ParsedReg &OutDest) {
  if (parseToken(AsmToken::MinusGreater, "expected '->'"))
    return true;

  ParsedReg D;
  D.Loc = getTok().getLoc();

  if (!getTok().is(AsmToken::Identifier))
    return Error(getTok().getLoc(),
                 "expected destination after '->' (use '->u' for U-hand)");

  StringRef Tok = getTok().getString();
  StringRef Base = Tok;
  if (size_t Dot = Tok.find('.'); Dot != StringRef::npos)
    Base = Tok.take_front(Dot);

  Lex();

  // Optional tile-descriptor angle suffix:
  //   - `->t<Size>` / `->acc<Size>` (B.IOT)
  //   - `->t<RegSrc>` / `->acc<RegSrc>` (B.IOT)
  // This syntax is used by B.IOT/B.IOT and is not a normal register operand.
  if (getTok().is(AsmToken::Less)) {
    std::string Up = toUpperStr(Base);
    unsigned Kind = 0;
    if (Up == "T") {
      Kind = 0;
    } else if (Up == "U") {
      Kind = 1;
    } else if (Up == "M") {
      Kind = 2;
    } else if (Up == "N") {
      Kind = 3;
    } else if (Up == "ACC") {
      Kind = 4;
    } else {
      return Error(D.Loc,
                   "invalid tile kind for '-><kind><...>' (expected t/u/m/n/acc)");
    }
    D.Code = Kind;
    Lex(); // '<'

    if (getTok().is(AsmToken::Identifier)) {
      if (auto Reg = parseRegCode(getTok().getString())) {
        if (*Reg >= 32u)
          return Error(getTok().getLoc(),
                       "B.IOT RegSrc must be a scalar 5-bit register");
        D.HasAngleReg = true;
        D.AngleReg = *Reg & 0x1fu;
        Lex();
        if (parseToken(AsmToken::Greater, "expected '>' to close reg suffix"))
          return true;
        OutDest = D;
        return false;
      }
    }

    // Parse size form: <SizeCode> or <N KB> or <N B>
    if (!getTok().is(AsmToken::Integer) && !getTok().is(AsmToken::Identifier))
      return Error(getTok().getLoc(),
                   "expected size code or <N KB>/<N B> after '<'");

    uint64_t Bytes = 0;
    bool HaveBytes = false;
    unsigned SizeCode = 0;

    auto toSizeCodeFromBytes = [&](uint64_t B) -> std::optional<unsigned> {
      if (B == 0 || (B & (B - 1u)) != 0u)
        return std::nullopt;
      unsigned Log2 = 0;
      uint64_t T = B;
      while (T > 1u) {
        T >>= 1;
        Log2++;
      }
      if (Log2 < 4u)
        return std::nullopt;
      const unsigned Code = Log2 - 4u;
      if (Code > 31u)
        return std::nullopt;
      return Code;
    };

    if (getTok().is(AsmToken::Integer)) {
      uint64_t N = 0;
      if (getTok().getString().getAsInteger(0, N))
        return Error(getTok().getLoc(), "invalid integer in size suffix");
      Lex();

      if (getTok().is(AsmToken::Identifier)) {
        std::string UnitUp = toUpperStr(getTok().getString());
        if (UnitUp == "KB") {
          Bytes = N * 1024u;
          HaveBytes = true;
          Lex();
        } else if (UnitUp == "B") {
          Bytes = N;
          HaveBytes = true;
          Lex();
        }
      }

      if (!HaveBytes) {
        // No unit: treat as the raw SizeCode.
        if (N > 31u)
          return Error(getTok().getLoc(), "size code must fit 5 bits");
        SizeCode = static_cast<unsigned>(N);
      }
    } else {
      // Identifier-only form: allow `4KB` / `512B` / `8` (as SizeCode).
      std::string TextUp = toUpperStr(getTok().getString());
      Lex();

      auto parseWithSuffix = [&](StringRef S, StringRef Suffix,
                                 uint64_t Scale) -> std::optional<uint64_t> {
        if (!S.ends_with(Suffix))
          return std::nullopt;
        StringRef Head = S.drop_back(Suffix.size());
        uint64_t N = 0;
        if (Head.getAsInteger(0, N))
          return std::nullopt;
        return N * Scale;
      };

      if (auto B = parseWithSuffix(TextUp, "KB", 1024u)) {
        Bytes = *B;
        HaveBytes = true;
      } else if (auto B = parseWithSuffix(TextUp, "B", 1u)) {
        Bytes = *B;
        HaveBytes = true;
      } else {
        uint64_t N = 0;
        if (StringRef(TextUp).getAsInteger(0, N) || N > 31u)
          return Error(getTok().getLoc(), "expected <SizeCode> or <N KB>/<N B>");
        SizeCode = static_cast<unsigned>(N);
      }
    }

    if (HaveBytes) {
      auto Code = toSizeCodeFromBytes(Bytes);
      if (!Code)
        return Error(getTok().getLoc(),
                     "size must be power-of-two bytes and >=16B");
      SizeCode = *Code;
    }

    // Canonical source syntax restricts unit-qualified sizes to 512B..4KB.
    // Raw disassembly SizeCode values must still roundtrip.
    if (HaveBytes && (SizeCode < 5u || SizeCode > 8u)) {
      return Error(getTok().getLoc(),
                   "tile size must be in strict range 512B..4KB");
    }

    if (parseToken(AsmToken::Greater, "expected '>' to close size suffix"))
      return true;

    D.HasAngleSize = true;
    D.AngleSize = SizeCode & 0x1fu;
    OutDest = D;
    return false;
  }

  auto Code = parseRegCode(Base);
  if (!Code)
    return Error(D.Loc, "invalid destination after '->'");
  D.Code = *Code;
  OutDest = D;
  return false;
}

static bool hasField(const linxisa_inst_form &Form, StringRef FieldName);

bool LinxISAAsmParser::parseInstruction(ParseInstructionInfo &Info,
                                        StringRef Name, SMLoc NameLoc,
                                        OperandVector &Operands) {
  (void)Info;

  Operands.push_back(LinxOperand::createToken(Name, NameLoc));

  // Bracketed operands are ambiguous in LinxISA syntax:
  // - Memory operands: [base, off] or [base, lc0<<k, off]
  // - Descriptor / template lists: [a, b, c] with optional keys.
  //
  // Use the encoding table as a heuristic: only parse memory operands for
  // mnemonics whose asm_fmt includes `[SrcL, ...]` / `[SrcR, ...]`.
  bool AllowMemOperands = false;
  bool AllowFrameRangeOperands = false;
  bool IsTileIODesc = false;
  bool IsBlockAttr = false;
  unsigned MaxArrowDests = 0;
  {
    std::string Key = toUpperStr(Name);
    if (StringRef(Key).ends_with(".LOCAL"))
      Key.resize(Key.size() - StringRef(".LOCAL").size());
    const auto &Map = getMnemonicMap();
    auto canonicalizePreviewKey = [&](std::string &K) {
      auto CanonicalizeByPrefix = [&](StringRef Prefix, StringRef Canonical) {
        if (StringRef(K).starts_with_insensitive(Prefix))
          K = Canonical.str();
      };
      CanonicalizeByPrefix("HL.PRF.A.", "HL.PRF.A");
      CanonicalizeByPrefix("HL.PRF.", "HL.PRF");
      CanonicalizeByPrefix("HL.PRFI.UA.", "HL.PRFI.UA");
      CanonicalizeByPrefix("HL.PRFI.U.", "HL.PRFI.U");
      CanonicalizeByPrefix("HL.QMT.", "HL.QMT");
      CanonicalizeByPrefix("HL.QPOP.", "HL.QPOP");
      CanonicalizeByPrefix("HL.QPUSH.", "HL.QPUSH");
      CanonicalizeByPrefix("V.FCVTI.", "V.FCVTI");
      CanonicalizeByPrefix("V.FCVT.", "V.FCVT");
      CanonicalizeByPrefix("V.ICVTF.", "V.ICVTF");
      CanonicalizeByPrefix("V.ICVT.", "V.ICVT");
    };
    if (Map.find(Key) == Map.end()) {
      canonicalizePreviewKey(Key);
    }
    if (Map.find(Key) == Map.end()) {
      const size_t Dot = Key.rfind('.');
      if (Dot != std::string::npos) {
        std::string BaseKey = Key.substr(0, Dot);
        unsigned Aq = 0, Far = 0, Rl = 0, Rd = 0;
        if (decodeAtomicMnemonicSuffix(StringRef(Key).substr(Dot + 1), Aq, Far,
                                       Rl, Rd) &&
            Map.find(BaseKey) != Map.end()) {
          Key = BaseKey;
        }
      }
    }
    auto It = Map.find(Key);
    if (It != Map.end()) {
      for (unsigned FormIndex : It->second) {
        const linxisa_inst_form &F = linxisa_inst_forms[FormIndex];
        StringRef Fmt(F.asm_fmt ? F.asm_fmt : "");
        std::string FmtUp = toUpperStr(Fmt);
        StringRef FmtRef(FmtUp);
        if (FmtRef.contains("[SRCL") || FmtRef.contains("[SRCR")) {
          AllowMemOperands = true;
          break;
        }
        if (Fmt.contains("[RegSrc0 ~ RegSrcn]") ||
            Fmt.contains("[RegDst0 ~ RegDstn]"))
          AllowFrameRangeOperands = true;
      }
      for (unsigned FormIndex : It->second) {
        const linxisa_inst_form &F = linxisa_inst_forms[FormIndex];
        if (hasField(F, "RegDst1") || hasField(F, "RegDst0"))
          MaxArrowDests = std::max(MaxArrowDests, 2u);
        else if (hasField(F, "RegDst"))
          MaxArrowDests = std::max(MaxArrowDests, 1u);
      }
    }

    StringRef KeyRef(Key);
    IsTileIODesc = KeyRef == "B.IOT";
    IsBlockAttr = KeyRef == "B.CATR" || KeyRef == "B.DATR";
  }

  while (!getTok().is(AsmToken::EndOfStatement)) {
    if (getTok().is(AsmToken::Comma)) {
      Lex();
      continue;
    }

    if (AllowFrameRangeOperands && getTok().is(AsmToken::Identifier) &&
        getTok().getString().equals_insensitive("sp") &&
        getLexer().peekTok().is(AsmToken::Exclaim)) {
      Lex(); // sp
      Lex(); // !
      continue;
    }

    if (getTok().is(AsmToken::MinusGreater)) {
      ParsedReg D;
      if (parseArrowDestOperand(D))
        return true;
      Operands.push_back(LinxOperand::createArrowDest(D, getTok().getLoc()));
      if (MaxArrowDests >= 2) {
        if (parseToken(AsmToken::Comma,
                       "expected ',' before second destination register"))
          return true;
        ParsedReg D1;
        if (parseRegOperand(D1))
          return true;
        Operands.push_back(LinxOperand::createArrowDest(D1, getTok().getLoc()));
      }
      continue;
    }

    if (getTok().is(AsmToken::LBrac)) {
      if (IsTileIODesc)
        return Error(getTok().getLoc(),
                     "bracketed B.IOT source lists are not canonical v0.56; "
                     "write tile sources as comma-separated operands");
      if (AllowFrameRangeOperands) {
        SMLoc Start = getTok().getLoc();
        Lex(); // '['
        ParsedReg Begin;
        if (parseRegOperand(Begin))
          return true;
        if (parseToken(AsmToken::Tilde, "expected '~' in register range"))
          return true;
        ParsedReg End;
        if (parseRegOperand(End))
          return true;
        if (parseToken(AsmToken::RBrac, "expected ']'"))
          return true;
        Operands.push_back(LinxOperand::createReg(Begin, getTok().getLoc()));
        Operands.push_back(LinxOperand::createReg(End, getTok().getLoc()));
        (void)Start;
        continue;
      }

      // Bracketed operands are either:
      //   - Memory operands: [base, off] (base is a register; ',' after it)
      //   - Bracketed operand lists: [Key=Val, Key=Val, ...] or [Val, Val, ...]
      //     (used by template blocks and descriptor-like ops in the spec)
      SMLoc Start = getTok().getLoc();
      Lex(); // consume '['

      if (getTok().is(AsmToken::Identifier)) {
        StringRef Tok = getTok().getString();
        StringRef BaseTok = Tok;
        if (size_t Dot = Tok.find('.'); Dot != StringRef::npos)
          BaseTok = Tok.take_front(Dot);

        const bool LooksLikeReg = parseRegCode(BaseTok).has_value();
        const bool IsMem = AllowMemOperands && LooksLikeReg &&
                           getLexer().peekTok().is(AsmToken::Comma);
        if (IsMem) {
          // Parse [base, off] without re-consuming '['.
          ParsedReg Base;
          if (parseRegOperand(Base))
            return true;

          if (parseToken(AsmToken::Comma, "expected ',' in memory operand"))
            return true;

          ParsedMem M;
          M.Base = Base;
          M.StartLoc = Start;

          if (getTok().is(AsmToken::Identifier)) {
            StringRef ITok = getTok().getString();
            StringRef IBaseTok = ITok;
            if (size_t Dot = ITok.find('.'); Dot != StringRef::npos)
              IBaseTok = ITok.take_front(Dot);
            if (parseRegCode(IBaseTok)) {
              ParsedReg Index;
              if (parseRegOperand(Index))
                return true;
              if (getTok().is(AsmToken::Comma)) {
                // Vector/SIMT mem syntax: [base, <lc0<<k>, idx|imm]
                M.HasLane = true;
                M.Lane = Index;
                Lex(); // consume ','

                if (getTok().is(AsmToken::Identifier)) {
                  StringRef DTok = getTok().getString();
                  StringRef DBaseTok = DTok;
                  if (size_t Dot = DTok.find('.'); Dot != StringRef::npos)
                    DBaseTok = DTok.take_front(Dot);
                  if (parseRegCode(DBaseTok)) {
                    ParsedReg DIndex;
                    if (parseRegOperand(DIndex))
                      return true;
                    M.HasIndex = true;
                    M.Index = DIndex;
                  } else {
                    const MCExpr *Expr = nullptr;
                    SMLoc ExprLoc, ExprEnd;
                    if (parseImmOperand(Expr, ExprLoc, ExprEnd))
                      return true;
                    M.OffExpr = Expr;
                  }
                } else {
                  const MCExpr *Expr = nullptr;
                  SMLoc ExprLoc, ExprEnd;
                  if (parseImmOperand(Expr, ExprLoc, ExprEnd))
                    return true;
                  M.OffExpr = Expr;
                }
              } else {
                M.HasIndex = true;
                M.Index = Index;
              }
            } else {
              const MCExpr *Expr = nullptr;
              SMLoc ExprLoc, ExprEnd;
              if (parseImmOperand(Expr, ExprLoc, ExprEnd))
                return true;
              M.OffExpr = Expr;
            }
          } else {
            const MCExpr *Expr = nullptr;
            SMLoc ExprLoc, ExprEnd;
            if (parseImmOperand(Expr, ExprLoc, ExprEnd))
              return true;
            M.OffExpr = Expr;
          }

          if (parseToken(AsmToken::RBrac, "expected ']'"))
            return true;
          M.EndLoc = getTok().getLoc();

          Operands.push_back(LinxOperand::createMem(M));
          continue;
        }
      }

      // Parse a comma-separated list of optional key-value pairs inside [..].
      while (!getTok().is(AsmToken::EndOfStatement) &&
             !getTok().is(AsmToken::RBrac)) {
        if (getTok().is(AsmToken::Comma)) {
          Lex();
          continue;
        }

        // Optional "Key=" (ignored; order defines field binding for now).
        if (getTok().is(AsmToken::Identifier) &&
            getLexer().peekTok().is(AsmToken::Equal)) {
          Lex(); // key
          Lex(); // '='
        }

        if (getTok().is(AsmToken::Identifier)) {
          // Tile descriptors: treat t#/u#/m#/n# as tile IDs (not registers) and
          // encode `.reuse` inline.
          if (IsTileIODesc) {
            bool Reuse = false;
            if (auto Tile = parseTileRef(getTok().getString(), Reuse)) {
              SMLoc S = getTok().getLoc();
              SMLoc E = getTok().getEndLoc();
              Lex();
              const unsigned Enc = (*Tile & 0x3fu) | (Reuse ? (1u << 6) : 0u);
              Operands.push_back(LinxOperand::createImm(
                  MCConstantExpr::create(Enc, getContext()), S, E));
              continue;
            }
          }

          // Try register first, then SSR name, then expression.
          StringRef VTok = getTok().getString();
          StringRef VBase = VTok;
          if (size_t Dot = VTok.find('.'); Dot != StringRef::npos)
            VBase = VTok.take_front(Dot);

          if (parseRegCode(VBase)) {
            ParsedReg R;
            if (parseRegOperand(R))
              return true;
            Operands.push_back(LinxOperand::createReg(R, getTok().getLoc()));
            continue;
          }

          if (auto SSR = parseSSRIdName(getTok().getString())) {
            SMLoc S = getTok().getLoc();
            SMLoc E = getTok().getEndLoc();
            Lex();
            Operands.push_back(LinxOperand::createImm(
                MCConstantExpr::create(*SSR, getContext()), S, E));
            continue;
          }
        }

        const MCExpr *Expr = nullptr;
        SMLoc ExprStart, ExprEnd;
        if (parseImmOperand(Expr, ExprStart, ExprEnd))
          return true;
        Operands.push_back(LinxOperand::createImm(Expr, ExprStart, ExprEnd));
      }

      if (parseToken(AsmToken::RBrac, "expected ']'"))
        return true;
      continue;
    }

    if (getTok().is(AsmToken::LCurly)) {
      Lex(); // '{'
      while (!getTok().is(AsmToken::EndOfStatement) &&
             !getTok().is(AsmToken::RCurly)) {
        if (getTok().is(AsmToken::Comma)) {
          Lex();
          continue;
        }

        if (getTok().is(AsmToken::Identifier)) {
          SMLoc L = getTok().getLoc();
          std::string Text = getTok().getString().str();
          SMLoc E = getTok().getEndLoc();
          Lex();
          if (getTok().is(AsmToken::Equal)) {
            Lex();
            if (!getTok().is(AsmToken::Identifier))
              return Error(getTok().getLoc(), "expected identifier after '='");
            Text += "=";
            Text += getTok().getString().str();
            E = getTok().getEndLoc();
            Lex();
          }
          Operands.push_back(LinxOperand::createKeyword(Text, L, E));
          continue;
        }

        const MCExpr *Expr = nullptr;
        SMLoc ExprStart, ExprEnd;
        if (parseImmOperand(Expr, ExprStart, ExprEnd))
          return true;
        Operands.push_back(LinxOperand::createImm(Expr, ExprStart, ExprEnd));
      }

      if (parseToken(AsmToken::RCurly, "expected '}'"))
        return true;
      continue;
    }

    if (getTok().is(AsmToken::Identifier)) {
      // Fused BSTART return-target syntax: `ra=<label>`.
      if (getTok().getString().equals_insensitive("ra") &&
          getLexer().peekTok().is(AsmToken::Equal)) {
        SMLoc Start = getTok().getLoc();
        Lex();
        if (parseToken(AsmToken::Equal, "expected '=' after 'ra'"))
          return true;

        const MCExpr *Expr = nullptr;
        SMLoc ExprStart, ExprEnd;
        if (parseImmOperand(Expr, ExprStart, ExprEnd))
          return true;
        Operands.push_back(
            LinxOperand::createSetRetTarget(Expr, Start, ExprEnd));
        continue;
      }

      // Keywords used by block headers (BSTART/C.BSTART).
      if (auto Br = parseBrType(getTok().getString())) {
        SMLoc L = getTok().getLoc();
        SMLoc E = getTok().getEndLoc();
        StringRef T = getTok().getString();
        Lex();
        Operands.push_back(LinxOperand::createKeyword(T, L, E));
        continue;
      }

      // Tile IOT group marker: disassembler prints `, last` for group=1.
      if (IsTileIODesc && getTok().getString().equals_insensitive("last")) {
        SMLoc L = getTok().getLoc();
        SMLoc E = getTok().getEndLoc();
        StringRef T = getTok().getString();
        Lex();
        Operands.push_back(LinxOperand::createKeyword(T, L, E));
        continue;
      }

      if (IsTileIODesc) {
        bool Reuse = false;
        if (auto Tile = parseTileRef(getTok().getString(), Reuse)) {
          SMLoc L = getTok().getLoc();
          SMLoc E = getTok().getEndLoc();
          Lex();
          const unsigned Enc = (*Tile & 0x3fu) | (Reuse ? (1u << 6) : 0u);
          Operands.push_back(LinxOperand::createImm(
              MCConstantExpr::create(Enc, getContext()), L, E));
          continue;
        }
      }

      if (IsBlockAttr) {
        StringRef Text = getTok().getString();
        if (Text.equals_insensitive("AQ") || Text.equals_insensitive("RL") ||
            Text.equals_insensitive("AQRL") ||
            Text.equals_insensitive("ATOMIC") ||
            Text.equals_insensitive("TRAP") || Text.equals_insensitive("DR") ||
            Text.equals_insensitive("FAR") || Text.equals_insensitive("SAT") ||
            Text.starts_with_insensitive("CMODE") ||
            Text.starts_with_insensitive("RMODE") ||
            Text.equals_insensitive("NORMAL") ||
            Text.equals_insensitive("CANON") || parseDataTypeKeyword(Text) ||
            parsePadValueKeyword(Text)) {
          SMLoc L = getTok().getLoc();
          SMLoc E = getTok().getEndLoc();
          Lex();
          Operands.push_back(LinxOperand::createKeyword(Text, L, E));
          continue;
        }
      }

      // Registers (including suffixed forms like a0.sw).
      StringRef Tok = getTok().getString();
      StringRef BaseTok = Tok;
      if (size_t Dot = Tok.find('.'); Dot != StringRef::npos)
        BaseTok = Tok.take_front(Dot);
      if (parseRegCode(BaseTok)) {
        ParsedReg R;
        if (parseRegOperand(R))
          return true;
        Operands.push_back(LinxOperand::createReg(R, getTok().getLoc()));
        continue;
      }

      // System Status Register (SSR) names (e.g. TP/GP/CYCLE).
      if (auto SSR = parseSSRIdName(getTok().getString())) {
        SMLoc Start = getTok().getLoc();
        SMLoc End = getTok().getEndLoc();
        Lex(); // consume name
        Operands.push_back(LinxOperand::createImm(
            MCConstantExpr::create(*SSR, getContext()), Start, End));
        continue;
      }

      // Otherwise treat as expression.
      const MCExpr *Expr = nullptr;
      SMLoc Start, End;
      if (parseImmOperand(Expr, Start, End))
        return true;
      Operands.push_back(LinxOperand::createImm(Expr, Start, End));
      continue;
    }

    // Expression (numbers, symbols with +/-).
    const MCExpr *Expr = nullptr;
    SMLoc Start, End;
    if (parseImmOperand(Expr, Start, End))
      return true;
    Operands.push_back(LinxOperand::createImm(Expr, Start, End));
  }

  return false;
}

bool LinxISAAsmParser::buildParsedInst(OperandVector &Operands, ParsedInst &Out) {
  Out = ParsedInst();
  for (unsigned i = 1; i < Operands.size(); ++i) {
    auto *Op = static_cast<LinxOperand *>(Operands[i].get());
    switch (Op->getKind()) {
    case LinxOperand::Reg:
      Out.Regs.push_back(Op->getParsedReg());
      break;
    case LinxOperand::ArrowDest:
      Out.ArrowDests.push_back(Op->getParsedReg());
      break;
    case LinxOperand::Imm: {
      ParsedImm I;
      I.Expr = Op->getExpr();
      I.Loc = Op->getStartLoc();
      Out.Imms.push_back(I);
      break;
    }
    case LinxOperand::SetRetTarget: {
      ParsedImm I;
      I.Expr = Op->getExpr();
      I.Loc = Op->getStartLoc();
      Out.SetRetTarget = I;
      break;
    }
    case LinxOperand::Mem:
      Out.Mem = Op->getMem();
      break;
    case LinxOperand::Keyword:
      Out.Keywords.push_back(Op->getKeyword());
      break;
    case LinxOperand::Token:
      break;
    }
  }
  return false;
}

static bool hasField(const linxisa_inst_form &Form, StringRef FieldName) {
  for (unsigned i = 0; i < Form.field_count; ++i) {
    const linxisa_field &F = linxisa_fields[Form.field_start + i];
    if (!F.name)
      continue;
    if (FieldName == StringRef(F.name))
      return true;
  }
  return false;
}

bool LinxISAAsmParser::buildMCInstForForm(unsigned FormIndex, const ParsedInst &PI,
                                          MCInst &OutInst, std::string &Err) {
  const linxisa_inst_form &Form = linxisa_inst_forms[FormIndex];
  StringRef AsmFmt(Form.asm_fmt ? Form.asm_fmt : "");

  auto require = [&](bool Cond, const Twine &Msg) -> bool {
    if (!Cond) {
      Err = Msg.str();
      return false;
    }
    return true;
  };

  OutInst.clear();
  OutInst.setOpcode(FormIndex);

  if (PI.LocalBit != 0 && !hasField(Form, "L")) {
    Err = "mnemonic suffix '.local' is not supported for this instruction";
    return false;
  }

  // Helpers for emitting a field value in spec order.
  auto emitFieldImm = [&](int64_t V) { OutInst.addOperand(MCOperand::createImm(V)); };
  auto emitFieldExpr = [&](const MCExpr *E) {
    OutInst.addOperand(MCOperand::createExpr(E));
  };

  // Special-case: setret/c.setret/hl.setret.
  if (AsmFmt.starts_with_insensitive("setret") ||
      AsmFmt.starts_with_insensitive("c.setret") ||
      AsmFmt.starts_with_insensitive("hl.setret")) {
    if (!require(PI.Imms.size() == 1, "expected target for setret"))
      return false;
    if (!require(PI.ArrowDests.empty() || PI.ArrowDests.size() == 1,
                 "unexpected destination for setret"))
      return false;
    if (!require(PI.ArrowDests.empty() || PI.ArrowDests[0].Code == 10,
                 "setret destination must be ->ra"))
      return false;
    if (!require(PI.Regs.empty() && PI.Keywords.empty() && !PI.Mem &&
                     !PI.SetRetTarget,
                 "unexpected operands for setret"))
      return false;
    const MCExpr *Target = PI.Imms[0].Expr;
    int64_t V = 0;
    bool IsConst = isConstExpr(Target, V);

    if (!require(Form.field_count == 1, "unexpected setret field layout"))
      return false;
    if (IsConst)
      emitFieldImm(V);
    else
      emitFieldExpr(Target);
    return true;
  }

  // BSTOP/C.BSTOP: no operands.
  if (AsmFmt.equals_insensitive("bstop") || AsmFmt.equals_insensitive("c.bstop")) {
    if (!require(Form.field_count == 0, "unexpected bstop field layout"))
      return false;
    if (!require(PI.Regs.empty() && PI.Imms.empty() && PI.Keywords.empty() &&
                     !PI.Mem && PI.ArrowDests.empty() && !PI.SetRetTarget,
                 "unexpected operands for bstop"))
      return false;
    return true;
  }

  // Block headers: BSTART/C.BSTART/HL.BSTART/L.BSTART (branch-kind forms only).
  const bool IsBStartBranchHeader =
      (AsmFmt.starts_with_insensitive("bstart") ||
       AsmFmt.starts_with_insensitive("c.bstart") ||
       AsmFmt.starts_with_insensitive("hl.bstart") ||
       AsmFmt.starts_with_insensitive("l.bstart")) &&
      (hasField(Form, "BrType") || AsmFmt.contains_insensitive("{DIRECT, CALL}") ||
       AsmFmt.contains_insensitive(" FALL") || AsmFmt.contains_insensitive(" DIRECT") ||
       AsmFmt.contains_insensitive(" COND") || AsmFmt.contains_insensitive(" CALL") ||
       AsmFmt.contains_insensitive(" ICALL") || AsmFmt.contains_insensitive(" IND") ||
       AsmFmt.contains_insensitive(" RET"));

  if (IsBStartBranchHeader) {
    if (!require(PI.Regs.empty() && !PI.Mem && PI.ArrowDests.empty() &&
                     !PI.SetRetTarget,
                 "unexpected operands for bstart"))
      return false;
    if (!require(PI.Keywords.size() <= 1, "too many branch kind operands"))
      return false;
    if (!require(PI.Imms.size() <= 1, "too many immediate operands"))
      return false;

    std::optional<unsigned> BrTypeVal;
    if (!PI.Keywords.empty()) {
      BrTypeVal = parseBrType(PI.Keywords[0].TextUpper);
      if (!require(BrTypeVal.has_value(), "invalid branch kind (BrType)"))
        return false;
    } else if (hasField(Form, "BrType")) {
      // FALL is the default when the branch kind is omitted.
      BrTypeVal = 1u;
    }

    const MCExpr *LabelExpr = nullptr;
    if (!PI.Imms.empty())
      LabelExpr = PI.Imms[0].Expr;

    const bool HasBrTypeField = hasField(Form, "BrType");
    bool HasLabelField = false;
    for (unsigned i = 0; i < Form.field_count; ++i) {
      StringRef FN(linxisa_fields[Form.field_start + i].name);
      if (FN.starts_with("simm") || FN.starts_with("imm")) {
        HasLabelField = true;
        break;
      }
    }

    const BStartKind WantKind =
        BrTypeVal ? brTypeToBStartKind(*BrTypeVal) : BStartKind::Unknown;
    const BStartKind FormKind = bstartKindFromAsmFmt(AsmFmt);
    const BStartKind EffectiveKind =
        HasBrTypeField ? WantKind : FormKind;

    // If the encoding carries BrType, use the parsed/defaulted branch kind.
    if (HasBrTypeField && !require(BrTypeVal.has_value(),
                                   "expected branch kind (BrType)"))
      return false;

    // If the encoding does not carry BrType, ensure the requested kind matches
    // the chosen encoding.
    if (!HasBrTypeField && BrTypeVal.has_value()) {
      if (!require(FormKind != BStartKind::Unknown,
                   "invalid BSTART encoding kind"))
        return false;
      const bool KindOK =
          (FormKind == WantKind) ||
          (FormKind == BStartKind::DirectOrCall &&
           (WantKind == BStartKind::Direct || WantKind == BStartKind::Call));
      if (!require(KindOK, "branch kind does not match BSTART encoding"))
        return false;
    } else if (!HasBrTypeField && !BrTypeVal.has_value()) {
      // Without an explicit kind, only FALL forms are valid.
      if (!require(FormKind == BStartKind::Fall,
                   "expected branch kind (e.g. FALL/DIRECT/CALL)"))
        return false;
    }

    // Require a label operand for encodings that carry a label.
    if (LabelExpr && !HasLabelField)
      return require(false, "unexpected label operand for this BSTART encoding");

    if (HasLabelField) {
      const bool NeedsLabel =
          EffectiveKind == BStartKind::Direct || EffectiveKind == BStartKind::Cond ||
          EffectiveKind == BStartKind::Call ||
          EffectiveKind == BStartKind::DirectOrCall;
      if (EffectiveKind == BStartKind::Fall || EffectiveKind == BStartKind::Unknown) {
        // FALL encodings may include an optional fixup label.
      } else if (NeedsLabel) {
        if (!require(LabelExpr != nullptr, "expected branch target label"))
          return false;
      } else {
        if (!require(LabelExpr == nullptr, "unexpected branch target label"))
          return false;
      }
    }

    unsigned BlockTypeVal = 0;
    if (hasField(Form, "BlockType")) {
      if (auto BT = parseBlockTypeFromMnemonic(AsmFmt.split(' ').first))
        BlockTypeVal = *BT;
    }

    // Fill fields in encoding order.
    for (unsigned i = 0; i < Form.field_count; ++i) {
      StringRef FN(linxisa_fields[Form.field_start + i].name);
      if (FN == "BrType") {
        if (!require(BrTypeVal.has_value(), "expected branch kind (BrType)"))
          return false;
        emitFieldImm(*BrTypeVal);
        continue;
      }
      if (FN == "reserve") {
        if (AsmFmt.equals_insensitive("BSTART.FP RET") ||
            AsmFmt.equals_insensitive("BSTART.FP IND") ||
            AsmFmt.equals_insensitive("BSTART.FP ICALL") ||
            AsmFmt.equals_insensitive("BSTART.STD RET") ||
            AsmFmt.equals_insensitive("BSTART.STD IND") ||
            AsmFmt.equals_insensitive("BSTART.STD ICALL")) {
          emitFieldImm(129);
          continue;
        }
        emitFieldImm(0);
        continue;
      }
      if (FN == "BlockType") {
        emitFieldImm(BlockTypeVal);
        continue;
      }
      if (FN.starts_with("simm") || FN.starts_with("imm")) {
        if (!LabelExpr) {
          emitFieldImm(0);
          continue;
        }
        int64_t V = 0;
        if (isConstExpr(LabelExpr, V)) {
          if (FN == "simm" &&
              AsmFmt.starts_with_insensitive("L.BSTART")) {
            if (!require((V & 0x1) == 0,
                         "L.BSTART target is not 2-byte aligned"))
              return false;
            V >>= 1;
            if (!require(isInt<42>(V), "L.BSTART target out of range"))
              return false;
          }
          emitFieldImm(V);
        } else {
          emitFieldExpr(LabelExpr);
        }
        continue;
      }
      Err = ("unsupported BSTART field: " + FN).str();
      return false;
    }
    return true;
  }

  // Memory ops.
  if (PI.Mem.has_value()) {
    const ParsedMem &M = *PI.Mem;
    const bool HasArrow = AsmFmt.contains("->");
    const bool AllowsExplicitHiddenMemDst =
        AsmFmt.starts_with_insensitive("prf") ||
        AsmFmt.starts_with_insensitive("prfi.");
    const bool IsHlCas = AsmFmt.starts_with_insensitive("hl.cas");
    const bool IsLr = AsmFmt.starts_with_insensitive("lr.");
    unsigned MemRegIdx = 0;

    auto takeMemReg = [&]() -> std::optional<ParsedReg> {
      if (MemRegIdx >= PI.Regs.size())
        return std::nullopt;
      return PI.Regs[MemRegIdx++];
    };

    auto laneScaleFromAsm = [&]() -> std::optional<unsigned> {
      if (!AsmFmt.contains_insensitive("<lc0"))
        return std::nullopt;
      if (AsmFmt.contains_insensitive("lc0<<3"))
        return 3u;
      if (AsmFmt.contains_insensitive("lc0<<2"))
        return 2u;
      if (AsmFmt.contains_insensitive("lc0<<1"))
        return 1u;
      return 0u;
    };

    const std::optional<unsigned> LaneScale = laneScaleFromAsm();
    if (LaneScale.has_value()) {
      if (!require(M.HasLane,
                   "expected vector memory operand '[base, lc0<<k, off]'"))
        return false;
      const unsigned ExpectedLc0 = (3u << 5) | 0u; // lc0
      if (!require(M.Lane.Code == ExpectedLc0, "expected lane base 'lc0'"))
        return false;
      if (!require(M.Lane.Shamt == *LaneScale,
                   "unexpected lane scale; expected lc0<<k per mnemonic"))
        return false;
    } else {
      if (M.HasLane) {
        const unsigned ExpectedLc0 = (3u << 5) | 0u; // lc0
        const bool AllowCompatVectorLane =
            Form.length_bits == 64 && M.HasIndex && M.Lane.Code == ExpectedLc0 &&
            hasField(Form, "SrcR");
        if (!require(AllowCompatVectorLane,
                     "unexpected 3-part memory operand for this instruction"))
          return false;
      }
    }

    // Determine base field from the asm template: `[SrcL, ...]` / `[SrcR, ...]`.
    StringRef BaseField = "SrcL";
    if (size_t L = AsmFmt.find('['); L != StringRef::npos) {
      StringRef Inside = AsmFmt.substr(L + 1);
      Inside = Inside.split(']').first;
      StringRef BasePart = Inside.split(',').first.trim();
      if (BasePart.starts_with_insensitive("srcr"))
        BaseField = "SrcR";
      else if (BasePart.starts_with_insensitive("srcl"))
        BaseField = "SrcL";
    }

    // Detect encoded value field (SrcD or SrcL). Otherwise the store value is
    // implicit (for example compressed stores) or the instruction is a load.
    std::optional<StringRef> ValueField;
    StringRef Prefix = AsmFmt;
    if (size_t L = Prefix.find('['); L != StringRef::npos)
      Prefix = Prefix.take_front(L);
    if (Prefix.contains_insensitive("SrcD"))
      ValueField = "SrcD";
    else if (Prefix.contains_insensitive("SrcL"))
      ValueField = "SrcL";
    const bool IsStore = ValueField.has_value();

    // Provide field values.
    if (IsStore)
      if (!require(PI.Regs.size() >= 1, "expected store value register"))
        return false;

    int64_t Scale = memScaleFromMnemonic(Form.mnemonic ? StringRef(Form.mnemonic)
                                                       : StringRef());

    for (unsigned i = 0; i < Form.field_count; ++i) {
      const linxisa_field &Field = linxisa_fields[Form.field_start + i];
      StringRef FN(Field.name);

      if (FN == "C") {
        const bool IsVectorLocalMem =
            Form.length_bits == 64 && hasField(Form, "L") && PI.LocalBit != 0;
        emitFieldImm(IsVectorLocalMem ? 1 : 0);
        continue;
      }

      if (FN == "L") {
        emitFieldImm(static_cast<int64_t>(PI.LocalBit & 1u));
        continue;
      }

      if (FN == "RegDst") {
        if (!require(HasArrow || AllowsExplicitHiddenMemDst,
                     "expected '->' destination for RegDst"))
          return false;
        if (!require(!PI.ArrowDests.empty(), "expected destination after '->'"))
          return false;
        if (!require(PI.ArrowDests[0].Code < (1u << Field.bit_width),
                     "destination register does not fit field width"))
          return false;
        emitFieldImm(static_cast<int64_t>(PI.ArrowDests[0].Code));
        continue;
      }

      if (FN == "RegDst0" || FN == "RegDst1") {
        if (!require(HasArrow, "expected '->' destination list for RegDst0/1"))
          return false;
        const unsigned Index = (FN == "RegDst0") ? 0u : 1u;
        if (!require(PI.ArrowDests.size() > Index,
                     "missing destination register after '->'"))
          return false;
        if (!require(PI.ArrowDests[Index].Code < (1u << Field.bit_width),
                     "destination register does not fit field width"))
          return false;
        emitFieldImm(static_cast<int64_t>(PI.ArrowDests[Index].Code));
        continue;
      }

      if (FN == BaseField) {
        if (!require(M.Base.Code < (1u << Field.bit_width),
                     "base register does not fit field width"))
          return false;
        emitFieldImm(static_cast<int64_t>(M.Base.Code));
        continue;
      }

      if (FN == "SrcD" && IsStore) {
        if (!require(ValueField.has_value() && *ValueField == "SrcD",
                     "unexpected SrcD for this store"))
          return false;
        if (!require(PI.Regs[0].Code < (1u << Field.bit_width),
                     "store value register does not fit field width"))
          return false;
        emitFieldImm(static_cast<int64_t>(PI.Regs[0].Code));
        continue;
      }

      if (FN == "SrcD" && !IsStore) {
        if (IsHlCas) {
          if (!require(PI.Regs.size() >= 2, "expected compare and swap registers"))
            return false;
          if (!require(PI.Regs[1].Code < (1u << Field.bit_width),
                       "store value register does not fit field width"))
            return false;
          emitFieldImm(static_cast<int64_t>(PI.Regs[1].Code));
        } else {
          if (auto R = takeMemReg()) {
            if (!require(R->Code < (1u << Field.bit_width),
                         "store value register does not fit field width"))
              return false;
            emitFieldImm(static_cast<int64_t>(R->Code));
          } else {
            emitFieldImm(0);
          }
        }
        continue;
      }

      if (FN == "SrcD1") {
        if (!require(IsStore, "unexpected SrcD1 on load"))
          return false;
        if (!require(PI.Regs.size() >= 2, "expected second store value register"))
          return false;
        if (!require(PI.Regs[1].Code < (1u << Field.bit_width),
                     "store value register does not fit field width"))
          return false;
        emitFieldImm(static_cast<int64_t>(PI.Regs[1].Code));
        continue;
      }

      if (FN == "SrcL" && IsStore && ValueField.has_value() && *ValueField == "SrcL") {
        if (!require(PI.Regs[0].Code < (1u << Field.bit_width),
                     "store value register does not fit field width"))
          return false;
        emitFieldImm(static_cast<int64_t>(PI.Regs[0].Code));
        continue;
      }

      if (FN == "SrcR") {
        if (M.HasIndex) {
          if (!require(M.Index.Code < (1u << Field.bit_width),
                       "index register does not fit field width"))
            return false;
          emitFieldImm(static_cast<int64_t>(M.Index.Code));
          continue;
        }
        // If this is an imm-offset form, SrcR might be the base field.
        if (BaseField == "SrcR") {
          if (!require(M.Base.Code < (1u << Field.bit_width),
                       "base register does not fit field width"))
          return false;
          emitFieldImm(static_cast<int64_t>(M.Base.Code));
          continue;
        }
        if (IsHlCas) {
          if (!require(!PI.Regs.empty(), "expected compare register"))
            return false;
          if (!require(PI.Regs[0].Code < (1u << Field.bit_width),
                       "register operand does not fit field width"))
            return false;
          emitFieldImm(static_cast<int64_t>(PI.Regs[0].Code));
          continue;
        }
        if (auto R = takeMemReg()) {
          if (!require(R->Code < (1u << Field.bit_width),
                       "register operand does not fit field width"))
            return false;
          emitFieldImm(static_cast<int64_t>(R->Code));
        } else {
          emitFieldImm(0);
        }
        continue;
      }

      if (FN == "SrcRType") {
        if (!require(M.HasIndex, "expected register offset for SrcRType"))
          return false;
        emitFieldImm(static_cast<int64_t>(M.Index.SrcRType));
        continue;
      }

      if (FN == "shamt") {
        if (M.HasIndex) {
          unsigned Sh = M.Index.Shamt;
          if (AsmFmt.contains("+shamt")) {
            const unsigned ScaleK = LaneScale.value_or(0u);
            const unsigned Actual = M.Index.HasExplicitShift ? Sh : ScaleK;
            if (!require(Actual >= ScaleK, "expected index shift >= lane scale"))
              return false;
            Sh = Actual - ScaleK;
          }
          emitFieldImm(static_cast<int64_t>(Sh));
          continue;
        }
        emitFieldImm(0);
        continue;
      }

      if (FN.starts_with("simm") || FN.starts_with("uimm")) {
        if (!require(!M.HasIndex, "expected immediate offset in memory operand"))
          return false;
        if (!require(M.OffExpr != nullptr, "missing memory offset"))
          return false;
        int64_t ByteOff = 0;
        if (!require(isConstExpr(M.OffExpr, ByteOff),
                     "memory offsets must be constant for now"))
          return false;
        if (!require(Scale != 0 && (ByteOff % Scale) == 0,
                     "memory offset is not aligned for instruction scale"))
          return false;
        emitFieldImm(ByteOff / Scale);
        continue;
      }

      if (FN == "model") {
        if (!require(PI.Imms.size() == 1 && PI.Imms[0].Expr,
                     "missing model immediate"))
          return false;
        const MCExpr *E = PI.Imms[0].Expr;
        int64_t V = 0;
        if (!require(isConstExpr(E, V), "model must be a constant for now"))
          return false;
        emitFieldImm(V);
        continue;
      }

      if (FN == "SrcZero") {
        emitFieldImm((AsmFmt.starts_with_insensitive("jr ") || IsLr) ? 1 : 0);
        continue;
      }

      if (FN == "aq") {
        emitFieldImm(static_cast<int64_t>(PI.AtomicAq & 1u));
        continue;
      }

      if (FN == "rl") {
        emitFieldImm(static_cast<int64_t>(PI.AtomicRl & 1u));
        continue;
      }

      if (FN == "far") {
        emitFieldImm(static_cast<int64_t>(PI.AtomicFar & 1u));
        continue;
      }

      if (FN == "rd") {
        emitFieldImm(static_cast<int64_t>(PI.AtomicRd & 1u));
        continue;
      }

      Err = ("unsupported memory field: " + FN).str();
      return false;
    }

    // Sanity: for stores with an explicit encoded value reg, require it be present.
    if (IsStore && ValueField.has_value())
      if (!require(PI.Regs.size() >= 1, "expected store value register"))
        return false;

    return true;
  }

  // Special-case: block argument format selector (B.ARG).
  if (AsmFmt.starts_with("B.ARG")) {
    if (!require(!PI.Mem && PI.ArrowDests.empty() && !PI.SetRetTarget,
                 "unexpected operands for B.ARG"))
      return false;
    if (!require(PI.Regs.empty(), "unexpected register operands for B.ARG"))
      return false;
    if (!require(PI.Keywords.empty(), "unexpected keyword operands for B.ARG"))
      return false;
    if (Form.field_count == 0) {
      SmallString<64> Printed;
      for (unsigned I = 0; I < PI.Imms.size(); ++I) {
        if (I)
          Printed += ", ";
        const MCExpr *E = PI.Imms[I].Expr;
        if (!require(E != nullptr, "expected B.ARG selector"))
          return false;
        const auto *Sym = dyn_cast<MCSymbolRefExpr>(E);
        if (!require(Sym != nullptr, "B.ARG symbolic form expects named selectors"))
          return false;
        Printed += toUpperStr(Sym->getSymbol().getName());
      }

      std::string Expected = toUpperStr(AsmFmt.str());
      if (StringRef(Expected).starts_with("B.ARG "))
        Expected = Expected.substr(StringRef("B.ARG ").size());
      if (!require(StringRef(Printed).equals_insensitive(Expected),
                   "unexpected B.ARG field layout"))
        return false;
      return true;
    }

    if (!require(Form.field_count == 1 &&
                     StringRef(linxisa_fields[Form.field_start].name) == "format",
                 "unexpected B.ARG field layout"))
      return false;
    if (!require(PI.Imms.size() == 1 && PI.Imms[0].Expr,
                 "expected format selector"))
      return false;

    const MCExpr *E = PI.Imms[0].Expr;
    int64_t V = 0;
    if (isConstExpr(E, V)) {
      if (!require(V >= 0 && V <= 31, "format must fit 5 bits"))
        return false;
      emitFieldImm(V);
      return true;
    }

    auto *Sym = dyn_cast<MCSymbolRefExpr>(E);
    if (!require(Sym != nullptr,
                 "format must be an integer or known layout name"))
      return false;
    std::string Up = toUpperStr(Sym->getSymbol().getName());
    unsigned Fmt = 0;
    if (Up == "NORM.NORMAL")
      Fmt = 0;
    else if (Up == "ND2NZ.NORMAL")
      Fmt = 2;
    else if (Up == "ND2ZN.NORMAL")
      Fmt = 3;
    else if (Up == "DN2ZN.NORMAL")
      Fmt = 8;
    else if (Up == "DN2NZ.NORMAL")
      Fmt = 9;
    else if (Up == "NZ2DN.CANON")
      Fmt = 28;
    else if (Up == "V2V")
      Fmt = 0;
    else if (Up == "A2V")
      Fmt = 1;
    else if (Up == "VV")
      Fmt = 0;
    else if (Up == "VS")
      Fmt = 1;
    else if (Up == "SV")
      Fmt = 2;
    else
      return require(false,
                     "unknown B.ARG name (use format=<imm> for raw values)");

    emitFieldImm(Fmt);
    return true;
  }

  // Special-case: block argument registers (B.DIM).
  if (AsmFmt.starts_with("B.DIM")) {
    if (!require(!PI.Mem && !PI.SetRetTarget, "unexpected operands for B.DIM"))
      return false;
    if (!require(PI.Keywords.empty(),
                 "unexpected keyword operands for B.DIM"))
      return false;
    if (!require(PI.Regs.size() == 1, "expected RegSrc for B.DIM"))
      return false;
    if (!require(PI.Imms.size() == 1 && PI.Imms[0].Expr,
                 "expected uimm for B.DIM"))
      return false;
    if (!require(PI.ArrowDests.size() == 1,
                 "expected destination ->lbN for B.DIM"))
      return false;

    unsigned WantLb = PI.ArrowDests[0].Code & 0x3u;
    unsigned FormLb = 0;
    if (AsmFmt.contains("->LB1"))
      FormLb = 1;
    else if (AsmFmt.contains("->LB2"))
      FormLb = 2;
    if (!require(WantLb == FormLb, "B.DIM destination does not match encoding"))
      return false;

    int64_t Uimm = 0;
    if (!require(isConstExpr(PI.Imms[0].Expr, Uimm),
                 "B.DIM uimm must be constant for now"))
      return false;

    for (unsigned i = 0; i < Form.field_count; ++i) {
      StringRef FN(linxisa_fields[Form.field_start + i].name);
      if (FN == "RegSrc") {
        emitFieldImm(static_cast<int64_t>(PI.Regs[0].Code));
        continue;
      }
      if (FN.starts_with("uimm")) {
        emitFieldImm(Uimm);
        continue;
      }
      Err = ("unsupported B.DIM field: " + FN).str();
      return false;
    }
    return true;
  }

  // Special-case: GPR descriptor binding (B.IOR).
  if (AsmFmt.starts_with("B.IOR")) {
    if (!require(!PI.Mem && PI.ArrowDests.empty() && !PI.SetRetTarget,
                 "unexpected operands for B.IOR"))
      return false;
    if (!require(PI.Imms.empty(), "unexpected immediate operands for B.IOR"))
      return false;
    if (!require(PI.Keywords.empty(), "unexpected keyword operands for B.IOR"))
      return false;

    if (!require(PI.Regs.size() <= 4, "B.IOR expects up to 4 registers"))
      return false;

    // Canonical v0.4 disassembly prints sources as:
    //   B.IOR [RegSrc1, RegSrc0, RegSrc2],[RegDst]
    // with zeros omitted.
    unsigned RegSrc1 = 0;
    unsigned RegSrc0 = 0;
    unsigned RegSrc2 = 0;
    unsigned RegDst = 0;
    if (PI.Regs.size() >= 1)
      RegSrc1 = PI.Regs[0].Code;
    if (PI.Regs.size() >= 2)
      RegSrc0 = PI.Regs[1].Code;
    if (PI.Regs.size() >= 3)
      RegSrc2 = PI.Regs[2].Code;
    if (PI.Regs.size() >= 4)
      RegDst = PI.Regs[3].Code;

    emitFieldImm(RegDst & 0x1fu);  // RegDst
    emitFieldImm(RegSrc0 & 0x1fu); // RegSrc0 (stride)
    emitFieldImm(RegSrc1 & 0x1fu); // RegSrc1 (base)
    emitFieldImm(RegSrc2 & 0x1fu); // RegSrc2
    return true;
  }

  if (AsmFmt.starts_with("B.CATR") || AsmFmt.starts_with("B.DATR")) {
    if (!require(!PI.Mem && PI.Regs.empty() && PI.ArrowDests.empty() &&
                     !PI.SetRetTarget,
                 "unexpected operands for B.CATR"))
      return false;

    StringMap<StringRef> Attrs;
    unsigned CMode = 0;
    unsigned RMode = 0;
    auto recordAttrToken = [&](StringRef RawText) {
      StringRef Text = RawText;
      if (Text.starts_with("."))
        Text = Text.drop_front();
      auto Parts = Text.split('=');
      if (!Parts.second.empty()) {
        Attrs[Parts.first] = Parts.second;
        return;
      }
      if (Text.equals_insensitive("AQ") || Text.equals_insensitive("RL") ||
          Text.equals_insensitive("AQRL")) {
        Attrs["ORDER"] = Text;
        return;
      }
      if (Text.equals_insensitive("ATOMIC") || Text.equals_insensitive("TRAP") ||
          Text.equals_insensitive("DR") || Text.equals_insensitive("FAR") ||
          Text.equals_insensitive("SAT")) {
        Attrs[Text] = "ON";
        return;
      }
      if (Text.starts_with_insensitive("CMODE")) {
        unsigned Value = 0;
        if (!Text.drop_front(5).getAsInteger(0, Value) && Value < 8)
          CMode = Value;
        return;
      }
      if (Text.starts_with_insensitive("RMODE")) {
        unsigned Value = 0;
        if (!Text.drop_front(5).getAsInteger(0, Value) && Value < 8)
          RMode = Value;
        return;
      }
      if (Text.equals_insensitive("NORMAL") || Text.equals_insensitive("CANON")) {
        Attrs["LAYOUT"] = Text;
        return;
      }
      if (parseDataTypeKeyword(Text)) {
        Attrs["DTYPE"] = Text;
        return;
      }
      if (parsePadValueKeyword(Text)) {
        Attrs["PAD"] = Text;
        return;
      }
    };
    for (const ParsedKeyword &KW : PI.Keywords) {
      recordAttrToken(KW.TextUpper);
    }
    for (const ParsedImm &Imm : PI.Imms) {
      if (const auto *S = dyn_cast<MCSymbolRefExpr>(Imm.Expr))
        recordAttrToken(toUpperStr(S->getSymbol().getName()));
    }

    auto onBit = [&](StringRef Key) -> unsigned {
      auto It = Attrs.find(Key);
      return (It != Attrs.end() && It->second.equals_insensitive("ON")) ? 1u
                                                                        : 0u;
    };

    unsigned Aq = 0;
    unsigned Rl = 0;
    if (auto It = Attrs.find("ORDER"); It != Attrs.end()) {
      if (It->second.equals_insensitive("AQ") ||
          It->second.equals_insensitive("AQRL"))
        Aq = 1;
      if (It->second.equals_insensitive("RL") ||
          It->second.equals_insensitive("AQRL"))
        Rl = 1;
    }

    unsigned Layout = 0;
    if (auto It = Attrs.find("LAYOUT"); It != Attrs.end() &&
                                      It->second.equals_insensitive("NORMAL"))
      Layout = 1;

    unsigned DType = 0;
    if (auto It = Attrs.find("DTYPE"); It != Attrs.end()) {
      auto DT = parseDataTypeKeyword(It->second);
      if (!require(DT.has_value(), "invalid B.CATR dtype"))
        return false;
      DType = *DT;
    }

    unsigned Pad = 0;
    if (auto It = Attrs.find("PAD"); It != Attrs.end()) {
      auto P = parsePadValueKeyword(It->second);
      if (!require(P.has_value(), "invalid B.CATR pad value"))
        return false;
      Pad = *P;
    }

    for (unsigned i = 0; i < Form.field_count; ++i) {
      StringRef FN(linxisa_fields[Form.field_start + i].name);
      if (FN == "trap")
        emitFieldImm(onBit("TRAP"));
      else if (FN == "DR" || FN == "dr")
        emitFieldImm(onBit("DR"));
      else if (FN == "DataLayout")
        emitFieldImm(Layout);
      else if (FN == "DataType")
        emitFieldImm(DType);
      else if (FN == "PadValue")
        emitFieldImm(Pad);
      else if (FN == "CMode")
        emitFieldImm(CMode);
      else if (FN == "RMode")
        emitFieldImm(RMode);
      else if (FN == "Sat")
        emitFieldImm(onBit("SAT"));
      else if (FN == "aq")
        emitFieldImm(Aq);
      else if (FN == "atom" || FN == "atomic")
        emitFieldImm(onBit("ATOMIC"));
      else if (FN == "far")
        emitFieldImm(onBit("FAR"));
      else if (FN == "rl")
        emitFieldImm(Rl);
      else if (FN == "reserve")
        emitFieldImm(0);
      else
        return require(false, ("unsupported block attribute field: " + FN).str());
    }
    return true;
  }

  if (AsmFmt.starts_with("B.HINT ") && !AsmFmt.starts_with("B.HINT TRACE")) {
    if (!require(!PI.Mem && PI.Regs.empty() && PI.ArrowDests.empty() &&
                     !PI.SetRetTarget,
                 "unexpected operands for B.HINT"))
      return false;

    unsigned Likely = 1;
    unsigned Valid = 1;
    unsigned Temp = 0;
    int64_t PrefetchSize = 129;

    for (const ParsedKeyword &KW : PI.Keywords) {
      StringRef Text(KW.TextUpper);
      if (Text.equals_insensitive("BR.LIKELY")) {
        Likely = 1;
        Valid = 1;
      } else if (Text.equals_insensitive("BR.UNLIKELY")) {
        Likely = 0;
        Valid = 1;
      } else if (Text.equals_insensitive("TEMP.HOT")) {
        Temp = 0;
      } else if (Text.equals_insensitive("TEMP.WARM")) {
        Temp = 1;
      } else if (Text.equals_insensitive("TEMP.COOL")) {
        Temp = 2;
      } else if (Text.equals_insensitive("TEMP.NONE")) {
        Temp = 3;
      }
    }

    for (const ParsedImm &Imm : PI.Imms) {
      int64_t V = 0;
      if (!require(isConstExpr(Imm.Expr, V), "B.HINT PRFSIZE must be constant"))
        return false;
      PrefetchSize = V;
    }

    for (unsigned i = 0; i < Form.field_count; ++i) {
      StringRef FN(linxisa_fields[Form.field_start + i].name);
      if (FN == "L/UL")
        emitFieldImm(Likely);
      else if (FN == "V")
        emitFieldImm(Valid);
      else if (FN == "prefetch_size")
        emitFieldImm(PrefetchSize);
      else if (FN == "temp")
        emitFieldImm(Temp);
      else
        return require(false, ("unsupported B.HINT field: " + FN).str());
    }
    return true;
  }

  if (AsmFmt.starts_with("B.HINT TRACE")) {
    if (!require(!PI.Mem && PI.Regs.empty() && PI.ArrowDests.empty() &&
                     !PI.SetRetTarget,
                 "unexpected operands for B.HINT"))
      return false;
    if (!require(PI.Imms.size() == 1 && PI.Imms[0].Expr,
                 "expected TRACE.begin or TRACE.end"))
      return false;
    const auto *Sym = dyn_cast<MCSymbolRefExpr>(PI.Imms[0].Expr);
    if (!require(Sym != nullptr, "expected TRACE.begin or TRACE.end"))
      return false;
    StringRef Name = Sym->getSymbol().getName();
    unsigned Begin = 0;
    if (Name.equals_insensitive("TRACE.begin"))
      Begin = 1;
    else if (Name.equals_insensitive("TRACE.end"))
      Begin = 0;
    else
      return require(false, "expected TRACE.begin or TRACE.end");
    emitFieldImm(Begin);
    emitFieldImm(129);
    return true;
  }

  // Special-case: canonical v0.56.5 immediate-size B.IOT descriptors.
  if (AsmFmt.starts_with("B.IOT")) {
    if (!require(!PI.Mem && !PI.SetRetTarget,
                 "unexpected operands for B.IOT"))
      return false;
    if (!require(PI.Regs.empty(),
                 "unexpected register operands for B.IOT"))
      return false;

    bool WantLast = false;
    for (const ParsedKeyword &K : PI.Keywords)
      if (K.TextUpper == "LAST")
        WantLast = true;
    if (!require(PI.ArrowDests.size() == 1,
                 "expected tile destination suffix (for example '->t<1KB>')"))
      return false;

    const unsigned DstHand = PI.ArrowDests[0].Code & 0x7u;
    const unsigned DstTile = DstHand;
    const unsigned SizeCode = PI.ArrowDests[0].AngleSize;
    if (!require(SizeCode <= 15u, "B.IOT size code must fit canonical imm4"))
      return false;
    if (!require(PI.ArrowDests[0].HasAngleSize &&
                     !PI.ArrowDests[0].HasAngleReg,
                 "B.IOT expects size suffix '->t<Size>'"))
      return false;

    if (!require(PI.Imms.size() <= 2,
                 "B.IOT supports at most 2 SrcTile operands"))
      return false;

    const unsigned ExpectedSources = AsmFmt.contains("SrcTile1")
                                         ? 2u
                                     : AsmFmt.contains("SrcTile0") ? 1u : 0u;
    if (!require(PI.Imms.size() == ExpectedSources,
                 "source tile count does not match B.IOT encoding form"))
      return false;

    unsigned S0R = 0, S1R = 0;
    unsigned Src0 = 0, Src1 = 0;
    auto takeTileImm = [&](unsigned i, unsigned &Tile,
                           unsigned &Reuse) -> bool {
      int64_t V = 0;
      if (!isConstExpr(PI.Imms[i].Expr, V))
        return false;
      Tile = static_cast<unsigned>(V) & 0x3fu;
      Reuse = (static_cast<unsigned>(V) >> 6) & 0x1u;
      return true;
    };

    if (PI.Imms.size() >= 1) {
      unsigned Reuse = 0;
      if (!require(takeTileImm(0, Src0, Reuse),
                   "tile refs must be constant in bring-up"))
        return false;
      S0R = Reuse & 1u;
    }
    if (PI.Imms.size() >= 2) {
      unsigned Reuse = 0;
      if (!require(takeTileImm(1, Src1, Reuse),
                   "tile refs must be constant in bring-up"))
        return false;
      S1R = Reuse & 1u;
    }

    for (unsigned i = 0; i < Form.field_count; ++i) {
      const linxisa_field &Field = linxisa_fields[Form.field_start + i];
      StringRef FN(Field.name);
      if (FN == "DstTile")
        emitFieldImm(DstTile);
      else if (FN == "L")
        emitFieldImm(WantLast);
      else if (FN == "S0R")
        emitFieldImm(S0R);
      else if (FN == "S1R")
        emitFieldImm(S1R);
      else if (FN == "SrcTile0")
        emitFieldImm(Src0);
      else if (FN == "SrcTile1")
        emitFieldImm(Src1);
      else if (FN == "imm4")
        emitFieldImm(SizeCode);
      else
        return require(false,
                       ("unsupported B.IOT field: " + FN).str());
    }

    return true;
  }

  // Non-memory ops: map register sources in common order and immediates in
  // field order. SrcRType/shamt are treated as attributes of SrcR when present.
  if (!require(!PI.Mem.has_value(), "unexpected memory operand"))
    return false;

  // Tile block headers are canonically "<selector>, DataType" in v0.4:
  // Function for TMA/CUBE, TileOpcode for TEPL.
  //
  // The encoded field order remains DataType/Function|TileOpcode, so parse in
  // canonical source order here and emit in field order below.
  const bool IsBStartTMA = AsmFmt.starts_with("BSTART.TMA");
  const bool IsBStartCUBE = AsmFmt.starts_with("BSTART.CUBE");
  const bool IsBStartTEPL = AsmFmt.starts_with("BSTART.TEPL");
  const bool IsBStartFIXP = AsmFmt.starts_with("BSTART.FIXP");
  if (IsBStartTMA || IsBStartCUBE || IsBStartTEPL || IsBStartFIXP) {
    const char *Kind = IsBStartTMA
                           ? "BSTART.TMA"
                           : (IsBStartCUBE ? "BSTART.CUBE"
                                           : (IsBStartTEPL ? "BSTART.TEPL"
                                                           : "BSTART.FIXP"));
    if (!require(PI.Regs.empty() && PI.Keywords.empty() && PI.ArrowDests.empty() &&
                     !PI.SetRetTarget,
                 (Twine("unexpected operands for ") + Kind)))
      return false;
    const char *Selector = (IsBStartTEPL || IsBStartFIXP) ? "TileOpcode"
                                                          : "Function";
    if (!require(
            PI.Imms.size() == 2,
            (Twine("expected operands '") + Selector + ", DataType' for " + Kind)))
      return false;

    auto decodeNamedImm = [&](const MCExpr *E, StringRef FieldName)
        -> std::optional<int64_t> {
      int64_t V = 0;
      if (isConstExpr(E, V))
        return V;
      if (const auto *S = dyn_cast<MCSymbolRefExpr>(E)) {
        StringRef Sym = S->getSymbol().getName();
        if (FieldName == "DataType") {
          if (auto DT = parseDataTypeKeyword(Sym))
            return static_cast<int64_t>(*DT);
          return std::nullopt;
        }
        if (FieldName == "Function") {
          if (IsBStartTMA) {
            if (auto Fn = parseTMAFunctionKeyword(Sym))
              return static_cast<int64_t>(*Fn);
          } else if (IsBStartCUBE) {
            if (auto Fn = parseCubeFunctionKeyword(Sym))
              return static_cast<int64_t>(*Fn);
          } else if (IsBStartTEPL || IsBStartFIXP) {
            if (auto Op = parseTEPLTileOpcodeKeyword(Sym))
              return static_cast<int64_t>(*Op);
          }
          return std::nullopt;
        }
        if (FieldName == "TileOpcode") {
          if (auto Op = parseTEPLTileOpcodeKeyword(Sym))
            return static_cast<int64_t>(*Op);
          return std::nullopt;
        }
      }
      return std::nullopt;
    };

    std::optional<int64_t> FuncVal =
        decodeNamedImm(PI.Imms[0].Expr, IsBStartTEPL ? "TileOpcode" : "Function");
    std::optional<int64_t> DataTypeVal =
        decodeNamedImm(PI.Imms[1].Expr, "DataType");

    if (IsBStartTMA) {
      if (!require(FuncVal.has_value(),
                   "Function must be a constant or one of {TLOAD,TSTORE,TMOV}"))
        return false;
    } else if (IsBStartCUBE) {
      if (!require(FuncVal.has_value(),
                   "Function must be a constant or one of "
                   "{MAMULB,TMATMUL,MAMULB.ACC,TMATMUL.ACC,ACCCVT}"))
        return false;
    } else {
      if (!require(FuncVal.has_value(),
                   "TileOpcode must be a constant or one of "
                   "{TADD,TSUB,TMUL,TDIV,TMAX,TMIN,TROWMAX,TROWMIN,TROWSUM,"
                   "TCOLMAX,TCOLMIN,TCOLSUM,TADDS,...}"))
        return false;
    }
    if (!require(DataTypeVal.has_value(),
                 "DataType must be a constant or one of "
                 "{FP64,FP32,FP16,FP8,BF16,FPL8,FP4,FPL4,"
                 "INT64,INT32,INT16,INT8,INT4,"
                 "UINT64,UINT32,UINT16,UINT8,UINT4}"))
      return false;
    if (IsBStartTMA)
      if (!require(*FuncVal >= 0 && *FuncVal <= 2,
                   "BSTART.TMA Function must be in range 0..2 in canonical "
                   "v0.4"))
        return false;
    if (IsBStartTEPL)
      if (!require(*FuncVal >= 0 && *FuncVal <= 0x3ff,
                   "BSTART.TEPL TileOpcode must be in range 0..1023"))
        return false;
    if (IsBStartFIXP)
      if (!require(*FuncVal >= 0 && *FuncVal <= 0x3ff,
                   "BSTART.FIXP selector must be in range 0..1023"))
        return false;
    if (!require(*DataTypeVal >= 0 && *DataTypeVal <= 31,
                 (Twine(Kind) + " DataType out of range")))
      return false;

    for (unsigned i = 0; i < Form.field_count; ++i) {
      const linxisa_field &Field = linxisa_fields[Form.field_start + i];
      StringRef FN(Field.name);
      if (FN == "DataType") {
        emitFieldImm(*DataTypeVal);
        continue;
      }
      if (FN == "Function") {
        emitFieldImm(*FuncVal);
        continue;
      }
      if (FN == "TileOpcode") {
        emitFieldImm(*FuncVal);
        continue;
      }
      Err = (Twine("unsupported ") + Kind + " field: " + FN).str();
      return false;
    }
    return true;
  }

  unsigned RegIdx = 0;
  unsigned ImmIdx = 0;
  std::optional<int64_t> SplitImm12;
  std::optional<ParsedReg> SrcROp;
  std::optional<ParsedReg> ForcedSrcL;
  std::optional<ParsedReg> ForcedSrcR;
  std::optional<ParsedReg> ForcedSrcD;
  std::optional<ParsedReg> ForcedSrcP;
  std::optional<ParsedReg> ForcedSrcA;

  auto takeReg = [&]() -> std::optional<ParsedReg> {
    if (RegIdx >= PI.Regs.size())
      return std::nullopt;
    return PI.Regs[RegIdx++];
  };

  auto takeImmExpr = [&]() -> const MCExpr * {
    if (ImmIdx >= PI.Imms.size())
      return nullptr;
    return PI.Imms[ImmIdx++].Expr;
  };

  auto bindFieldRegOrder =
      [&](std::initializer_list<std::pair<StringRef, std::optional<ParsedReg> *>>
              Bindings) -> bool {
    for (const auto &Binding : Bindings) {
      auto R = takeReg();
      if (!require(R.has_value(),
                   ("missing register operand for " + Binding.first).str()))
        return false;
      *Binding.second = *R;
    }
    return true;
  };

  if (!PI.Mem && !PI.SetRetTarget) {
    if ((AsmFmt.starts_with_insensitive("csel") ||
         AsmFmt.starts_with_insensitive("v.csel")) &&
        hasField(Form, "SrcP") && hasField(Form, "SrcL") &&
        hasField(Form, "SrcR")) {
      if (!bindFieldRegOrder({{"SrcP", &ForcedSrcP},
                              {"SrcL", &ForcedSrcL},
                              {"SrcR", &ForcedSrcR}}))
        return false;
    } else if ((AsmFmt.starts_with_insensitive("madd") ||
                AsmFmt.starts_with_insensitive("maddw") ||
                AsmFmt.starts_with_insensitive("hl.madd") ||
                AsmFmt.starts_with_insensitive("hl.maddw") ||
                AsmFmt.starts_with_insensitive("v.madd")) &&
               hasField(Form, "SrcD") && hasField(Form, "SrcL") &&
               hasField(Form, "SrcR")) {
      if (!bindFieldRegOrder({{"SrcL", &ForcedSrcL},
                              {"SrcR", &ForcedSrcR},
                              {"SrcD", &ForcedSrcD}}))
        return false;
    } else if ((AsmFmt.starts_with_insensitive("fmadd") ||
                AsmFmt.starts_with_insensitive("fmsub") ||
                AsmFmt.starts_with_insensitive("fnmadd") ||
                AsmFmt.starts_with_insensitive("fnmsub") ||
                AsmFmt.starts_with_insensitive("v.fmadd") ||
                AsmFmt.starts_with_insensitive("v.fmsub") ||
                AsmFmt.starts_with_insensitive("v.fnmadd") ||
                AsmFmt.starts_with_insensitive("v.fnmsub")) &&
               hasField(Form, "SrcA") && hasField(Form, "SrcL") &&
               hasField(Form, "SrcR")) {
      if (!bindFieldRegOrder({{"SrcL", &ForcedSrcL},
                              {"SrcR", &ForcedSrcR},
                              {"SrcA", &ForcedSrcA}}))
        return false;
    } else if ((AsmFmt.starts_with_insensitive("v.shfl.down") ||
                AsmFmt.starts_with_insensitive("v.shfl.idx") ||
                AsmFmt.starts_with_insensitive("v.shfl.up")) &&
               hasField(Form, "SrcP") && hasField(Form, "SrcL") &&
               hasField(Form, "SrcR")) {
      if (!bindFieldRegOrder({{"SrcL", &ForcedSrcL},
                              {"SrcR", &ForcedSrcR},
                              {"SrcP", &ForcedSrcP}}))
        return false;
    }
  }

  for (unsigned i = 0; i < Form.field_count; ++i) {
    const linxisa_field &Field = linxisa_fields[Form.field_start + i];
    StringRef FN(Field.name);

    if (FN == "RegDst") {
      if (!require(PI.ArrowDests.size() == 1, "expected destination after '->'"))
        return false;
      if (!require(PI.ArrowDests[0].Code < (1u << Field.bit_width),
                   "destination register does not fit field width"))
        return false;
      emitFieldImm(static_cast<int64_t>(PI.ArrowDests[0].Code));
      continue;
    }

    if (FN == "RegDst0" || FN == "RegDst1") {
      const unsigned Index = (FN == "RegDst0") ? 0u : 1u;
      if (!require(PI.ArrowDests.size() > Index,
                   "missing destination register after '->'"))
        return false;
      if (!require(PI.ArrowDests[Index].Code < (1u << Field.bit_width),
                   "destination register does not fit field width"))
        return false;
      emitFieldImm(static_cast<int64_t>(PI.ArrowDests[Index].Code));
      continue;
    }

    if (FN == "RegSrc" || FN == "RegSrc0" || FN == "RegSrc1" || FN == "RegSrc2" ||
        FN.starts_with("RegSrc0") || FN.starts_with("RegSrc1") ||
        FN.starts_with("RegSrc2")) {
      auto R = takeReg();
      if (!require(R.has_value(), "missing register operand"))
        return false;
      if (!require(R->Code < (1u << Field.bit_width),
                   "register operand does not fit field width"))
        return false;
      emitFieldImm(static_cast<int64_t>(R->Code));
      continue;
    }

    if (FN == "SrcBegin" || FN == "SrcEnd" || FN == "DstBegin" ||
        FN == "DstEnd") {
      auto R = takeReg();
      if (!require(R.has_value(), "missing register range operand"))
        return false;
      if (!require(R->Code < (1u << Field.bit_width),
                   "register range operand does not fit field width"))
        return false;
      emitFieldImm(static_cast<int64_t>(R->Code));
      continue;
    }

    if (FN == "LoopNest") {
      if (!require(PI.ArrowDests.size() == 1, "expected destination after '->'"))
        return false;
      if (!require(PI.ArrowDests[0].Code < (1u << Field.bit_width),
                   "loopnest selector does not fit field width"))
        return false;
      emitFieldImm(static_cast<int64_t>(PI.ArrowDests[0].Code));
      continue;
    }

    if (FN == "SrcL" || FN == "SrcD" || FN == "SrcP" || FN == "SrcA") {
      std::optional<ParsedReg> R;
      if (FN == "SrcL")
        R = ForcedSrcL ? ForcedSrcL : takeReg();
      else if (FN == "SrcD")
        R = ForcedSrcD ? ForcedSrcD : takeReg();
      else if (FN == "SrcP")
        R = ForcedSrcP ? ForcedSrcP : takeReg();
      else
        R = ForcedSrcA ? ForcedSrcA : takeReg();
      if (!require(R.has_value(), "missing register operand"))
        return false;
      if (!require(R->Code < (1u << Field.bit_width),
                   "register operand does not fit field width"))
        return false;
      emitFieldImm(static_cast<int64_t>(R->Code));
      continue;
    }

    if (FN == "SrcR") {
      auto R = ForcedSrcR ? ForcedSrcR : takeReg();
      if (!require(R.has_value(), "missing SrcR operand"))
        return false;
      SrcROp = *R;
      if (!require(R->Code < (1u << Field.bit_width),
                   "SrcR register does not fit field width"))
        return false;
      emitFieldImm(static_cast<int64_t>(R->Code));
      continue;
    }

    if (FN == "SrcRType") {
      if (!require(SrcROp.has_value(), "missing SrcR for SrcRType"))
        return false;
      emitFieldImm(static_cast<int64_t>(SrcROp->SrcRType));
      continue;
    }

    if (FN == "shamt") {
      const bool IsSetcImm = AsmFmt.starts_with_insensitive("setc.") &&
                             (hasField(Form, "simm12") || hasField(Form, "uimm12"));
      if (IsSetcImm) {
        if (!SplitImm12.has_value()) {
          const MCExpr *E = takeImmExpr();
          if (!require(E != nullptr, "missing immediate operand"))
            return false;
          int64_t V = 0;
          if (!require(isConstExpr(E, V), "immediate must be a constant for now"))
            return false;
          SplitImm12 = V;
        }
        uint64_t Bits = static_cast<uint64_t>(*SplitImm12);
        unsigned Shamt = 0;
        while (Shamt < 31u && (Bits & 1u) == 0u && Bits > 0u) {
          Bits >>= 1;
          ++Shamt;
        }
        emitFieldImm(Shamt);
        continue;
      }
      if (hasField(Form, "SrcR") && AsmFmt.contains("<<<shamt>")) {
        // Shift attached to SrcR operand.
        if (SrcROp.has_value())
          emitFieldImm(static_cast<int64_t>(SrcROp->Shamt));
        else
          emitFieldImm(0);
        continue;
      }
      const MCExpr *E = takeImmExpr();
      if (!require(E != nullptr, "missing shamt immediate"))
        return false;
      int64_t V = 0;
      if (!require(isConstExpr(E, V), "shamt must be a constant"))
        return false;
      emitFieldImm(V);
      continue;
    }

    if (FN == "SSR_ID" || FN == "SSRID") {
      const MCExpr *E = takeImmExpr();
      if (!require(E != nullptr, "missing SSR ID immediate"))
        return false;
      int64_t V = 0;
      if (!require(isConstExpr(E, V), "SSR ID must be a constant for now"))
        return false;
      emitFieldImm(V);
      continue;
    }

    if (FN == "SrcType" || FN == "DstType") {
      const MCExpr *E = takeImmExpr();
      if (!require(E != nullptr, ("missing " + FN + " immediate").str()))
        return false;
      int64_t V = 0;
      if (!require(isConstExpr(E, V), (FN + " must be a constant for now").str()))
        return false;
      emitFieldImm(V);
      continue;
    }

    if (FN == "LSR_ID" || FN == "ACR-ID" || FN == "C-ID" ||
        FN == "CROSS-BID" || FN == "M" || FN == "N" || FN == "model") {
      const MCExpr *E = takeImmExpr();
      if (!require(E != nullptr, ("missing " + FN + " immediate").str()))
        return false;
      int64_t V = 0;
      if (!require(isConstExpr(E, V), (FN + " must be a constant for now").str()))
        return false;
      emitFieldImm(V);
      continue;
    }

    if (FN == "e" || FN == "h" || FN == "i" || FN == "r" || FN == "s") {
      const MCExpr *E = takeImmExpr();
      if (!require(E != nullptr, ("missing " + FN + " immediate").str()))
        return false;
      int64_t V = 0;
      if (!require(isConstExpr(E, V), (FN + " must be a constant for now").str()))
        return false;
      emitFieldImm(V);
      continue;
    }

    if (FN == "SrcZero") {
      emitFieldImm((AsmFmt.starts_with_insensitive("jr ") ||
                    AsmFmt.starts_with_insensitive("lr.") ||
                    AsmFmt.starts_with_insensitive("v.psel"))
                       ? 1
                       : 0);
      continue;
    }

    if (FN == "aq") {
      emitFieldImm(static_cast<int64_t>(PI.AtomicAq & 1u));
      continue;
    }

    if (FN == "rl") {
      emitFieldImm(static_cast<int64_t>(PI.AtomicRl & 1u));
      continue;
    }

    if (FN == "far") {
      emitFieldImm(static_cast<int64_t>(PI.AtomicFar & 1u));
      continue;
    }

    if (FN == "rd") {
      emitFieldImm(static_cast<int64_t>(PI.AtomicRd & 1u));
      continue;
    }

    // Block header fields (bring-up subset).
    //
    // The spec tables name these symbolically; for bring-up, accept numeric
    // immediates (keywords like dt0/tload can be added later).
    if (FN == "DataType" || FN == "Function" || FN == "Mode" ||
        FN == "TileOpcode" || FN == "TileOp") {
      const MCExpr *E = takeImmExpr();
      if (E == nullptr) {
        if (FN == "Mode" &&
            (AsmFmt.starts_with_insensitive("BSTART.VPAR") ||
             AsmFmt.starts_with_insensitive("BSTART.VSEQ"))) {
          emitFieldImm(1);
          continue;
        }
        if (!require(false, ("missing " + FN + " immediate").str()))
          return false;
      }
      int64_t V = 0;
      if (!isConstExpr(E, V)) {
        if (FN == "Mode") {
          if (const auto *S = dyn_cast<MCSymbolRefExpr>(E))
            if (auto Mode = parseVectorBlockModeKeyword(S->getSymbol().getName())) {
              emitFieldImm(*Mode);
              continue;
            }
        }
        if (!require(false, (FN + " must be a constant for now").str()))
          return false;
      }
      if (FN == "Function" && AsmFmt.starts_with_insensitive("BSTART.FIXP") &&
          V >= 0 && V <= 0x3ff) {
        emitFieldImm(V);
        continue;
      }
      emitFieldImm(V);
      continue;
    }

    if (FN == "RST_Type" || FN == "RRA_Type") {
      const MCExpr *E = takeImmExpr();
      if (E == nullptr) {
        if ((FN == "RST_Type" && AsmFmt.equals_insensitive("acrc rst_type")) ||
            (FN == "RRA_Type" && AsmFmt.equals_insensitive("acre rra_type"))) {
          emitFieldImm(1);
          continue;
        }
        if (!require(false, ("missing " + FN + " immediate").str()))
          return false;
      }
      int64_t V = 0;
      if (!require(isConstExpr(E, V), (FN + " must be a constant for now").str()))
        return false;
      emitFieldImm(V);
      continue;
    }

    if (FN == "PRED_IMM" || FN == "SUCC_IMM") {
      const MCExpr *E = takeImmExpr();
      if (E == nullptr && AsmFmt.equals_insensitive("fence.d pred_imm, succ_imm")) {
        emitFieldImm(1);
        continue;
      }
      if (!require(E != nullptr, ("missing " + FN + " immediate").str()))
        return false;
      int64_t V = 0;
      if (!isConstExpr(E, V)) {
        if (const auto *S = dyn_cast<MCSymbolRefExpr>(E)) {
          auto FenceMask = parseFenceImmKeyword(S->getSymbol().getName());
          if (FenceMask) {
            emitFieldImm(*FenceMask);
            continue;
          }
        }
        if (!require(false, (FN + " must be a constant for now").str()))
          return false;
      }
      emitFieldImm(V);
      continue;
    }

    if (FN == "reserve") {
      if (AsmFmt.equals_insensitive("BSTART.FP RET") ||
          AsmFmt.equals_insensitive("BSTART.FP IND") ||
          AsmFmt.equals_insensitive("BSTART.FP ICALL") ||
          AsmFmt.equals_insensitive("BSTART.STD RET") ||
          AsmFmt.equals_insensitive("BSTART.STD IND") ||
          AsmFmt.equals_insensitive("BSTART.STD ICALL")) {
        emitFieldImm(129);
        continue;
      }
      emitFieldImm(0);
      continue;
    }

    if ((FN == "imml" || FN == "imms") && hasField(Form, "imml") &&
        hasField(Form, "imms")) {
      if (!SplitImm12.has_value()) {
        const MCExpr *E = takeImmExpr();
        if (!require(E != nullptr, "missing immediate operand"))
          return false;
        int64_t V = 0;
        if (!require(isConstExpr(E, V), "immediate must be a constant for now"))
          return false;
        SplitImm12 = V;
      }
      const uint64_t Bits = static_cast<uint64_t>(*SplitImm12) & 0xfffu;
      emitFieldImm(FN == "imml" ? static_cast<int64_t>(Bits & 0x3fu)
                                : static_cast<int64_t>((Bits >> 6) & 0x3fu));
      continue;
    }

    if ((FN == "simm12" || FN == "uimm12") &&
        AsmFmt.starts_with_insensitive("setc.") && hasField(Form, "shamt")) {
      if (!SplitImm12.has_value()) {
        const MCExpr *E = takeImmExpr();
        if (!require(E != nullptr, "missing immediate operand"))
          return false;
        int64_t V = 0;
        if (!require(isConstExpr(E, V), "immediate must be a constant for now"))
          return false;
        SplitImm12 = V;
      }
      uint64_t Bits = static_cast<uint64_t>(*SplitImm12);
      unsigned Shamt = 0;
      while (Shamt < 31u && (Bits & 1u) == 0u && Bits > 0u) {
        Bits >>= 1;
        ++Shamt;
      }
      emitFieldImm(static_cast<int64_t>(Bits & 0xfffu));
      continue;
    }

    if (FN.starts_with("simm") || FN.starts_with("uimm") || FN.starts_with("imm")) {
      const MCExpr *E = takeImmExpr();
      if (!require(E != nullptr, "missing immediate operand"))
        return false;
      int64_t V = 0;
      if (isConstExpr(E, V))
        emitFieldImm(V);
      else
        emitFieldExpr(E);
      continue;
    }

    Err = ("unsupported field: " + FN).str();
    return false;
  }

  // Require full consumption of operands for a stable syntax.
  if (!require(RegIdx == PI.Regs.size(), "too many register operands"))
    return false;
  if (!require(ImmIdx == PI.Imms.size(), "too many immediate operands"))
    return false;
  if (!require(PI.Keywords.empty(), "unexpected keyword operands"))
    return false;

  return true;
}

bool LinxISAAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                               OperandVector &Operands,
                                               MCStreamer &Out,
                                               uint64_t &ErrorInfo,
                                               bool MatchingInlineAsm) {
  (void)Opcode;
  (void)ErrorInfo;
  (void)MatchingInlineAsm;

  if (Operands.empty())
    return Error(IDLoc, "empty instruction");

  auto *TokOp = static_cast<LinxOperand *>(Operands[0].get());
  StringRef Mnemonic = TokOp->getToken();
  if (auto LegacyDiag = getLegacyAliasDiag(Mnemonic))
    return Error(IDLoc, *LegacyDiag);

  std::string Key = toUpperStr(Mnemonic);
  unsigned LocalBit = 0;
  if (StringRef(Key).ends_with(".LOCAL")) {
    LocalBit = 1;
    Key.resize(Key.size() - StringRef(".LOCAL").size());
  }

  std::optional<TileBlockAlias> TileAlias = parseTileBlockAliasMnemonic(Key);
  if (TileAlias)
    Key = TileAlias->CanonicalMnemonic;

  const auto &Map = getMnemonicMap();
  SmallVector<ParsedImm, 2> SynthesizedImms;

  auto tryResolveScalarFpAlias =
      [&](StringRef KeyRef) -> std::optional<std::string> {
    const auto Dot = KeyRef.find('.');
    if (Dot == StringRef::npos)
      return std::nullopt;
    const StringRef Base = KeyRef.substr(0, Dot);
    const StringRef Suffix = KeyRef.substr(Dot + 1);

    auto pushImm = [&](unsigned Value) {
      ParsedImm I;
      I.Expr = MCConstantExpr::create(Value, getContext());
      I.Loc = IDLoc;
      SynthesizedImms.push_back(I);
    };

    auto pushFpUnary = [&]() -> std::optional<std::string> {
      auto SrcType = parseScalarFpTypeSuffix(Suffix);
      if (!SrcType)
        return std::nullopt;
      pushImm(*SrcType);
      return Base.str();
    };

    if (Base == "FABS" || Base == "FADD" || Base == "FDIV" ||
        Base == "FEQ" || Base == "FEQS" || Base == "FEXP" ||
        Base == "FGE" || Base == "FGES" || Base == "FLT" ||
        Base == "FLTS" || Base == "FMADD" || Base == "FMAX" ||
        Base == "FMIN" || Base == "FMSUB" || Base == "FMUL" ||
        Base == "FNE" || Base == "FNES" || Base == "FNMADD" ||
        Base == "FNMSUB" || Base == "FRECIP" || Base == "FSQRT" ||
        Base == "FSUB")
      return pushFpUnary();

    const auto Sep = Suffix.find("2");
    if (Sep == StringRef::npos)
      return std::nullopt;
    const StringRef SrcSuffix = Suffix.substr(0, Sep);
    const StringRef DstSuffix = Suffix.substr(Sep + 1);

    if (Base == "FCVT" || Base == "FCVTA" || Base == "FCVTM" ||
        Base == "FCVTN" || Base == "FCVTP") {
      auto SrcType = parseScalarFpTypeSuffix(SrcSuffix);
      auto DstType = parseScalarFpTypeSuffix(DstSuffix);
      if (!SrcType || !DstType)
        return std::nullopt;
      pushImm(*SrcType);
      pushImm(*DstType);
      return Base.str();
    }

    if (Base == "FCVTZ") {
      auto SrcType = parseScalarFpTypeSuffix(SrcSuffix);
      auto DstType = parseScalarIntDstTypeSuffix(DstSuffix);
      if (!SrcType || !DstType)
        return std::nullopt;
      pushImm(*SrcType);
      pushImm(*DstType);
      return Base.str();
    }

    if (Base == "SCVTF" || Base == "UCVTF") {
      auto SrcType = parseScalarIntSrcTypeSuffix(SrcSuffix, Base == "SCVTF");
      auto DstType = parseScalarFpTypeSuffix(DstSuffix);
      if (!SrcType || !DstType)
        return std::nullopt;
      pushImm(*SrcType);
      pushImm(*DstType);
      return Base.str();
    }

    return std::nullopt;
  };

  auto tryResolveExtendedAlias =
      [&](StringRef KeyRef) -> std::optional<std::string> {
    auto pushImm = [&](unsigned Value) {
      ParsedImm I;
      I.Expr = MCConstantExpr::create(Value, getContext());
      I.Loc = IDLoc;
      SynthesizedImms.push_back(I);
    };

    auto tryResolvePrefetchAlias =
        [&](StringRef Prefix) -> std::optional<std::string> {
      if (!KeyRef.starts_with_insensitive(Prefix))
        return std::nullopt;
      auto Model = parsePrefetchModelSuffix(KeyRef.drop_front(Prefix.size()));
      if (!Model)
        return std::nullopt;
      pushImm(*Model);
      return Prefix.drop_back().str();
    };

    if (auto Canonical = tryResolvePrefetchAlias("HL.PRF."))
      return Canonical;
    if (auto Canonical = tryResolvePrefetchAlias("HL.PRF.A."))
      return Canonical;
    if (auto Canonical = tryResolvePrefetchAlias("HL.PRFI.U."))
      return Canonical;
    if (auto Canonical = tryResolvePrefetchAlias("HL.PRFI.UA."))
      return Canonical;

    auto tryResolveQueueAlias =
        [&](StringRef Prefix, bool AllowH, bool HasI, bool HasS,
            bool HasH) -> std::optional<std::string> {
      if (!KeyRef.starts_with_insensitive(Prefix))
        return std::nullopt;
      unsigned E = 0, H = 0, I = 0, R = 0, S = 0;
      if (!parseQueueFlagSuffix(KeyRef.drop_front(Prefix.size()), AllowH, E, H,
                                I, R, S))
        return std::nullopt;
      pushImm(E);
      if (HasH)
        pushImm(H);
      if (HasI)
        pushImm(I);
      pushImm(R);
      if (HasS)
        pushImm(S);
      return Prefix.drop_back().str();
    };

    if (auto Canonical = tryResolveQueueAlias("HL.QMT.", /*AllowH=*/false,
                                              /*HasI=*/true, /*HasS=*/true,
                                              /*HasH=*/false))
      return Canonical;
    if (auto Canonical = tryResolveQueueAlias("HL.QPOP.", /*AllowH=*/false,
                                              /*HasI=*/false, /*HasS=*/false,
                                              /*HasH=*/false))
      return Canonical;
    if (auto Canonical = tryResolveQueueAlias("HL.QPUSH.", /*AllowH=*/true,
                                              /*HasI=*/false, /*HasS=*/false,
                                              /*HasH=*/true))
      return Canonical;

    auto tryResolveVectorCvtAlias =
        [&](StringRef Prefix, auto ParseSrc, auto ParseDst)
            -> std::optional<std::string> {
      if (!KeyRef.starts_with_insensitive(Prefix))
        return std::nullopt;
      StringRef Suffix = KeyRef.drop_front(Prefix.size());
      const auto Sep = Suffix.find("2");
      if (Sep == StringRef::npos)
        return std::nullopt;
      auto SrcType = ParseSrc(Suffix.substr(0, Sep));
      auto DstType = ParseDst(Suffix.substr(Sep + 1));
      if (!SrcType || !DstType)
        return std::nullopt;
      pushImm(*SrcType);
      pushImm(*DstType);
      return Prefix.drop_back().str();
    };

    if (auto Canonical = tryResolveVectorCvtAlias(
            "V.FCVT.", parseScalarFpTypeSuffix, parseScalarFpTypeSuffix))
      return Canonical;
    if (auto Canonical = tryResolveVectorCvtAlias(
            "V.FCVTI.", parseScalarFpTypeSuffix, parseScalarIntDstTypeSuffix))
      return Canonical;
    if (auto Canonical = tryResolveVectorCvtAlias(
            "V.ICVT.",
            [&](StringRef Suffix) {
              return parseScalarIntSrcTypeSuffix(Suffix, /*Signed=*/true);
            },
            parseScalarIntDstTypeSuffix))
      return Canonical;
    if (auto Canonical = tryResolveVectorCvtAlias(
            "V.ICVTF.",
            [&](StringRef Suffix) {
              return parseScalarIntSrcTypeSuffix(Suffix, /*Signed=*/true);
            },
            parseScalarFpTypeSuffix))
      return Canonical;

    return std::nullopt;
  };

  unsigned AtomicAq = 0;
  unsigned AtomicFar = 0;
  unsigned AtomicRl = 0;
  unsigned AtomicRd = 0;
  auto It = Map.find(Key);
  if (It == Map.end()) {
    if (auto CanonicalKey = tryResolveScalarFpAlias(Key)) {
      Key = *CanonicalKey;
      It = Map.find(Key);
    }
  }
  if (It == Map.end()) {
    if (auto CanonicalKey = tryResolveExtendedAlias(Key)) {
      Key = *CanonicalKey;
      It = Map.find(Key);
    }
  }
  if (It == Map.end()) {
    const size_t Dot = Key.rfind('.');
    if (Dot != std::string::npos) {
      std::string BaseKey = Key.substr(0, Dot);
      StringRef Suffix = StringRef(Key).substr(Dot + 1);
      if (decodeAtomicMnemonicSuffix(Suffix, AtomicAq, AtomicFar, AtomicRl,
                                     AtomicRd)) {
        It = Map.find(BaseKey);
        if (It != Map.end())
          Key = BaseKey;
      }
    }
  }
  if (It == Map.end())
    return Error(IDLoc, ("unrecognized instruction '" + Mnemonic + "'").str());

  ParsedInst PI;
  buildParsedInst(Operands, PI);
  PI.LocalBit = LocalBit;
  PI.AtomicAq = AtomicAq;
  PI.AtomicFar = AtomicFar;
  PI.AtomicRl = AtomicRl;
  PI.AtomicRd = AtomicRd;
  for (const ParsedImm &I : SynthesizedImms)
    PI.Imms.push_back(I);

  if (TileAlias) {
    if (!PI.Regs.empty() || !PI.Keywords.empty() || PI.Mem.has_value() ||
        !PI.ArrowDests.empty() || PI.SetRetTarget.has_value())
      return Error(IDLoc, "tile alias expects only a DataType operand");
    if (PI.Imms.size() != 1)
      return Error(IDLoc, "tile alias expects exactly one DataType operand");

    ParsedImm FuncImm;
    FuncImm.Expr = MCConstantExpr::create(TileAlias->OpSel, getContext());
    FuncImm.Loc = IDLoc;
    ParsedImm DataTypeImm = PI.Imms[0];

    PI.Imms.clear();
    PI.Imms.push_back(FuncImm);
    PI.Imms.push_back(DataTypeImm);
  }

  // Fused syntax: `BSTART CALL, <target>, ra=<return>`.
  if (PI.SetRetTarget.has_value()) {
    StringRef KeyRef(Key);
    const bool IsBStart =
        KeyRef == "BSTART" || KeyRef.starts_with("BSTART.") ||
        KeyRef == "C.BSTART" || KeyRef.starts_with("C.BSTART.") ||
        KeyRef == "HL.BSTART" || KeyRef.starts_with("HL.BSTART.");
    if (!IsBStart)
      return Error(IDLoc, "unexpected 'ra=' operand (only valid for BSTART)");
    if (PI.Keywords.empty() || PI.Keywords[0].TextUpper != "CALL")
      return Error(IDLoc, "expected 'CALL' for fused BSTART 'ra=' syntax");
    if (PI.Imms.empty() || !PI.Imms[0].Expr)
      return Error(IDLoc, "expected call target label for BSTART CALL");

    ParsedInst BStartPI = PI;
    BStartPI.SetRetTarget.reset();

    struct Match {
      unsigned Index = 0;
      MCInst Inst;
      unsigned FixedBits = 0;
      unsigned LengthBits = 0;
    };

    std::optional<Match> Best;
    std::string LastErr;

    for (unsigned FormIndex : It->second) {
      const linxisa_inst_form &F = linxisa_inst_forms[FormIndex];
      MCInst MI;
      std::string Err;
      if (!buildMCInstForForm(FormIndex, BStartPI, MI, Err)) {
        LastErr = Err;
        continue;
      }

      Match M;
      M.Index = FormIndex;
      M.Inst = MI;
      M.FixedBits = llvm::popcount(static_cast<uint64_t>(F.mask));
      M.LengthBits = F.length_bits;

      if (!Best || M.FixedBits > Best->FixedBits ||
          (M.FixedBits == Best->FixedBits && M.LengthBits < Best->LengthBits)) {
        Best = M;
      }
    }

    if (!Best) {
      if (!LastErr.empty())
        return Error(IDLoc, LastErr);
      return Error(IDLoc, "no matching encoding for instruction");
    }

    Out.emitInstruction(Best->Inst, getSTI());

    auto SetRetIt = Map.find("C.SETRET");
    if (SetRetIt == Map.end())
      return Error(IDLoc, "missing C.SETRET encoding table");

    ParsedInst SetRetPI;
    SetRetPI.Imms.push_back(*PI.SetRetTarget);

    // C.SETRET is a single encoding; build it directly.
    MCInst SetRetMI;
    std::string SetRetErr;
    bool Built = false;
    for (unsigned FormIndex : SetRetIt->second) {
      if (buildMCInstForForm(FormIndex, SetRetPI, SetRetMI, SetRetErr)) {
        Built = true;
        break;
      }
    }
    if (!Built)
      return Error(IDLoc, SetRetErr.empty() ? "failed to build C.SETRET" : SetRetErr);

    Out.emitInstruction(SetRetMI, getSTI());
    return false;
  }

  struct Match {
    unsigned Index = 0;
    MCInst Inst;
    unsigned FixedBits = 0;
    unsigned LengthBits = 0;
    bool ExactMnemonic = false;
  };

  std::optional<Match> Best;
  std::string LastErr;

  for (unsigned FormIndex : It->second) {
    const linxisa_inst_form &F = linxisa_inst_forms[FormIndex];
    MCInst MI;
    std::string Err;
    if (!buildMCInstForForm(FormIndex, PI, MI, Err)) {
      LastErr = Err;
      continue;
    }

    Match M;
    M.Index = FormIndex;
    M.Inst = MI;
    M.FixedBits = llvm::popcount(static_cast<uint64_t>(F.mask));
    M.LengthBits = F.length_bits;
    M.ExactMnemonic =
        F.mnemonic && StringRef(F.mnemonic).equals_insensitive(StringRef(Key));

    if (!Best || M.ExactMnemonic > Best->ExactMnemonic ||
        (M.ExactMnemonic == Best->ExactMnemonic &&
         (M.FixedBits > Best->FixedBits ||
          (M.FixedBits == Best->FixedBits &&
           M.LengthBits < Best->LengthBits)))) {
      Best = M;
    }
  }

  if (!Best) {
    if (!LastErr.empty())
      return Error(IDLoc, LastErr);
    return Error(IDLoc, "no matching encoding for instruction");
  }

  Out.emitInstruction(Best->Inst, getSTI());
  return false;
}

} // namespace

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLinxISAAsmParser() {
  RegisterMCAsmParser<LinxISAAsmParser> X(getTheLinx32Target());
  RegisterMCAsmParser<LinxISAAsmParser> Y(getTheLinx64Target());
}
