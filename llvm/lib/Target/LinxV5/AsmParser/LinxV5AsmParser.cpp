//=== LinxV5AsmParser.cpp - Parse LinxV5 assembly to MCInst instructions ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5Subtarget.h"
#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "MCTargetDesc/LinxV5CompressInst.h"
#include "MCTargetDesc/LinxV5InstPrinter.h"
#include "MCTargetDesc/LinxV5MCExpr.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "MCTargetDesc/LinxV5MatInt.h"
#include "MCTargetDesc/LinxV5TargetStreamer.h"
#include "MCTargetDesc/LinxV5TileOpExpand.h"
#include "TargetInfo/LinxV5TargetInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/LinxV5ISAInfo.h"
#include "llvm/Support/MathExtras.h"

#include <limits>

using namespace llvm;

#define DEBUG_TYPE "linx-asm-parser"

namespace {

using namespace LinxV5Op;

class LinxV5AsmParser : public MCTargetAsmParser {
  enum {
    IA_SHUTDOWN,
    IA_START,
    IA_TILE,
    IA_SIMT_START,
    IA_SIMT_SINGLE,
    IA_SIMT_MULTI,
    IA_SIMT_BSTOP,
    IA_BLOCK,
    IA_UNSAFE,
  };
  int IAVS = IA_SHUTDOWN; // Inline Asm Validate State

  SMLoc getLoc() const { return getParser().getTok().getLoc(); }

  LinxV5TargetStreamer &getTargetStreamer() {
    MCTargetStreamer &TS = *getParser().getStreamer().getTargetStreamer();
    return static_cast<LinxV5TargetStreamer &>(TS);
  }

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;

  bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  bool ParseRegister(unsigned &RegNo, SMLoc &StartLoc, SMLoc &EndLoc) override;
  OperandMatchResultTy tryParseRegister(unsigned &RegNo, SMLoc &StartLoc,
                                        SMLoc &EndLoc) override;

  bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool ParseDirective(AsmToken DirectiveID) override;

  // Helper to actually emit an instruction to the MCStreamer. Also, when
  // possible, compression of the instruction is performed.
  void emitToStreamer(MCStreamer &S, const MCInst &Inst);

  // Helper to emit pseudo instruction "LaunchKernel".
  void emitVCall(MCInst &Inst, MCStreamer &Out);

  void emitMCall(MCInst &Inst, MCStreamer &Out);

  void emitTCOPY(MCInst &Inst, llvm::MCStreamer &Out);

  void emitTEPL(MCInst &Inst, llvm::MCStreamer &Out, unsigned TEPLOpc);

  void emitEmptyTile(MCInst &Inst, llvm::MCStreamer &Out);

  void emitMAMULBAC(MCInst &Inst, llvm::MCStreamer &Out);

  void emitMAMULBMXAC(MCInst &Inst, MCStreamer &Out);

  void emitMAMULBMXBAC(MCInst &Inst, MCStreamer &Out);

  void emitTCOPYIO(MCInst &Inst, MCStreamer &Out, unsigned MemOpc);

  void emitMcInstVecToStreamer(llvm::SmallVector<MCInst> McVec, MCStreamer &Out);

  // Helper to emit CUBE pseudo instruction
  // "MAMULB/MAMULBT/MAMULB.ACC/MAMULBT.ACC".
  void emitCCall(MCInst &Inst, MCStreamer &Out, unsigned CUBEOpc);

  // Helper to transform instruction "BSTART.STD/AUX/FP, DIRECT/COND/CALL" ->
  // "L.BSTART.STD/AUX/FP, DIRECT/COND/CALL".
  void emitBStartWithTarget(MCInst &Inst, SMLoc IDLoc, MCStreamer &Out);

  // Check instruction constraints.
  bool validateInstruction(MCInst &Inst, OperandVector &Operands, SMLoc IDLoc);

  void startParsingInlineAsm() override {
    bool isSIMT = static_cast<const LinxV5Subtarget *>(&getSTI())->isSIMT();
    if (isSIMT)
      IAVS = IA_SIMT_START;
    else
      IAVS = IA_START;
  }

  void onEndOfFile() override {
    if (IAVS == IA_SIMT_MULTI) {
      Error(getLoc(),
            "simt inline-asm in multi-instructions should ends at l.bstop.");
      Warning(
          getLoc(),
          "The l.bstop in simt inline-asm returns the simt function. Please "
          "write whole simt function in inline-asm or embed single-instruction "
          "inline-asm to represent a specific operation.");
    }
  }

  // Check inline-asm
  bool maybeValidateInlineAsm(MCInst &Inst, SMLoc IDLoc);

  /// Helper for processing MC instructions that have been successfully matched
  /// by MatchAndEmitInstruction.
  bool processInstruction(MCInst &Inst, SMLoc IDLoc, OperandVector &Operands,
                          MCStreamer &Out);

// Auto-generated instruction matching functions
#define GET_ASSEMBLER_HEADER
#include "LinxV5GenAsmMatcher.inc"

  OperandMatchResultTy parseImmediate(OperandVector &Operands);
  OperandMatchResultTy parseRegister(OperandVector &Operands);
  OperandMatchResultTy tryParseToken(OperandVector &Operands);
  OperandMatchResultTy tryParseRegWithSrcRTypeImpl(OperandVector &Operands,
                                                   StringRef Asm,
                                                   SrcRType Type);
  template <class SrcRTypeTraits>
  OperandMatchResultTy tryParseRegWithSrcRType(OperandVector &Operands) {
    return tryParseRegWithSrcRTypeImpl(Operands, SrcRTypeTraits::Asm,
                                       SrcRTypeTraits::Type);
  }
  OperandMatchResultTy parseShamtImm(OperandVector &Operands);
  OperandMatchResultTy parseGPRSrc(OperandVector &Operands);
  OperandMatchResultTy parseRegDepSrc(OperandVector &Operands);
  OperandMatchResultTy parseDstRWithArrow(OperandVector &Operands);
  OperandMatchResultTy parseLoopBDstRWithArrow(OperandVector &Operands);
  OperandMatchResultTy parseTileReg(OperandVector &Operands);
  OperandMatchResultTy parseTileRegWithArrow(OperandVector &Operands);
  // v5: parse "S#n" Shared architectural ID (C.B.IOS binder).
  OperandMatchResultTy parseSharedTID(OperandVector &Operands);
  // PTO v0.58 reissue: parse "->S17" destination Shared ID (B.IOS).
  OperandMatchResultTy parseSharedTIDWithArrow(OperandVector &Operands);
  // v5: parse "mask=N" PE_MASK and "TSize=N" TSize operands.
  OperandMatchResultTy parsePE_MASK(OperandVector &Operands);
  OperandMatchResultTy parseTSize(OperandVector &Operands);
  OperandMatchResultTy parseGPRWithBracket(OperandVector &Operands);
  OperandMatchResultTy parseGPRPlusImm(OperandVector &Operands);
  OperandMatchResultTy parseDRImm(OperandVector &Operands);
  OperandMatchResultTy parseGPRList(OperandVector &Operands);
  OperandMatchResultTy parsePlusImm17(OperandVector &Operands);
  OperandMatchResultTy parseTileSizeWithBracket(OperandVector &Operands);
  OperandMatchResultTy parseBIOSTileSizeWithBracket(OperandVector &Operands);
  OperandMatchResultTy parseSIMTDstRWithArrow(OperandVector &Operands);
  OperandMatchResultTy parseSIMTDstVecRWithArrow(OperandVector &Operands);
  OperandMatchResultTy parseBAttrType(OperandVector &Operands);
  OperandMatchResultTy parseBArgFormat(OperandVector &Operands);
  OperandMatchResultTy parseRMode(OperandVector &Operands);
  OperandMatchResultTy parseCanon(OperandVector &Operands);
  OperandMatchResultTy parseSat(OperandVector &Operands);
  OperandMatchResultTy parseByteID(OperandVector &Operands);
  OperandMatchResultTy parseGroupOp(OperandVector &Operands);
  OperandMatchResultTy parseGPRBitMap(OperandVector &Operands);
  OperandMatchResultTy parseBstartDataType(OperandVector &Operands);
  OperandMatchResultTy parseTileOPTMA(OperandVector &Operands);
  OperandMatchResultTy parseTileOPCUBE(OperandVector &Operands);
  OperandMatchResultTy parseTileOPTEPL(OperandVector &Operands);
  OperandMatchResultTy parseTileOPMode(OperandVector &Operands);
  OperandMatchResultTy parseTEPLMode(OperandVector &Operands);
  OperandMatchResultTy parseBStartWithoutTargetBrType(OperandVector &Operands);
  OperandMatchResultTy parseJALOffset(OperandVector &Operands);
  OperandMatchResultTy parseBareSymbol(OperandVector &Operands);
  OperandMatchResultTy parseCallSymbol(OperandVector &Operands);
  OperandMatchResultTy parsePseudoJumpSymbol(OperandVector &Operands);
  OperandMatchResultTy parseOperandWithModifier(OperandVector &Operands);

  OperandMatchResultTy parseSIMTIntReg(OperandVector &Operands);
  OperandMatchResultTy parseSIMTIntSrcRegType(OperandVector &Operands);
  OperandMatchResultTy parseSIMTFloatSrcRegType(OperandVector &Operands);
  OperandMatchResultTy parseSIMTDstRegType(OperandVector &Operands);
  OperandMatchResultTy parseSIMTRegOpAU(OperandVector &Operands);
  OperandMatchResultTy parseSIMTRegOpLU(OperandVector &Operands);
  OperandMatchResultTy parsePadValue(OperandVector &Operands);
  OperandMatchResultTy parseCmpMode(OperandVector &Operands);
  OperandMatchResultTy parseSIMTRegOp(OperandVector &Operands, StringRef suffix);
  OperandMatchResultTy parseSIMTShamtImm(OperandVector &Operands);
  OperandMatchResultTy parseFenceFlag(OperandVector &Operands);

  bool parseOperand(OperandVector &Operands, StringRef Mnemonic);
  bool parseDirectiveOption();

  void setFeatureBits(uint64_t Feature, StringRef FeatureString) {
    if (!(getSTI().getFeatureBits()[Feature])) {
      MCSubtargetInfo &STI = copySTI();
      setAvailableFeatures(
          ComputeAvailableFeatures(STI.ToggleFeature(FeatureString)));
    }
  }

  void clearFeatureBits(uint64_t Feature, StringRef FeatureString) {
    if (getSTI().getFeatureBits()[Feature]) {
      MCSubtargetInfo &STI = copySTI();
      setAvailableFeatures(
          ComputeAvailableFeatures(STI.ToggleFeature(FeatureString)));
    }
  }

public:
  enum LinxV5MatchResultTy {
    Match_Dummy = FIRST_TARGET_MATCH_RESULT_TY,
#define GET_OPERAND_DIAGNOSTIC_TYPES
#include "LinxV5GenAsmMatcher.inc"
#undef GET_OPERAND_DIAGNOSTIC_TYPES
  };

  static bool classifySymbolRef(const MCExpr *Expr,
                                LinxV5MCExpr::VariantKind &Kind);

  LinxV5AsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                  const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII) {
    Parser.addAliasForDirective(".half", ".2byte");
    Parser.addAliasForDirective(".hword", ".2byte");
    Parser.addAliasForDirective(".word", ".4byte");
    Parser.addAliasForDirective(".dword", ".8byte");
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));

    auto ABIName = StringRef(Options.ABIName);

    // Use computeTargetABI to check if ABIName is valid. If invalid, output
    // error message.
    LinxV5ABI::computeTargetABI(STI.getTargetTriple(), STI.getFeatureBits(),
                                ABIName);

    const MCObjectFileInfo *MOFI = Parser.getContext().getObjectFileInfo();

    // For Lexer to identifier 't#N'/'u#N'
    Parser.getLexer().setAllowHashInIdentifier(true);
  }
};

/// LinxV5Operand - Instances of this class represent a parsed machine
/// instruction
struct LinxV5Operand : public MCParsedAsmOperand {

  enum class KindTy {
    Token,
    Register,
    Immediate,
    RegWithSrcRType,
    SIMTRegType,
    SIMTRegOp,
    VariableOp,
    GroupOp,
    BAttrType,
    Canon,
    DRImm,
    Sat,
    ByteID,
    PadValue,
    CmpMode
  };

  struct RegWithSrcRType {
    MCRegister RegNum;
    SrcRType Type;
    StringRef Asm;
  };

  struct SIMTRegType {
    unsigned Type;
  };

  struct GroupOp {
    unsigned Type;
  };
  struct Canon {
    unsigned Type;
  };

  struct DRImm {
    unsigned Type;
  };

  struct BAttrType {
    unsigned Type;
  };

  struct Sat {
    unsigned Type;
  };

  struct ByteID {
    unsigned Type;
  };

  struct PadValue {
    unsigned Type;
  };

  struct CmpMode {
    unsigned Type;
  };

  struct SIMTRegOp {
    unsigned Type;
  };

  struct RegOp {
    MCRegister RegNum;
  };

  struct VarOp {
    RegOp Regs[24];
    unsigned size;
  };

  struct ImmOp {
    const MCExpr *Val;
  };

  KindTy Kind;
  SMLoc StartLoc, EndLoc;
  union {
    StringRef Tok;
    RegOp Reg;
    ImmOp Imm;
    RegWithSrcRType RegWithSrcRTy;
    SIMTRegType SIMTRegTy;
    SIMTRegOp SIMTRegOpTy;
    GroupOp Group;
    VarOp VarOps;
    Canon CanonTy;
    DRImm DRImmTy;
    BAttrType BAttrTy;
    Sat SatTy;
    ByteID ByteIDTy;
    PadValue PadValueTy;
    CmpMode CmpModeTy;
  };

  LinxV5Operand(KindTy K) : Kind(K) {}

public:
  LinxV5Operand(const LinxV5Operand &o) : MCParsedAsmOperand() {}

  bool isToken() const override { return Kind == KindTy::Token; }
  bool isReg() const override { return Kind == KindTy::Register; }
  bool isImm() const override { return Kind == KindTy::Immediate; }
  bool isCompound() { return Kind == KindTy::RegWithSrcRType; }
  bool isSIMTRegType() { return Kind == KindTy::SIMTRegType; }
  bool isCanon() { return Kind == KindTy::Canon; }
  bool isDRImm() { return Kind == KindTy::DRImm; }
  bool isBAttrType() { return Kind == KindTy::BAttrType; }
  bool isSat() { return Kind == KindTy::Sat; }
  bool isByteID() { return Kind == KindTy::ByteID; }
  bool isPadValue() { return Kind == KindTy::PadValue; }
  bool isCmpMode() { return Kind == KindTy::CmpMode; }
  bool isVariableOp() const { return Kind == KindTy::VariableOp; }
  bool isMem() const override {
    assert(0 && "TODO: Asmparser for Mem operand!");
    return false;
  }

  template <class SrcRTypeTraits> bool isRegWithSrcRType() const {
    return RegWithSrcRTy.Type == SrcRTypeTraits::Type;
  }

  bool isSImm20LUI() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm) {
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
      return IsValid && (VK == LinxV5MCExpr::VK_LinxV5_TPREL_HI);
    } else {
      return isInt<20>(Imm) && (VK == LinxV5MCExpr::VK_LinxV5_None ||
                                VK == LinxV5MCExpr::VK_LinxV5_TPREL_HI);
    }
  }

  bool isSImm20ADDTPC() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm) {
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
      return IsValid && (VK == LinxV5MCExpr::VK_LinxV5_TPCREL_HI);
    } else {
      return isInt<20>(Imm) && (VK == LinxV5MCExpr::VK_LinxV5_None ||
                                VK == LinxV5MCExpr::VK_LinxV5_TPCREL_HI);
    }
  }

  bool isSImm32HLADDTPC() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm) {
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
      return IsValid && (VK == LinxV5MCExpr::VK_LinxV5_TPCREL_HI32);
    } else {
      return isInt<20>(Imm) && (VK == LinxV5MCExpr::VK_LinxV5_None ||
                                VK == LinxV5MCExpr::VK_LinxV5_TPCREL_HI32);
    }
  }

  bool isSIMTIntReg() const {
    return isReg() &&
           (LinxV5MCRegisterClasses[LinxV5::SIMT_SRC_VecRegClassID].contains(
                getReg()) ||
            LinxV5MCRegisterClasses[LinxV5::SIMT_SRC_ScalarRegClassID].contains(
                getReg()));
  }

  bool isSIMTRegOpAU() const {
    return Kind == KindTy::SIMTRegOp;
  }

  bool isSIMTRegOpLU() const {
    return Kind == KindTy::SIMTRegOp;
  }

  bool isSIMTDstIntReg() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::SIMT_DST_Vec_MCRegClassID].contains(
               getReg());
  }

  bool isSIMTDstCMPIntScalarReg() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::SIMT_DST_CMP_Scalar_MCRegClassID]
               .contains(getReg());
  }

  bool isSIMTDstIntScalarReg() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::SIMT_DST_Scalar_MCRegClassID]
               .contains(getReg()) &&
           getReg() != LinxV5::SIMT_P;
  }

  bool isSIMTDstIntScalarRegP() const {
    return isReg() && getReg() == LinxV5::SIMT_P;
  }

  bool isSIMTDstIntRegReduce() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::SIMT_DST_Scalar_MCRegClassID]
               .contains(getReg());
  }

  bool isTileReg() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::TILE_SRCRegClassID].contains(
               getReg());
  }

  bool isACCTileReg() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::ACC_SRCRegClassID].contains(
               getReg());
  }

  bool isTileRegWithArrow() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::TILE_DSTRegClassID].contains(
               getReg());
  }

  bool isTileDstNoArrow() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::TILE_DSTRegClassID].contains(
               getReg());
  }

  bool isACC_TileRegWithArrow() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::ACC_DSTRegClassID].contains(
               getReg());
  }

  bool isStack_TileRegWithArrow() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::TILE_DSTSRegClassID].contains(
               getReg());
  }

  bool isStack_TileRegNoArrow() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::TILE_DSTSRegClassID].contains(
               getReg());
  }

  bool isGroupOp() const { return Kind == KindTy::GroupOp; }

  // v5: SharedTID/PE_MASK/TSize are parsed as immediates.
  bool isSharedTID() const { return isImm(); }
  bool isSharedTIDWithArrow() const { return isImm(); }
  bool isPE_MASK() const { return isImm(); }
  bool isTSize() const { return isImm(); }

  // v5: B.IOT destination-suffix TileSize ("<8KB>"), same value form as
  // isTileSizeWithBracket; distinct predicate per AsmOperandClass name.
  bool isB_IOT_TileSize() const { return isTileSizeWithBracket(); }
  bool isBIOSTileSize() const { return isTileSizeWithBracket(); }

  bool isGPRWithBracket() const { return isReg(); }

  bool isTileSizeWithBracket() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm) {
      return true;
    } else {
      return IsUImm<4>();
    }
  }

  bool isGPRPlusImm() const { return true; }

  bool isPlusImm17() const { return true; }

  bool isRegDepSrc() const {
    return isReg() &&
           LinxV5MCRegisterClasses[LinxV5::Dep_SRCRegClassID].contains(
               getReg());
  }

  bool isGPRSrc() const { return isReg(); }

  bool isGPRSrcNoR0R1() const {
    return LinxV5MCRegisterClasses[LinxV5::GRRegClassID].contains(getReg()) &&
           getReg() != LinxV5::R0 && getReg() != LinxV5::R1;
  }

  bool isGPRSrcNoR0() const {
    return LinxV5MCRegisterClasses[LinxV5::GRRegClassID].contains(getReg()) &&
           getReg() != LinxV5::R0;
  }

  bool isSIMTSrcRegType() const { return Kind == KindTy::SIMTRegType; }

  bool isSIMTIntSrcRegType() const { return isSIMTSrcRegType(); }

  bool isSIMTFloatSrcRegType() const { return isSIMTSrcRegType(); }

  bool isSIMTDstRegType() const { return isSIMTSrcRegType(); }

  bool isSIMTShamtImm() const {
    // TODO: Add more check.
    return true;
  }

  template <unsigned N> bool isSIMTShamtImmPlus() {
    // TODO: Add more check.
    return true;
  }

  bool isShamtImm() const {
    // TODO: Add more check.
    return true;
  }

  bool isUimm3Plus1() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    return IsConstantImm && Imm >= 1 && Imm <= 8 &&
           VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  bool isUimm6Plus1() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    return IsConstantImm && Imm >= 1 && Imm <= 64 &&
           VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  bool isUImm6RevPlus1() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    std::set<int64_t> vals = {2, 4, 8, 16, 32, 64};
    return IsConstantImm && vals.find(Imm) != vals.end()
           && VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  bool isUImm6Rev() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    std::set<int64_t> vals = {1, 2, 4, 8, 16, 32};
    return IsConstantImm && vals.find(Imm) != vals.end()
           && VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  bool isDstRWithArrow() const {
    // TODO: Add more check.
    return true;
  }

  bool isDstRWithArrowNoRA() const { return getReg() != LinxV5::R10; }

  bool isDstRWithArrowLoopB() const { return true; }

  bool isBStartWithoutTargetBrType() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValidBrType = false;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    IsValidBrType = (Imm == BranchType::FALL) || (Imm == BranchType::IND) ||
                    (Imm == BranchType::ICALL) || (Imm == BranchType::RET);
    return IsConstantImm && IsValidBrType && VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  bool isBstartDataType() const { return isImm() && IsUImm<5>(); }

  bool isTileOPTMA() const { return isImm() && IsUImm<5>(); }

  bool isTileOPCUBE() const { return isImm() && IsUImm<5>(); }

  bool isTileOPTEPL() const { return isImm() && IsUImm<7>(); }

  bool isTileOPMode() const { return isImm() && IsUImm<2>(); }

  bool isTEPLMode() const { return isImm() && IsUImm<2>(); }

  bool isBArgFormat() const { return isImm() && IsUImm<5>(); }

  bool isRMode() const { return isImm() && IsUImm<3>(); }

  bool isGPRBitMap() const {
    // TODO: Add more check.
    return true;
  }

  bool isPadValue() const { return IsUImm<3>(); }

  bool isCmpMode() const { return IsUImm<2>(); }

  bool isGPRList() const { return isVariableOp(); }

  bool isFenceFlag() const {
    // TODO: Add more check.
    return true;
  }

  template <bool Signed, unsigned N, unsigned S> struct isShiftedImm {
    bool operator()(uint64_t Imm) { return false; }
  };

  template <unsigned N, unsigned S> struct isShiftedImm<true, N, S> {
    bool operator()(uint64_t Imm) { return isShiftedInt<N, S>(Imm); }
  };

  template <unsigned N, unsigned S> struct isShiftedImm<false, N, S> {
    bool operator()(uint64_t Imm) { return isShiftedUInt<N, S>(Imm); }
  };

  template <bool Signed, unsigned N, unsigned S> bool isBranchTarget() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm) {
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
    } else {
      IsValid = isShiftedImm<Signed, N, S>()(Imm);
    }
    return IsValid && VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  bool isSImm43Lsb0BNext() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm) {
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
    } else {
      IsValid = isShiftedInt<42, 1>(Imm);
    }
    return IsValid && VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  bool isSImm30Lsb0BNext() const {
    // TODO: Add more check.
    return true;
  }

  bool isSImm26Lsb0BNext() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm) {
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
    } else {
      IsValid = isShiftedInt<25, 1>(Imm);
    }
    return IsValid && VK == LinxV5MCExpr::VK_LinxV5_None;
  }

    bool isSImm18Lsb0BNext() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm) {
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
    } else {
      IsValid = isShiftedInt<17, 1>(Imm);
    }
    return IsValid && VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  bool isSImm13Lsb0() const { return isBranchTarget<true, 12, 1>(); }

  bool isSImm23Lsb0() const { return isBranchTarget<true, 22, 1>(); }

  bool isSImm17() const { return isBranchTarget<true, 17, 0>(); }

  bool isSImm29() const { return isBranchTarget<true, 29, 0>(); }

  bool isUImm32() const { return IsUImm<32>(); }

  bool isSImm42() const { return isBranchTarget<true, 42, 0>(); }

  bool isSImm11Lsb0() const { return isBranchTarget<true, 10, 1>(); }

  bool isUImm6Lsb0() const { return isBranchTarget<false, 5, 1>(); }

  bool isUImm33Lsb0() const { return isBranchTarget<false, 32, 1>(); }

  bool isUImm21Lsb0() const { return isBranchTarget<false, 20, 1>(); }

  bool isSImm15Lsl3() const { return isBranchTarget<true, 12, 3>(); }

  bool isSImm25Lsl3() const { return isBranchTarget<true, 22, 3>(); }

  bool isBareSymbol() const {
    assert(0 && "Delete me! This is the LinxV5V3 define!");
    return false;
  }

  bool isCallSymbol() const {
    assert(0 && "Delete me! This is the LinxV5V3 define!");
    return false;
  }

  bool isPseudoJumpSymbol() const {
    assert(0 && "Delete me! This is the LinxV5V3 define!");
    return false;
  }

  bool isTPRelAddSymbol() const {
    assert(0 && "Delete me! This is the LinxV5V3 define!");
    return false;
  }

  static bool evaluateConstantImm(const MCExpr *Expr, int64_t &Imm,
                                  LinxV5MCExpr::VariantKind &VK) {
    if (auto CE = dyn_cast<MCConstantExpr>(Expr)) {
      VK = LinxV5MCExpr::VK_LinxV5_None;
      Imm = CE->getValue();
      return true;
    }

    return false;
  }

  template <unsigned N> bool IsUImm() const {
    if (!isImm())
      return false;
    int64_t Imm;
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    return IsConstantImm && isUInt<N>(Imm) &&
           VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  template <unsigned N> bool IsSImm() const {
    if (!isImm())
      return false;
    int64_t Imm;
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    return IsConstantImm && isInt<N>(Imm) && VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  template <unsigned N, unsigned S> bool isSImmShifted() {
    if (!isImm())
      return false;
    int64_t Imm;
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    return IsConstantImm && isShiftedInt<N, S>(Imm) &&
           VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  template <bool Signed, unsigned N> bool isImmShiftN() {
    if (!isImm())
      return false;
    int64_t Imm;
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    int64_t shift, base;
    if (Signed ? isInt<N>(Imm) : isUInt<N>(Imm)) {
      shift = 0;
    } else {
      shift = llvm::countTrailingZeros((uint64_t)Imm);
      if (!isUInt<5>(shift))
        shift = 0x1f;
    }
    base = Signed ? Imm >> shift : ((uint64_t)Imm) >> shift;
    return IsConstantImm && isUInt<5>(shift) &&
           (Signed ? isInt<N>(base) : isUInt<N>(base)) &&
           VK == LinxV5MCExpr::VK_LinxV5_None;
  }

  template <unsigned N> bool isSImmShiftN() { return isImmShiftN<true, N>(); }

  template <unsigned N> bool isUImmShiftN() { return isImmShiftN<false, N>(); }

  bool isUImm1() { return IsUImm<1>(); }
  bool isUImm2() { return IsUImm<2>(); }
  bool isUImm3() { return IsUImm<3>(); }
  bool isUImm4() { return IsUImm<4>(); }
  bool isSImm4() { return IsSImm<4>(); }
  bool isUImm5() { return IsUImm<5>(); }
  bool isSImm5() { return IsSImm<5>(); }
  bool isUImm6() { return IsUImm<6>(); }
  bool isUImm7() { return IsUImm<7>(); }
  bool isUImm24() { return IsUImm<24>(); }
  bool isSImm7() { return IsSImm<7>(); }
  bool isUImm8() { return IsUImm<8>(); }
  bool isSImm8() { return IsSImm<8>(); }
  bool isSImm14() { return IsSImm<14>(); }
  bool isUImm10() { return IsUImm<10>(); }
  bool isUImm14() { return IsUImm<14>(); }
  bool isSImm10() { return IsSImm<10>(); }

  bool isUImm12() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm)
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
    else
      IsValid = isUInt<12>(Imm);
    return IsValid && ((IsConstantImm && VK == LinxV5MCExpr::VK_LinxV5_None) ||
                       VK == LinxV5MCExpr::VK_LinxV5_TPCREL_LO ||
                       VK == LinxV5MCExpr::VK_LinxV5_TPREL_LO);
  }

  bool isSImm12() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm)
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
    else
      IsValid = isInt<12>(Imm);
    return IsValid && ((IsConstantImm && VK == LinxV5MCExpr::VK_LinxV5_None) ||
                       VK == LinxV5MCExpr::VK_LinxV5_TPCREL_LO);
  }

  bool isSImm24() const {
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    int64_t Imm;
    bool IsValid;
    if (!isImm())
      return false;
    bool IsConstantImm = evaluateConstantImm(getImm(), Imm, VK);
    if (!IsConstantImm)
      IsValid = LinxV5AsmParser::classifySymbolRef(getImm(), VK);
    else
      IsValid = isInt<24>(Imm);
    return IsValid && ((IsConstantImm && VK == LinxV5MCExpr::VK_LinxV5_None) ||
                       VK == LinxV5MCExpr::VK_LinxV5_TPCREL_LO);
  }

  bool isUImm15() { return IsUImm<15>(); }
  bool isUImm16() { return IsUImm<16>(); }
  bool isUImm17() { return IsUImm<17>(); }
  bool isUImm19() { return IsUImm<19>(); }
  bool isSImm32() { return IsSImm<32>(); }
  bool isSImm64() { return IsSImm<64>(); }

  bool isSImm21Lsb0JAL() const {
    assert(0 && "Delete me! This is the LinxV5V3 define!");
    return false;
  }

  bool isUImmLog2XLen() const {
    assert(0 && "Delete me! This is the LinxV5V3 define!");
    return false;
  }

  unsigned getReg() const override {
    assert(Kind == KindTy::Register && "Invalid type access!");
    return Reg.RegNum.id();
  }

  SmallVector<unsigned> getRegs() const {
    assert(Kind == KindTy::VariableOp && "Invalid type access!");
    SmallVector<unsigned> res;
    for (int i = 0; i < VarOps.size; ++i) {
      res.push_back(VarOps.Regs[i].RegNum.id());
    }
    return res;
  }

  const MCExpr *getImm() const {
    assert(Kind == KindTy::Immediate && "Invalid type access!");
    return Imm.Val;
  }

  StringRef getToken() const {
    assert(Kind == KindTy::Token && "Invalid type access!");
    return Tok;
  }

  /// getStartLoc - Gets location of the first token of this operand
  SMLoc getStartLoc() const override { return StartLoc; }
  /// getEndLoc - Gets location of the last token of this operand
  SMLoc getEndLoc() const override { return EndLoc; }

  void print(raw_ostream &OS) const override {
    auto RegName = [](unsigned Reg) {
      if (Reg)
        return LinxV5InstPrinter::getRegisterName(Reg);
      else
        return "noreg";
    };

    switch (Kind) {
    case KindTy::Immediate:
      OS << "<imm " << *getImm() << ">";
      break;
    case KindTy::Register:
      OS << "<register " << RegName(getReg()) << ">";
      break;
    case KindTy::Token:
      OS << "'" << getToken() << "'";
      break;
    case KindTy::RegWithSrcRType:
      OS << "<SrcR " << RegName(RegWithSrcRTy.RegNum) << RegWithSrcRTy.Asm
         << ">";
      break;
    case KindTy::SIMTRegType:
      OS << "<RegType " << SIMTRegTy.Type << ">";
      break;
    case KindTy::GroupOp:
      OS << "<Group " << Group.Type << ">";
    case KindTy::Canon:
      OS << "<CanonType " << CanonTy.Type << ">";
      break;
    case KindTy::DRImm:
      OS << "<DRImmType " << DRImmTy.Type << ">";
      break;
    case KindTy::BAttrType:
      OS << "<BAttrType " << BAttrTy.Type << ">";
    case KindTy::Sat:
      OS << "<SatType " << SatTy.Type << ">";
      break;
    case KindTy::ByteID:
      OS << "<ByteIDType " << ByteIDTy.Type << ">";
      break;
    case KindTy::PadValue:
      OS << "<PadValueType " << PadValueTy.Type << ">";
      break;
    case KindTy::CmpMode:
      OS << "<CmpModeType " << CmpModeTy.Type << ">";
      break;
    case KindTy::SIMTRegOp:
      OS << "<RegSuffix " << SIMTRegOpTy.Type << ">";
      break;
    case KindTy::VariableOp:
      OS << "<VariableOp ";
      for (int i = 0; i < VarOps.size; ++i) {
        OS << RegName(VarOps.Regs[i].RegNum.id());
        if (i != VarOps.size - 1)
          OS << ", ";
      }
      OS << ">";
      break;
    default:
      OS << "<unknown kind " << (unsigned)Kind << ">";
      break;
    }
  }

  static std::unique_ptr<LinxV5Operand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::Token);
    Op->Tok = Str;
    Op->StartLoc = S;
    Op->EndLoc = S;

    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createReg(unsigned RegNo, SMLoc S,
                                                  SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::Register);
    Op->Reg.RegNum = RegNo;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createImm(const MCExpr *Val, SMLoc S,
                                                  SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::Immediate);
    Op->Imm.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand>
  createRegWithSrcRType(unsigned RegNo, StringRef Asm, SrcRType Type, SMLoc S,
                        SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::RegWithSrcRType);
    Op->RegWithSrcRTy.RegNum = RegNo;
    Op->RegWithSrcRTy.Asm = Asm;
    Op->RegWithSrcRTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createSIMTRegType(unsigned Type,
                                                          SMLoc S, SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::SIMTRegType);
    Op->SIMTRegTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createGroupOp(unsigned Type,
                                                          SMLoc S, SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::GroupOp);
    Op->Group.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createCanonType(unsigned Type, SMLoc S,
                                                        SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::Canon);
    Op->CanonTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createDRImmType(unsigned Type, SMLoc S,
                                                        SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::DRImm);
    Op->DRImmTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createBAttrType(unsigned Type, SMLoc S,
                                                        SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::BAttrType);
    Op->BAttrTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createSatType(unsigned Type, SMLoc S,
                                                      SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::Sat);
    Op->SatTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createByteIDType(unsigned Type, SMLoc S,
                                                         SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::ByteID);
    Op->ByteIDTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createPadValueType(unsigned Type,
                                                           SMLoc S, SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::PadValue);
    Op->PadValueTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createCmpModeType(unsigned Type,
                                                          SMLoc S, SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::CmpMode);
    Op->CmpModeTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand>
  createVariableOp(SmallVector<llvm::MCRegister> VarOps, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::VariableOp);
    Op->VarOps.size = VarOps.size();
    for (int i = 0; i < VarOps.size(); ++i) {
      Op->VarOps.Regs[i].RegNum = VarOps[i];
    }
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  static std::unique_ptr<LinxV5Operand> createSIMTRegOp(unsigned Type,
                                                          SMLoc S, SMLoc E) {
    auto Op = std::make_unique<LinxV5Operand>(KindTy::SIMTRegOp);
    Op->SIMTRegOpTy.Type = Type;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }

  void addExpr(MCInst &Inst, const MCExpr *Expr) const {
    assert(Expr && "Expr shouldn't be null!");
    int64_t Imm = 0;
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    bool IsConstant = evaluateConstantImm(Expr, Imm, VK);

    if (IsConstant)
      Inst.addOperand(MCOperand::createImm(Imm));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  void addRegsOperands(MCInst &Inst, unsigned N) const {
    assert(N == 0 && "Invalid number of operands!");
    auto Regs = getRegs();
    for (auto Reg : Regs)
      Inst.addOperand(MCOperand::createReg(Reg));
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    addExpr(Inst, getImm());
  }

  template <unsigned S>
  void addShamtImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    int64_t Imm = 0;
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    bool IsConstant = evaluateConstantImm(getImm(), Imm, VK);

    if (IsConstant) {
      assert(Imm >= S && "Invalid shamt operand!");
      Inst.addOperand(MCOperand::createImm(Imm - S));
    } else
      Inst.addOperand(MCOperand::createExpr(getImm()));
  }

  template <unsigned S>
  void addImmShiftedOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    int64_t Imm = 0;
    LinxV5MCExpr::VariantKind VK = LinxV5MCExpr::VK_LinxV5_None;
    bool IsConstant = evaluateConstantImm(getImm(), Imm, VK);

    if (IsConstant)
      Inst.addOperand(MCOperand::createImm(Imm >> S));
    else
      Inst.addOperand(MCOperand::createExpr(getImm()));
  }

  void addRegWithSrcRTypeOperands(MCInst &Inst, unsigned N) const {
    MCRegister RegNum = this->RegWithSrcRTy.RegNum;
    Inst.addOperand(MCOperand::createReg(RegNum.id()));
  }

  void addSIMTRegType(MCInst &Inst, unsigned N) const {
    unsigned Type = this->SIMTRegTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addGroupOp(MCInst &Inst, unsigned N) const {
    unsigned Type = this->Group.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addCanonType(MCInst &Inst, unsigned N) const {
    unsigned Type = this->CanonTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addDRImmType(MCInst &Inst, unsigned N) const {
    unsigned Type = this->DRImmTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addBAttrType(MCInst &Inst, unsigned N) const {
    unsigned Type = this->BAttrTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addSatType(MCInst &Inst, unsigned N) const {
    unsigned Type = this->SatTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addByteIDType(MCInst &Inst, unsigned N) const {
    unsigned Type = this->ByteIDTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addPadValueType(MCInst &Inst, unsigned N) const {
    unsigned Type = this->PadValueTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addCmpModeType(MCInst &Inst, unsigned N) const {
    unsigned Type = this->CmpModeTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }

  void addSIMTRegOp(MCInst &Inst, unsigned N) const {
    unsigned Type = this->SIMTRegOpTy.Type;
    Inst.addOperand(MCOperand::createImm(Type));
  }
};

} // end anonymous namespace.

#define GET_REGISTER_MATCHER
#define GET_SUBTARGET_FEATURE_NAME
#define GET_MATCHER_IMPLEMENTATION
#define GET_MNEMONIC_SPELL_CHECKER
#include "LinxV5GenAsmMatcher.inc"

unsigned LinxV5AsmParser::validateTargetOperandClass(MCParsedAsmOperand &AsmOp,
                                                     unsigned Kind) {
  return Match_InvalidOperand;
}

bool LinxV5AsmParser::MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                              OperandVector &Operands,
                                              MCStreamer &Out,
                                              uint64_t &ErrorInfo,
                                              bool MatchingInlineAsm) {
  MCInst Inst;
  FeatureBitset MissingFeatures;

  LLVM_DEBUG(dbgs() << "Parsed Operands ";
    for (unsigned i = 0; i < Operands.size(); ++i) {
      dbgs() << (i == 0 ? "" : ", ") << *Operands[i];
    }
    dbgs() << "\n";
  );

  auto Result = MatchInstructionImpl(Operands, Inst, ErrorInfo, MissingFeatures,
                                     MatchingInlineAsm);
  switch (Result) {
  // TODO: Handle more Result Type.
  default: {
    std::string Msg = "Match Instruction Error!";
    return Error(IDLoc, Msg);
  }
  case Match_Success:
    if (maybeValidateInlineAsm(Inst, IDLoc))
      return true;
    if (validateInstruction(Inst, Operands, IDLoc))
      return Error(IDLoc, "Match Instruction Invalid!");
    return processInstruction(Inst, IDLoc, Operands, Out);
  }

  // TODO: provide more detail error message.
}

bool LinxV5AsmParser::ParseRegister(unsigned &RegNo, SMLoc &StartLoc,
                                    SMLoc &EndLoc) {
  if (tryParseRegister(RegNo, StartLoc, EndLoc) != MatchOperand_Success)
    return Error(StartLoc, "invalid register name");
  return false;
}

// this function split str to 2 parts
// E.g. t#1.reuse.uw.neg<<1 -> (t#1.reuse, .uw.neg<<1)
//      t#1.uw.neg<<1 -> (t#1, .uw.neg<<1)
static std::pair<StringRef, StringRef> splitSIMTReuseRegStr(StringRef input) {
  size_t firstDot = input.find('.');
  if (firstDot == StringRef::npos) {
    return {input, StringRef()};
  }

  size_t reusePos = input.find("reuse");
  if (reusePos != StringRef::npos && reusePos > firstDot) {
    size_t afterReuse = input.find('.', StringLiteral("reuse").size() + reusePos);
    if (afterReuse == StringRef::npos) {
      return {input, StringRef()};
    }
    return {input.substr(0, afterReuse), input.substr(afterReuse)};
  } else {
    return {input.substr(0, firstDot), input.substr(firstDot)};
  }
}

static unsigned MatchLinxV5GlobalRegisteName(StringRef Name) {
  size_t DotPosition = Name.find('.');
  StringRef RegName = Name.slice(0, DotPosition);
  return StringSwitch<unsigned>(RegName.lower())
      .Case("zero", LinxV5::R0)
      .Case("sp", LinxV5::R1)
      .Case("a0", LinxV5::R2)
      .Case("a1", LinxV5::R3)
      .Case("a2", LinxV5::R4)
      .Case("a3", LinxV5::R5)
      .Case("a4", LinxV5::R6)
      .Case("a5", LinxV5::R7)
      .Case("a6", LinxV5::R8)
      .Case("a7", LinxV5::R9)
      .Case("ra", LinxV5::R10)
      .Cases("fp", "s0", LinxV5::R11)
      .Case("s1", LinxV5::R12)
      .Case("s2", LinxV5::R13)
      .Case("s3", LinxV5::R14)
      .Case("s4", LinxV5::R15)
      .Case("s5", LinxV5::R16)
      .Case("s6", LinxV5::R17)
      .Case("s7", LinxV5::R18)
      .Case("s8", LinxV5::R19)
      .Case("x0", LinxV5::R20)
      .Case("x1", LinxV5::R21)
      .Case("x2", LinxV5::R22)
      .Case("x3", LinxV5::R23)
      .Case("r0", LinxV5::R0)
      .Case("r1", LinxV5::R1)
      .Case("r2", LinxV5::R2)
      .Case("r3", LinxV5::R3)
      .Case("r4", LinxV5::R4)
      .Case("r5", LinxV5::R5)
      .Case("r6", LinxV5::R6)
      .Case("r7", LinxV5::R7)
      .Case("r8", LinxV5::R8)
      .Case("r9", LinxV5::R9)
      .Case("r10", LinxV5::R10)
      .Case("r11", LinxV5::R11)
      .Case("r12", LinxV5::R12)
      .Case("r13", LinxV5::R13)
      .Case("r14", LinxV5::R14)
      .Case("r15", LinxV5::R15)
      .Case("r16", LinxV5::R16)
      .Case("r17", LinxV5::R17)
      .Case("r18", LinxV5::R18)
      .Case("r19", LinxV5::R19)
      .Case("r20", LinxV5::R20)
      .Case("r21", LinxV5::R21)
      .Case("r22", LinxV5::R22)
      .Case("r23", LinxV5::R23)
      .Case("lc0", LinxV5::SIMT_LC0)
      .Case("lb0", LinxV5::SIMT_LB0)
      .Case("lc1", LinxV5::SIMT_LC1)
      .Case("lb1", LinxV5::SIMT_LB1)
      .Case("lc2", LinxV5::SIMT_LC2)
      .Case("lb2", LinxV5::SIMT_LB2)
      .Default(LinxV5::NoRegister);
}

static unsigned MatchLinxV5TileRegisteName(StringRef Name) {
  return StringSwitch<unsigned>(Name.lower())
      .Case("t#1", LinxV5::Tile_TOS1)
      .Case("t#2", LinxV5::Tile_TOS2)
      .Case("t#3", LinxV5::Tile_TOS3)
      .Case("t#4", LinxV5::Tile_TOS4)
      .Case("t#5", LinxV5::Tile_TOS5)
      .Case("t#6", LinxV5::Tile_TOS6)
      .Case("t#7", LinxV5::Tile_TOS7)
      .Case("t#8", LinxV5::Tile_TOS8)
      .Case("t#9", LinxV5::Tile_TOS9)
      .Case("t#10", LinxV5::Tile_TOS10)
      .Case("t#11", LinxV5::Tile_TOS11)
      .Case("t#12", LinxV5::Tile_TOS12)
      .Case("t#13", LinxV5::Tile_TOS13)
      .Case("t#14", LinxV5::Tile_TOS14)
      .Case("t#15", LinxV5::Tile_TOS15)
      .Case("t#16", LinxV5::Tile_TOS16)
      .Case("u#1", LinxV5::Tile_UOS1)
      .Case("u#2", LinxV5::Tile_UOS2)
      .Case("u#3", LinxV5::Tile_UOS3)
      .Case("u#4", LinxV5::Tile_UOS4)
      .Case("u#5", LinxV5::Tile_UOS5)
      .Case("u#6", LinxV5::Tile_UOS6)
      .Case("u#7", LinxV5::Tile_UOS7)
      .Case("u#8", LinxV5::Tile_UOS8)
      .Case("u#9", LinxV5::Tile_UOS9)
      .Case("u#10", LinxV5::Tile_UOS10)
      .Case("u#11", LinxV5::Tile_UOS11)
      .Case("u#12", LinxV5::Tile_UOS12)
      .Case("u#13", LinxV5::Tile_UOS13)
      .Case("u#14", LinxV5::Tile_UOS14)
      .Case("u#15", LinxV5::Tile_UOS15)
      .Case("u#16", LinxV5::Tile_UOS16)
      .Case("m#1", LinxV5::Tile_MOS1)
      .Case("m#2", LinxV5::Tile_MOS2)
      .Case("m#3", LinxV5::Tile_MOS3)
      .Case("m#4", LinxV5::Tile_MOS4)
      .Case("m#5", LinxV5::Tile_MOS5)
      .Case("m#6", LinxV5::Tile_MOS6)
      .Case("m#7", LinxV5::Tile_MOS7)
      .Case("m#8", LinxV5::Tile_MOS8)
      .Case("m#9", LinxV5::Tile_MOS9)
      .Case("m#10", LinxV5::Tile_MOS10)
      .Case("m#11", LinxV5::Tile_MOS11)
      .Case("m#12", LinxV5::Tile_MOS12)
      .Case("m#13", LinxV5::Tile_MOS13)
      .Case("m#14", LinxV5::Tile_MOS14)
      .Case("m#15", LinxV5::Tile_MOS15)
      .Case("m#16", LinxV5::Tile_MOS16)
      .Case("n#1", LinxV5::Tile_NOS1)
      .Case("n#2", LinxV5::Tile_NOS2)
      .Case("n#3", LinxV5::Tile_NOS3)
      .Case("n#4", LinxV5::Tile_NOS4)
      .Case("n#5", LinxV5::Tile_NOS5)
      .Case("n#6", LinxV5::Tile_NOS6)
      .Case("n#7", LinxV5::Tile_NOS7)
      .Case("n#8", LinxV5::Tile_NOS8)
      .Case("n#9", LinxV5::Tile_NOS9)
      .Case("n#10", LinxV5::Tile_NOS10)
      .Case("n#11", LinxV5::Tile_NOS11)
      .Case("n#12", LinxV5::Tile_NOS12)
      .Case("n#13", LinxV5::Tile_NOS13)
      .Case("n#14", LinxV5::Tile_NOS14)
      .Case("n#15", LinxV5::Tile_NOS15)
      .Case("n#16", LinxV5::Tile_NOS16)































































      .Case("t", LinxV5::Tile_T)
      .Case("u", LinxV5::Tile_U)
      .Case("m", LinxV5::Tile_M)
      .Case("n", LinxV5::Tile_N)
      .Case("s", LinxV5::Tile_S)
      .Case("acc", LinxV5::Tile_ACC)
      .Case("acc#1", LinxV5::Tile_ACCOS1)
      .Default(LinxV5::NoRegister);
}

static unsigned MatchLinxV5SIMTRegisterName(StringRef Name) {
  return StringSwitch<unsigned>(Name.lower())
      .Case("ri0", LinxV5::SIMT_RI0)
      .Case("ri1", LinxV5::SIMT_RI1)
      .Case("ri2", LinxV5::SIMT_RI2)
      .Case("ri3", LinxV5::SIMT_RI3)
      .Case("ri4", LinxV5::SIMT_RI4)
      .Case("ri5", LinxV5::SIMT_RI5)
      .Case("ri6", LinxV5::SIMT_RI6)
      .Case("ri7", LinxV5::SIMT_RI7)
      .Case("ri8", LinxV5::SIMT_RI8)
      .Case("ri9", LinxV5::SIMT_RI9)
      .Case("ri10", LinxV5::SIMT_RI10)
      .Case("ri11", LinxV5::SIMT_RI11)
      .Case("t#1", LinxV5::SIMT_OST1)
      .Case("t#2", LinxV5::SIMT_OST2)
      .Case("t#3", LinxV5::SIMT_OST3)
      .Case("t#4", LinxV5::SIMT_OST4)
      .Case("u#1", LinxV5::SIMT_OSU1)
      .Case("u#2", LinxV5::SIMT_OSU2)
      .Case("u#3", LinxV5::SIMT_OSU3)
      .Case("u#4", LinxV5::SIMT_OSU4)
      .Case("t", LinxV5::SIMT_T)
      .Case("u", LinxV5::SIMT_U)
      .Case("p", LinxV5::SIMT_P)
      .Case("ro0", LinxV5::SIMT_RO0)
      .Case("ro1", LinxV5::SIMT_RO1)
      .Case("ro2", LinxV5::SIMT_RO2)
      .Case("ro3", LinxV5::SIMT_RO3)
      .Case("vt#1", LinxV5::SIMT_OSVT1)
      .Case("vt#2", LinxV5::SIMT_OSVT2)
      .Case("vt#3", LinxV5::SIMT_OSVT3)
      .Case("vt#4", LinxV5::SIMT_OSVT4)
      .Case("vu#1", LinxV5::SIMT_OSVU1)
      .Case("vu#2", LinxV5::SIMT_OSVU2)
      .Case("vu#3", LinxV5::SIMT_OSVU3)
      .Case("vu#4", LinxV5::SIMT_OSVU4)
      .Case("vm#1", LinxV5::SIMT_OSVM1)
      .Case("vm#2", LinxV5::SIMT_OSVM2)
      .Case("vm#3", LinxV5::SIMT_OSVM3)
      .Case("vm#4", LinxV5::SIMT_OSVM4)
      .Case("vn#1", LinxV5::SIMT_OSVN1)
      .Case("vn#2", LinxV5::SIMT_OSVN2)
      .Case("vn#3", LinxV5::SIMT_OSVN3)
      .Case("vn#4", LinxV5::SIMT_OSVN4)















      .Case("vt", LinxV5::SIMT_VT)
      .Case("vu", LinxV5::SIMT_VU)
      .Case("vm", LinxV5::SIMT_VM)
      .Case("vn", LinxV5::SIMT_VN)
      .Case("vt1", LinxV5::SIMT_VT1)
      .Case("vt2", LinxV5::SIMT_VT2)
      .Case("vt3", LinxV5::SIMT_VT3)
      .Case("vt4", LinxV5::SIMT_VT4)
      .Case("vu1", LinxV5::SIMT_VU1)
      .Case("vu2", LinxV5::SIMT_VU2)
      .Case("vu3", LinxV5::SIMT_VU3)
      .Case("vu4", LinxV5::SIMT_VU4)
      .Case("vm1", LinxV5::SIMT_VM1)
      .Case("vm2", LinxV5::SIMT_VM2)
      .Case("vm3", LinxV5::SIMT_VM3)
      .Case("vm4", LinxV5::SIMT_VM4)
      .Case("vn1", LinxV5::SIMT_VN1)
      .Case("vn2", LinxV5::SIMT_VN2)
      .Case("vn3", LinxV5::SIMT_VN3)
      .Case("vn4", LinxV5::SIMT_VN4)
      .Case("ta", LinxV5::SIMT_TA)
      .Case("tb", LinxV5::SIMT_TB)
      .Case("tc", LinxV5::SIMT_TC)
      .Case("td", LinxV5::SIMT_TD)
      .Case("te", LinxV5::SIMT_TE)
      .Case("tf", LinxV5::SIMT_TF)
      .Case("tg", LinxV5::SIMT_TG)
      .Case("th", LinxV5::SIMT_TH)
      .Case("to", LinxV5::SIMT_TO)
      .Cases("to1", "ts", LinxV5::SIMT_TO1)
      .Case("to2", LinxV5::SIMT_TO2)
      .Case("to3", LinxV5::SIMT_TO3)
      .Default(LinxV5::NoRegister);
}

static unsigned MatchLinxV5LocalRegisteName(StringRef Name) {
  size_t DotPosition = Name.find('.');
  StringRef RegName = Name.slice(0, DotPosition);
  return StringSwitch<unsigned>(RegName.lower())
      .Case("t#1", LinxV5::TOS1)
      .Case("t#2", LinxV5::TOS2)
      .Case("t#3", LinxV5::TOS3)
      .Case("t#4", LinxV5::TOS4)
      .Case("u#1", LinxV5::UOS1)
      .Case("u#2", LinxV5::UOS2)
      .Case("u#3", LinxV5::UOS3)
      .Case("u#4", LinxV5::UOS4)
      .Case("t", LinxV5::T)
      .Case("u", LinxV5::U)
      .Case("tx2", LinxV5::TX2)
      .Case("ux2", LinxV5::UX2)
      .Case("tx4", LinxV5::TX4)
      .Case("ux4", LinxV5::UX4)
      .Default(LinxV5::NoRegister);
}

static unsigned MatchLinxV5DepRegisteName(StringRef Name) {
  return StringSwitch<unsigned>(Name.lower())
      .Case("d#1", LinxV5::Dep_DOS1)
      .Case("d#2", LinxV5::Dep_DOS2)
      .Case("d#3", LinxV5::Dep_DOS3)
      .Case("d#4", LinxV5::Dep_DOS4)
      .Case("d#5", LinxV5::Dep_DOS5)
      .Case("d#6", LinxV5::Dep_DOS6)
      .Case("d#7", LinxV5::Dep_DOS7)
      .Case("d#8", LinxV5::Dep_DOS8)
      .Default(LinxV5::NoRegister);
}

// Attempts to match Name as a register (either using the default name or
// alternative ABI names), setting RegNo to the matching register.
static bool matchRegisterNameHelper(MCRegister &RegNo, StringRef Name) {
  RegNo = MatchLinxV5GlobalRegisteName(Name);
  if (RegNo != LinxV5::NoRegister)
    return true;

  RegNo = MatchLinxV5LocalRegisteName(Name);
  if (RegNo != LinxV5::NoRegister)
    return true;

  return false;
}

// B.IOS has its own TSize map distinct from B.IOT: 0 selects the source
// form; 1..7 are destination capacities starting at 512 B (1=512B, 2=1KB,
// 3=2KB, 4=4KB, 5=8KB, 6=16KB, 7=32KB). B.IOT keeps the old map.
static unsigned matchBIOSTileSize(StringRef Name) {
  return StringSwitch<unsigned>(Name)
      .Case("0B", 0)
      .Case("512B", 1)
      .Case("1KB", 2)
      .Case("2KB", 3)
      .Case("4KB", 4)
      .Case("8KB", 5)
      .Case("16KB", 6)
      .Case("32KB", 7)
      .Default(16);
}

static unsigned matchTileSizeHelper(StringRef Name) {
  return StringSwitch<unsigned>(Name)
      .Case("0B", 0)
      .Case("128B", 1)
      .Case("256B", 2)
      .Case("512B", 3)
      .Case("1KB", 4)
      .Case("2KB", 5)
      .Case("4KB", 6)
      .Case("8KB", 7)
      .Default(16);
}

// Attempts to match Name as a SIMIT register (either using the default name or
// alternative ABI names), setting RegNo to the matching register.
static bool matchSIMTRegisterNameHelper(MCRegister &RegNo, StringRef Name) {
  RegNo = MatchLinxV5GlobalRegisteName(Name);
  if (RegNo != LinxV5::NoRegister)
    return true;

  RegNo = MatchLinxV5SIMTRegisterName(Name);
  if (RegNo != LinxV5::NoRegister)
    return true;

  return false;
}

static bool matchTileRegisterNameHelper(MCRegister &RegNo, StringRef Name) {
  RegNo = MatchLinxV5TileRegisteName(Name);
  return RegNo != LinxV5::NoRegister;
}

OperandMatchResultTy LinxV5AsmParser::tryParseRegister(unsigned &RegNo,
                                                       SMLoc &StartLoc,
                                                       SMLoc &EndLoc) {
  const AsmToken &Tok = getParser().getTok();
  StartLoc = Tok.getLoc();
  EndLoc = Tok.getEndLoc();
  RegNo = 0;
  StringRef Name = getLexer().getTok().getIdentifier();
  if (!matchRegisterNameHelper((MCRegister &)RegNo, Name))
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

/// Looks at a token type and creates the relevant operand from this
/// information, adding to Operands. If operand was parsed, returns false, else
/// true.
bool LinxV5AsmParser::parseOperand(OperandVector &Operands,
                                   StringRef Mnemonic) {
  // Check if the current operand has a custom associated parser, if so, try to
  // custom parse the operand, or fallback to the general approach.
  OperandMatchResultTy Result =
      MatchOperandParserImpl(Operands, Mnemonic, /*ParseForAllFeatures=*/true);
  if (Result == MatchOperand_Success)
    return false;
  if (Result == MatchOperand_ParseFail)
    return true;

  if (tryParseToken(Operands) == MatchOperand_Success)
    return false;

  // Attempt to parse token as a register.
  if (parseRegister(Operands) == MatchOperand_Success)
    return false;

  // Attempt to parse token as an immediate
  if (parseImmediate(Operands) == MatchOperand_Success)
    return false;

  if (getLexer().is(AsmToken::EndOfStatement))
    return false;

  // Finally we have exhausted all options and must declare defeat.
  Error(getLoc(), "unknown operand");
  return true;
}

OperandMatchResultTy LinxV5AsmParser::parseImmediate(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E;
  const MCExpr *Res;

  switch (getLexer().getKind()) {
  default:
    return MatchOperand_NoMatch;
  case AsmToken::LParen:
  case AsmToken::Minus:
  case AsmToken::Integer:
  case AsmToken::String:
  case AsmToken::Identifier:
    if (getParser().parseExpression(Res, E))
      return MatchOperand_ParseFail;
    break;
  case AsmToken::Percent:
    return parseOperandWithModifier(Operands);
  }

  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseRegister(OperandVector &Operands) {
  SMLoc FirstS = getLoc();
  AsmToken LParen;

  switch (getLexer().getKind()) {
  default:
    return MatchOperand_NoMatch;
  case AsmToken::Identifier:
    StringRef Name = getLexer().getTok().getIdentifier();
    MCRegister RegNo;
    matchRegisterNameHelper(RegNo, Name);

    if (RegNo == LinxV5::NoRegister)
      return MatchOperand_NoMatch;

    SMLoc S = getLoc();
    SMLoc E = SMLoc::getFromPointer(S.getPointer() + Name.size());
    getLexer().Lex();
    Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  }

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::tryParseToken(OperandVector &Operands) {
  SMLoc S = getLoc();

  if (getLexer().getKind() == AsmToken::LBrac) {
    getLexer().Lex();
    Operands.push_back(LinxV5Operand::createToken("[", S));
    return MatchOperand_Success;
  }
  if (getLexer().getKind() == AsmToken::RBrac) {
    getLexer().Lex();
    Operands.push_back(LinxV5Operand::createToken("]", S));
    return MatchOperand_Success;
  }

  if (getLexer().getKind() == AsmToken::Tilde) {
    getLexer().Lex();
    Operands.push_back(LinxV5Operand::createToken("~", S));
    return MatchOperand_Success;
  }

  if (getLexer().getKind() == AsmToken::Identifier) {
    if (getLexer().peekTok().getKind() == AsmToken::Exclaim) {
      getLexer().Lex();
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("sp", S));
      Operands.push_back(LinxV5Operand::createToken("!", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().upper() == "FALL") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("FALL", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().upper() == "IND") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("IND", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().upper() == "ICALL") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("ICALL", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().upper() == "RET") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("RET", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().upper() == "DIRECT") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("DIRECT", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().upper() == "CALL") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("CALL", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().upper() == "COND") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("COND", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "br.likely") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("BR.likely", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "br.unlikely") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("BR.unlikely", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "temp.none") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("TEMP.none", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "temp.cool") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("TEMP.cool", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "temp.warm") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("TEMP.warm", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "temp.hot") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("TEMP.hot", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "trace.begin") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("TRACE.begin", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "trace.end") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("TRACE.end", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "d") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("d", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier().lower() == "dr") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("DR", S));
      return MatchOperand_Success;
    } else if (getTok().getIdentifier() == "t#1") {
      Lex();
      SMLoc E = SMLoc::getFromPointer(S.getPointer());
      // NOTE: The llvm-tblgen treat a mark as a register as far as
      // possible. But the `t#1` refers to two registers `TOS1` and
      // `SIMT_OST1`. And according to the declare order, the `t#1`
      // mark is been treat as SIMT_OST1 in llvm-tblgen. So push the
      // SIMT_OST1 token.
      // It's OK because now the llvm-tblgen collect RegistersByName
      // in stable way.
      Operands.push_back(LinxV5Operand::createReg(LinxV5::SIMT_OST1, S, E));
      return MatchOperand_Success;
    }
    // lc0<<1 lc0<<2 lc0<<3
    else if (getLexer().getTok().getIdentifier().lower() == "lc0" &&
             getLexer().peekTok().is(AsmToken::LessLess)) {
      getLexer().Lex(); // consume lc0
      getLexer().Lex(); // consume <<
      if (getLexer().getTok().is(AsmToken::Integer)) {
        StringRef LSL = getLexer().getTok().getString();
        StringRef Tok(LSL.data() - 5, LSL.size() + 5);
        Operands.push_back(LinxV5Operand::createToken(Tok, S));
        getLexer().Lex(); // consume 1/2/3
        return MatchOperand_Success;
      }
    }
  } else if (getLexer().getKind() == AsmToken::MinusGreater) {
    AsmToken Tokken = getLexer().getTok();
    getLexer().Lex();
    if (getLexer().getTok().getIdentifier().lower() == "ra") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("->ra", S));
      return MatchOperand_Success;
    } else if (getLexer().getTok().getIdentifier().lower() == "t") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("->t", S));
      return MatchOperand_Success;
    } else if (getLexer().getTok().getIdentifier().lower() == "d") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("->d", S));
      return MatchOperand_Success;
    } else if (getLexer().getTok().getIdentifier().lower() == "p") {
      getLexer().Lex();
      Operands.push_back(LinxV5Operand::createToken("->p", S));
    } else {
      getLexer().UnLex(Tokken);
      return MatchOperand_NoMatch;
    }
  }

  if (getLexer().getKind() == AsmToken::LessLess &&
      getLexer().peekTok().getKind() == AsmToken::Integer) {
    getLexer().Lex();
    StringRef LSL = getLexer().getTok().getString();
    StringRef Tok(LSL.data() - 2, LSL.size() + 2);
    Operands.push_back(LinxV5Operand::createToken(Tok, S));
    getLexer().Lex();
    return MatchOperand_Success;
  }

  if (getLexer().getTok().getKind() == AsmToken::Greater) {
    getLexer().Lex();
    Operands.push_back(LinxV5Operand::createToken((">"), S));
    return MatchOperand_Success;
  }

  if (getLexer().getKind() == AsmToken::Less) {
    getLexer().Lex();
    StringRef maybeM = getLexer().getTok().getString();
    if (getLexer().isNot(AsmToken::EndOfStatement) &&
        getLexer().peekTok().getKind() == AsmToken::Colon &&
        !maybeM.str().compare("M")) {
      Operands.push_back(LinxV5Operand::createToken("<M:", S));
      getLexer().Lex(); // eat M
      getLexer().Lex(); // eat :
    } else if (getLexer().isNot(AsmToken::EndOfStatement) &&
               getLexer().peekTok().getKind() == AsmToken::Colon &&
               !maybeM.str().compare("Row")) {
      Operands.push_back(LinxV5Operand::createToken("<Row:", S));
      getLexer().Lex(); // eat Row
      getLexer().Lex(); // eat :
    } else if (getLexer().isNot(AsmToken::EndOfStatement) &&
               getLexer().peekTok().getKind() == AsmToken::Colon &&
               !maybeM.str().compare("LB0")) {
      Operands.push_back(LinxV5Operand::createToken("<LB0:", S));
      getLexer().Lex(); // eat Row
      getLexer().Lex(); // eat :
    }
    return MatchOperand_Success;
  }
  StringRef maybeNK = getLexer().getTok().getString();
  if (getLexer().isNot(AsmToken::EndOfStatement) &&
      getLexer().peekTok().getKind() == AsmToken::Colon) {
    if (!maybeNK.str().compare("N")) {
      Operands.push_back(LinxV5Operand::createToken("N:", S));
      getLexer().Lex(); // eat N
      getLexer().Lex(); // eat :
      return MatchOperand_Success;
    } else if (!maybeNK.str().compare("K")) {
      Operands.push_back(LinxV5Operand::createToken("K:", S));
      getLexer().Lex(); // eat K
      getLexer().Lex(); // eat :
      return MatchOperand_Success;
    } else if (!maybeNK.str().compare("Col")) {
      Operands.push_back(LinxV5Operand::createToken("Col:", S));
      getLexer().Lex(); // eat Col
      getLexer().Lex(); // eat :
      return MatchOperand_Success;
    } else if (!maybeNK.str().compare("LB1")) {
      Operands.push_back(LinxV5Operand::createToken("LB1:", S));
      getLexer().Lex(); // eat Col
      getLexer().Lex(); // eat :
      return MatchOperand_Success;
    } else if (!maybeNK.str().compare("LB2")) {
      Operands.push_back(LinxV5Operand::createToken("LB2:", S));
      getLexer().Lex(); // eat Col
      getLexer().Lex(); // eat :
      return MatchOperand_Success;
    }
  }

  return MatchOperand_NoMatch;
}

OperandMatchResultTy
LinxV5AsmParser::tryParseRegWithSrcRTypeImpl(OperandVector &Operands,
                                             StringRef Asm, SrcRType Type) {
  StringRef Str = getLexer().getTok().getString();
  size_t DotPosition = Str.find('.');

  StringRef RegName = Str.slice(0, DotPosition);
  StringRef CurType = Str.slice(DotPosition, Str.size());
  if (CurType.str().compare(Asm.str()))
    return MatchOperand_NoMatch;

  MCRegister RegNo;
  matchRegisterNameHelper(RegNo, RegName);
  if (RegNo != LinxV5::NoRegister) {
    SMLoc S = getLoc();
    SMLoc E = SMLoc::getFromPointer(S.getPointer());
    Operands.push_back(
        LinxV5Operand::createRegWithSrcRType(RegNo, Asm, Type, S, E));
    getParser().Lex(); // Eat identifier token.
    return MatchOperand_Success;
  }

  return MatchOperand_NoMatch;
}

OperandMatchResultTy LinxV5AsmParser::parseSIMTIntReg(OperandVector &Operands) {
  MCRegister RegNo;
  StringRef Str = getLexer().getTok().getString();
  auto SplitPair = splitSIMTReuseRegStr(Str);
  StringRef RegStr = SplitPair.first;
  StringRef UnLexStr = SplitPair.second;
  matchSIMTRegisterNameHelper(RegNo, RegStr);
  if (RegNo == LinxV5::NoRegister)
    return MatchOperand_NoMatch;

  if (UnLexStr.empty()) {
    if (LinxV5MCRegisterClasses[LinxV5::SIMT_TileBaseRegClassID].contains(
            RegNo) ||
        RegNo == LinxV5::SIMT_P) {
      SMLoc S = getLoc();
      SMLoc E = SMLoc::getFromPointer(S.getPointer());
      Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
      getParser().Lex(); // Eat identifier token.
      Operands.push_back(LinxV5Operand::createSIMTRegType(LinxV5Op::SIMT_INT_DST_REG_TYPE_D, S, E));
      return MatchOperand_Success;
    } else {
      return MatchOperand_NoMatch;
    }
  }
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getParser().Lex(); // Eat identifier token.
  // It is not easy to control Lex action.
  // unLex here, so that next operand parse can see the type.
  AsmToken TypeTok(AsmToken::Identifier, UnLexStr);
  getLexer().UnLex(TypeTok);
  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseSIMTRegOpAU(OperandVector &Operands) {
  return parseSIMTRegOp(Operands, ".neg");
}

OperandMatchResultTy LinxV5AsmParser::parseSIMTRegOpLU(OperandVector &Operands) {
  return parseSIMTRegOp(Operands, ".not");
}

OperandMatchResultTy LinxV5AsmParser::parsePadValue(OperandVector &Operands) {
  unsigned ParseValue;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  StringRef Identifier = getLexer().getTok().getIdentifier();
  ParseValue = StringSwitch<unsigned>(Identifier.lower())
                   .Case("zero", PadValue::Zero)
                   .Case("max", PadValue::Max)
                   .Case("min", PadValue::Min)
                   .Case("null", PadValue::Null)
                   .Default(PadValue::EMPTY_PadValue);

  if (ParseValue == PadValue::EMPTY_PadValue)
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  Operands.push_back(LinxV5Operand::createPadValueType(ParseValue, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseCmpMode(OperandVector &Operands) {
  unsigned ParseValue;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  StringRef Identifier = getLexer().getTok().getIdentifier();
  ParseValue = StringSwitch<unsigned>(Identifier.lower())
                   .Case("eq", CmpMode::EQ)
                   .Case("ne", CmpMode::NE)
                   .Case("lt", CmpMode::LT)
                   .Case("gt", CmpMode::GT)
                   .Case("le", CmpMode::LE)
                   .Case("ge", CmpMode::GE)
                   .Default(CmpMode::EMPTY_CmpMode);

  if (ParseValue == CmpMode::EMPTY_CmpMode)
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  Operands.push_back(LinxV5Operand::createCmpModeType(ParseValue, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseSIMTRegOp(OperandVector &Operands, StringRef suffix) {
  StringRef Str = getLexer().getTok().getString();
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  size_t DotPosition = Str.find('.');
  if (DotPosition == StringRef::npos) {
    Operands.push_back(LinxV5Operand::createSIMTRegOp(0, S, E));
    return MatchOperand_Success;
  }

  if (Str.lower() != suffix) {
    return llvm::MatchOperand_NoMatch;
  }
  Operands.push_back(LinxV5Operand::createSIMTRegOp(0b11, S, E));
  getParser().Lex();
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseSIMTDstRWithArrow(OperandVector &Operands) {
  StringRef Str = getLexer().getTok().getString();
  if (!Str.str().compare("->")) {
    Str = getLexer().peekTok().getIdentifier();
  } else {
    return MatchOperand_NoMatch;
  }

  MCRegister RegNo;
  auto SplitPair = splitSIMTReuseRegStr(Str);
  StringRef RegStr = SplitPair.first;
  StringRef UnLexStr = SplitPair.second;
  matchSIMTRegisterNameHelper(RegNo, RegStr);
  if (RegNo == LinxV5::NoRegister)
    return MatchOperand_NoMatch;
  if (UnLexStr.empty()) {
    if (RegNo == LinxV5::SIMT_P) {
      SMLoc S = getLoc();
      SMLoc E = SMLoc::getFromPointer(S.getPointer());
      Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
      getLexer().Lex();  // consume '->'
      getParser().Lex(); // Eat identifier token.
      Operands.push_back(LinxV5Operand::createSIMTRegType(
          LinxV5Op::SIMT_INT_DST_REG_TYPE_D, S, E));
      return MatchOperand_Success;
    } else {
      return MatchOperand_NoMatch;
    }
  }
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getLexer().Lex();  // consume '->'
  getParser().Lex(); // Eat identifier token.
  // It is not easy to control Lex action.
  // unLex here, so that next operand parse can see the type.
  AsmToken TypeTok(AsmToken::Identifier, UnLexStr);
  getLexer().UnLex(TypeTok);
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseSIMTDstVecRWithArrow(OperandVector &Operands) {
  StringRef Str = getLexer().getTok().getString();
  if (!Str.str().compare("->")) {
    Str = getLexer().peekTok().getIdentifier();
  } else {
    return MatchOperand_NoMatch;
  }

  MCRegister RegNo;
  auto SplitPair = splitSIMTReuseRegStr(Str);
  StringRef RegStr = SplitPair.first;
  StringRef UnLexStr = SplitPair.second;
  matchSIMTRegisterNameHelper(RegNo, RegStr);
  if (RegNo < LinxV5::SIMT_VM || RegNo > LinxV5::SIMT_VU)
    return MatchOperand_NoMatch;
  if (UnLexStr.empty())
    return MatchOperand_NoMatch;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getLexer().Lex();  // consume '->'
  getParser().Lex(); // Eat identifier token.
  // It is not easy to control Lex action.
  // unLex here, so that next operand parse can see the type.
  AsmToken TypeTok(AsmToken::Identifier, UnLexStr);
  getLexer().UnLex(TypeTok);
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseTileReg(OperandVector &Operands) {
  StringRef Str = getLexer().getTok().getString();
  MCRegister RegNo;
  matchTileRegisterNameHelper(RegNo, Str);
  if (RegNo == LinxV5::NoRegister)
    return MatchOperand_NoMatch;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getLexer().Lex();  // consume '->'
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseTileRegWithArrow(OperandVector &Operands) {
  StringRef Str = getLexer().getTok().getString();
  if (!Str.str().compare("->")) {
    Str = getLexer().peekTok().getIdentifier();
  } else {
    return MatchOperand_NoMatch;
  }
  MCRegister RegNo;
  matchTileRegisterNameHelper(RegNo, Str);
  if (RegNo == LinxV5::NoRegister)
    return MatchOperand_NoMatch;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getLexer().Lex();  // consume '->'
  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseGroupOp(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  StringRef Str = getLexer().getTok().getString();
  if (!Str.lower().compare("last")) {
    Operands.push_back(LinxV5Operand::createGroupOp(1, S, E));
    getLexer().Lex(); // consume 'last'
  } else {
    return MatchOperand_NoMatch;
  }
  // all success
  return MatchOperand_Success;
}

// PTO v0.58 reissue: parse destination Shared ID "->S17" for the 32-bit
// B.IOS destination form. Consumes an optional leading "->" arrow and the
// absolute "S17" ID (rejecting the retired "S#n" spelling). The arrow is kept
// in the operand so a single SharedIDWithArrow operand class serves both the
// source ("S17") and destination ("->S17") text forms.
OperandMatchResultTy
LinxV5AsmParser::parseSharedTIDWithArrow(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  StringRef Str = getLexer().getTok().getString();
  if (Str == "->") {
    getLexer().Lex();  // consume '->'
    Str = getLexer().getTok().getString();
  }
  if (Str.empty())
    Str = getLexer().getTok().getIdentifier();
  if (!Str.startswith_insensitive("s"))
    return MatchOperand_NoMatch;
  if (Str.size() > 1 && Str[1] == '#')
    return MatchOperand_NoMatch;
  StringRef NumStr = Str.substr(1);
  if (NumStr.empty())
    return MatchOperand_NoMatch;
  unsigned TID;
  if (NumStr.getAsInteger(10, TID))
    return MatchOperand_NoMatch;
  if (TID > 255)
    return MatchOperand_NoMatch;
  const MCExpr *Val = MCConstantExpr::create(TID, getParser().getContext());
  Operands.push_back(LinxV5Operand::createImm(Val, S, E));
  getLexer().Lex();  // consume 'S17'
  return MatchOperand_Success;
}

// PTO v0.58 reissue: parse absolute Shared architectural ID "S0".."S255"
// (no '#' prefix) for the 32-bit B.IOS binder operand. The leading "->" of a
// destination ("->S17") is matched as a separate literal token by the AsmMatcher,
// so this method only consumes the bare "S17". The retired "S#n" spelling is
// rejected.
OperandMatchResultTy
LinxV5AsmParser::parseSharedTID(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  auto &Tok = getLexer().getTok();
  StringRef Str = Tok.getString();
  if (Str.empty())
    Str = Tok.getIdentifier();
  if (!Str.startswith_insensitive("s"))
    return MatchOperand_NoMatch;
  // Reject the retired "S#n" spelling.
  if (Str.size() > 1 && Str[1] == '#')
    return MatchOperand_NoMatch;
  StringRef NumStr = Str.substr(1);
  if (NumStr.empty())
    return MatchOperand_NoMatch;
  unsigned TID;
  if (NumStr.getAsInteger(10, TID))
    return MatchOperand_NoMatch;
  if (TID > 255)
    return MatchOperand_NoMatch;
  const MCExpr *Val = MCConstantExpr::create(TID, getParser().getContext());
  Operands.push_back(LinxV5Operand::createImm(Val, S, E));
  getLexer().Lex();  // consume 'S17'
  return MatchOperand_Success;
}

// v5: parse "mask=N" where N is a 4-bit decimal (0..15).
OperandMatchResultTy
LinxV5AsmParser::parsePE_MASK(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  StringRef Str = getLexer().getTok().getString();
  if (!Str.startswith_insensitive("mask"))
    return MatchOperand_NoMatch;
  // consume "mask"
  getLexer().Lex();
  // expect "="
  if (getLexer().getTok().getKind() != AsmToken::Equal)
    return MatchOperand_ParseFail;
  getLexer().Lex(); // consume "="
  // expect integer
  if (getLexer().getTok().getKind() != AsmToken::Integer)
    return MatchOperand_ParseFail;
  // PTO v0.58 reissue: PE_MASK is a 4-bit binary spelling ("mask=0011" is
  // the bit pattern 0011 = 3, not octal/decimal 11). Parse the token as a
  // binary digit string.
  unsigned Val = 0;
  StringRef MaskStr = getLexer().getTok().getString();
  // Strip any leading "0b".
  if (MaskStr.startswith_insensitive("0b"))
    MaskStr = MaskStr.substr(2);
  for (char C : MaskStr) {
    if (C != '0' && C != '1')
      return MatchOperand_ParseFail;
    Val = (Val << 1) | (C - '0');
    if (Val > 15)
      return MatchOperand_ParseFail;
  }
  getLexer().Lex(); // consume integer
  const MCExpr *Expr = MCConstantExpr::create(Val, getParser().getContext());
  Operands.push_back(LinxV5Operand::createImm(Expr, S, E));
  return MatchOperand_Success;
}

// v5: parse "TSize=N" where N is a 3-bit decimal (0..7).
OperandMatchResultTy
LinxV5AsmParser::parseTSize(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  StringRef Str = getLexer().getTok().getString();
  if (!Str.startswith_insensitive("tsize"))
    return MatchOperand_NoMatch;
  getLexer().Lex(); // consume "TSize"
  if (getLexer().getTok().getKind() != AsmToken::Equal)
    return MatchOperand_ParseFail;
  getLexer().Lex(); // consume "="
  if (getLexer().getTok().getKind() != AsmToken::Integer)
    return MatchOperand_ParseFail;
  unsigned Val = getLexer().getTok().getIntVal();
  if (Val > 7)
    return MatchOperand_ParseFail;
  getLexer().Lex(); // consume integer
  const MCExpr *Expr = MCConstantExpr::create(Val, getParser().getContext());
  Operands.push_back(LinxV5Operand::createImm(Expr, S, E));
  return MatchOperand_Success;
}

// E.g in "R1+32" / "R1" / "32"
//    eat "R1+"   / "R1" / ""
OperandMatchResultTy LinxV5AsmParser::parseGPRPlusImm(OperandVector &Operands) {
  if (getLexer().getTok().getKind() == AsmToken::Integer) {
    SMLoc S = getLoc();
    SMLoc E = SMLoc::getFromPointer(S.getPointer());
    // give zero to Reg
    Operands.push_back(LinxV5Operand::createReg(LinxV5::R0, S, E));
    return MatchOperand_Success;
  }

  StringRef RegName = getLexer().getTok().getString(); // Reg
  MCRegister RegNo;
  matchRegisterNameHelper(RegNo, RegName);
  if (RegNo == LinxV5::NoRegister)
    return MatchOperand_ParseFail;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getLexer().Lex(); // eat Reg

  if (getLexer().getTok().getKind() == AsmToken::Plus) {
    getLexer().Lex(); // eat "+"
    return MatchOperand_Success;
  }
  // give 0 to imm
  const MCExpr *Res = MCConstantExpr::create(0, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  return MatchOperand_Success;
}

static unsigned calculateDRImm(StringRef Identifier) {
  unsigned Format = StringSwitch<unsigned>(Identifier.lower())
                        .Case("mr", DREnum::MR)
                        .Case("dr", DREnum::DR)
                        .Default(DREnum::EMPTY_DREnum);
  return Format;
}

OperandMatchResultTy LinxV5AsmParser::parseDRImm(OperandVector &Operands) {
  unsigned Ret = DREnum::EMPTY_DREnum;
  StringRef Str = getLexer().getTok().getString();
  Ret = calculateDRImm(Str);
  if (Ret == DREnum::EMPTY_DREnum)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Str.size());
  Operands.push_back(LinxV5Operand::createDRImmType(Ret, S, E));
  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseGPRList(OperandVector &Operands) {
  SmallVector<MCRegister> GPRList;
  unsigned Size = 0;
  bool isStop = false;

  StringRef RegName;

  do {
    switch (getLexer().getKind()) {
    default:
      return MatchOperand_NoMatch;

    case AsmToken::String:
    case AsmToken::Identifier: {
      RegName = getLexer().getTok().getIdentifier();
      MCRegister RegNo;
      matchRegisterNameHelper(RegNo, RegName);
      if (RegNo == LinxV5::NoRegister ||
          !(RegNo <= LinxV5::R23 && RegNo > LinxV5::R0))
        return MatchOperand_NoMatch;
      GPRList.push_back(RegNo);
      Size += RegName.size();
      getParser().Lex(); // Eat RegName token.
      continue;
    }
    case AsmToken::Comma: {
      getLexer().Lex();
      Size += 1;
      continue;
    }
    case AsmToken::RBrac: {
      isStop = true;
      break;
    }
    }
  } while (!isStop && getLexer().isNot(AsmToken::EndOfStatement));

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Size);
  Operands.push_back(LinxV5Operand::createVariableOp(GPRList, S, E));
  if (getLexer().getKind() == AsmToken::RBrac) {
    getLexer().Lex();
    Operands.push_back(LinxV5Operand::createToken("]", S));
  }
  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parsePlusImm17(OperandVector &Operands) {
  int64_t imm = getLexer().getTok().getIntVal();
  const MCExpr *Res = MCConstantExpr::create(imm, getContext());
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  getLexer().Lex();
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseGPRWithBracket(OperandVector &Operands) {
  StringRef Str = getLexer().getTok().getString();
  if (!Str.str().compare("<")) {
    getLexer().Lex(); // consume '<'
  } else {
    return MatchOperand_NoMatch;
  }
  StringRef RegName = getLexer().getTok().getString();
  MCRegister RegNo;
  matchRegisterNameHelper(RegNo, RegName);
  if (RegNo == LinxV5::NoRegister) {
    getLexer().UnLex(AsmToken(AsmToken::Less, "<"));
    return MatchOperand_NoMatch;
  }
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getParser().Lex(); // Eat identifier token.
  if (getLexer().peekTok().getString().str().compare(">")) {
    getLexer().Lex(); // consume '>'
  }
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseBstartDataType(OperandVector &Operands) {
  unsigned DataType;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());

  if (getLexer().getTok().is(AsmToken::Integer)) {
    unsigned Val = getLexer().getTok().getIntVal();
    // PTO 0.58.1: code 31 is DTYPE_NONE (inheritance sentinel);
    // codes 15, 21..23, 29..30 are reserved.
    if (Val > 31 || Val == 15 || (Val >= 21 && Val <= 23) ||
        Val == 29 || Val == 30)
      return MatchOperand_NoMatch;

    getParser().Lex();
    const MCExpr *Res = MCConstantExpr::create(Val, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));
    return MatchOperand_Success;
  }

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  StringRef Identifier = getLexer().getTok().getIdentifier();
  DataType = StringSwitch<unsigned>(Identifier.lower())
                 .Case("fp64", DataType::FP64)
                 .Case("fp32", DataType::FP32)
                 .Case("tf32", DataType::TF32)
                 .Case("hf32", DataType::HF32)
                 .Case("fp16", DataType::FP16)
                 .Case("bf16", DataType::BF16)
                 .Case("hif8", DataType::HiF8)
                 .Case("e4m3", DataType::e4m3)
                 .Case("e5m2", DataType::e5m2)
                 .Case("e3m2", DataType::e3m2)
                 .Case("e2m3", DataType::e2m3)
                 .Case("e2m1x2", DataType::e2m1x2)
                 .Case("e1m2x2", DataType::e1m2x2)
                 .Case("e8m0", DataType::e8m0)
                 .Case("hif4x2", DataType::HiF4x2)
                 .Case("s64", DataType::S64)
                 .Case("s32", DataType::S32)
                 .Case("s16", DataType::S16)
                 .Case("s8", DataType::S8)
                 .Case("s4x2", DataType::S4x2)
                 .Case("u64", DataType::U64)
                 .Case("u32", DataType::U32)
                 .Case("u16", DataType::U16)
                 .Case("u8", DataType::U8)
                 .Case("u4x2", DataType::U4x2)
                 .Case("dtype_none", DataType::EMPTY_DataType)
                 .Default(DataType::EMPTY_DataType);

  if (DataType == DataType::EMPTY_DataType &&
      Identifier.lower() != "dtype_none")
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  const MCExpr *Res = MCConstantExpr::create(DataType, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseTileOPTMA(OperandVector &Operands) {
  unsigned TileOP;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  bool ConsumedCompoundName = false;
  if (getLexer().getTok().is(AsmToken::EndOfStatement))
    return MatchOperand_Success;

  if (getLexer().getTok().is(AsmToken::Integer)) {
    unsigned long long Val = getLexer().getTok().getIntVal();
    getParser().Lex();
    const MCExpr *Res = MCConstantExpr::create(Val, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));

    return MatchOperand_Success;
  }

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  std::string Identifier = getLexer().getTok().getIdentifier().lower();
  SmallVector<AsmToken, 2> PeekToks;
  if (getLexer().peekTokens(PeekToks) == 2 && PeekToks[0].is(AsmToken::Dot) &&
      (PeekToks[1].is(AsmToken::Identifier) ||
       PeekToks[1].is(AsmToken::String))) {
    Identifier += ".";
    Identifier += PeekToks[1].getString().lower();
    getParser().Lex(); // Eat base identifier token.
    getParser().Lex(); // Eat '.'.
    getParser().Lex(); // Eat suffix identifier token.
    ConsumedCompoundName = true;
  }

  TileOP = StringSwitch<unsigned>(Identifier)
               .Case("tmov", TileOPTMA::TMOV)
               .Case("tload", TileOPTMA::TLOAD)
               .Case("tstore", TileOPTMA::TSTORE)
               .Case("tprefetch", TileOPTMA::TPREFETCH)
               .Case("mgather", TileOPTMA::MGATHER)
               .Case("mscatter", TileOPTMA::MSCATTER)
               .Case("mgather.mask", TileOPTMA::MGATHER_MASK)
               .Case("mscatter.mask", TileOPTMA::MSCATTER_MASK)
               // PTO v0.58 TLSU Function 8-14.
               .Case("mgather.cas", TileOPTMA::MGATHER_CAS)
               .Case("tmov.l2s.insert", TileOPTMA::TMOV_L2S_INSERT)
               .Case("tmov.l2s.publish", TileOPTMA::TMOV_L2S_PUBLISH)
               .Case("tmov.s2l.broadcast", TileOPTMA::TMOV_S2L_BROADCAST)
               .Case("tmov.s2l.extract", TileOPTMA::TMOV_S2L_EXTRACT)
               .Case("tstore.spart", TileOPTMA::TSTORE_SPART)
               .Case("gmov", TileOPTMA::GMOV)
               .Default(TileOPTMA::EMPTY_TileOPTMA);

  if (TileOP == TileOPTMA::EMPTY_TileOPTMA) {
    std::string OpName = Identifier;
    std::string ErrorMsg = "TileOP '" + OpName + "' Not supported yet, please use numeric codes for entry.";
    Error(getLexer().getLoc(), ErrorMsg);
    return MatchOperand_ParseFail;
  }

  if (!ConsumedCompoundName)
    getParser().Lex(); // Eat identifier token.
  const MCExpr *Res = MCConstantExpr::create(TileOP, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseTileOPCUBE(OperandVector &Operands) {
  unsigned TileOP;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  if (getLexer().getTok().is(AsmToken::EndOfStatement))
    return MatchOperand_Success;

  if (getLexer().getTok().is(AsmToken::Integer)) {
    unsigned long long Val = getLexer().getTok().getIntVal();
    getParser().Lex();
    const MCExpr *Res = MCConstantExpr::create(Val, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));

    return MatchOperand_Success;
  }

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  StringRef Identifier = getLexer().getTok().getIdentifier();
  TileOP = StringSwitch<unsigned>(Identifier.lower())
               .Case("tmatmul", TileOPCUBE::MAMULB)
               .Case("tmatmul.bias", TileOPCUBE::MAMULBAC)
               .Case("tmatmul.acc", TileOPCUBE::MAMULB_ACC)
               .Case("tmatmulmx", TileOPCUBE::MAMULBMX)
               .Case("tmatmulmx.bias", TileOPCUBE::MAMULBMXAC)
               .Case("tmatmulmx.acc", TileOPCUBE::MAMULBMX_ACC)
               .Case("tgemv", TileOPCUBE::TGEMV)
               .Case("tgemv.bias", TileOPCUBE::TGEMV_BIAS)
               .Case("tgemv.acc", TileOPCUBE::TGEMV_ACC)
               .Case("tgemvmx", TileOPCUBE::TGEMVMX)
               .Case("tgemvmx.bias", TileOPCUBE::TGEMVMX_BIAS)
               .Case("tgemvmx.acc", TileOPCUBE::TGEMVMX_ACC)
               .Default(TileOPCUBE::EMPTY_TileOPCUBE);

  if (TileOP == TileOPCUBE::EMPTY_TileOPCUBE)
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  const MCExpr *Res = MCConstantExpr::create(TileOP, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseTileOPTEPL(OperandVector &Operands) {
  unsigned TileOP;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  if (getLexer().getTok().is(AsmToken::EndOfStatement))
    return MatchOperand_Success;

  if (getLexer().getTok().is(AsmToken::Integer)) {
    unsigned long long Val = getLexer().getTok().getIntVal();
    getParser().Lex();
    const MCExpr *Res = MCConstantExpr::create(Val, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));

    return MatchOperand_Success;
  }

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  StringRef Identifier = getLexer().getTok().getIdentifier();
  TileOP = StringSwitch<unsigned>(Identifier.lower())
                              .Case("esave", TileOPTEPL::ESAVE)
               .Case("mamulbac", TileOPTEPL::ERCOV)
               .Case("tadd", 0)
               .Case("tsub", 1)
               .Case("tmul", 2)
               .Case("tdiv", 3)
               .Case("trem", 4)
               .Case("tand", 6)
               .Case("tor", 7)
               .Case("txor", 8)
               .Case("tshl", 9)
               .Case("tshr", 10)
               .Case("tmax", 11)
               .Case("tmin", 12)
               .Case("tcmp", 13)
               .Case("tabs", 15)
               .Case("tnot", 16)
               .Case("tneg", 17)
               .Case("texp", 18)
               .Case("tlog", 19)
               .Case("trecip", 20)
               .Case("tsqrt", 21)
               .Case("trsqrt", 22)
               .Case("trelu", 23)
               .Case("tsel", 26)
               .Case("tcvt", 27)
               .Case("tfma", 28)
               .Case("tadds", 32)
               .Case("tsubs", 33)
               .Case("tmuls", 34)
               .Case("tdivs", 35)
               .Case("trems", 36)
               .Case("tands", 38)
               .Case("tors", 39)
               .Case("txors", 40)
               .Case("tshls", 41)
               .Case("tshrs", 42)
               .Case("tmaxs", 43)
               .Case("tmins", 44)
               .Case("tcmps", 45)
               .Case("tsels", 58)
               .Case("texpands", 59)
               .Case("trowsum", 64)
               .Case("trowmax", 65)
               .Case("trowmin", 66)
               .Case("trowprod", 67)
               .Case("trowexpand", 68)
               .Case("trowexpandadd", 69)
               .Case("trowexpandsub", 70)
               .Case("trowexpandmul", 71)
               .Case("trowexpanddiv", 72)
               .Case("trowexpandmax", 73)
               .Case("trowexpandmin", 74)
               .Case("trowexpandexpdif", 75)
               .Case("trowargmax", 76)
               .Case("trowargmin", 77)
               .Case("tcolsum", 80)
               .Case("tcolmax", 81)
               .Case("tcolmin", 82)
               .Case("tcolprod", 83)
               .Case("tcolexpand", 84)
               .Case("tcolexpandadd", 85)
               .Case("tcolexpandsub", 86)
               .Case("tcolexpandmul", 87)
               .Case("tcolexpanddiv", 88)
               .Case("tcolexpandmax", 89)
               .Case("tcolexpandmin", 90)
               .Case("tcolexpandexpdif", 91)
               .Case("tcolargmax", 92)
               .Case("tcolargmin", 93)
               .Case("tconcat", 96)
               .Case("tconcat", 96)
               .Case("textract", 98)
               .Case("tinsert", 99)
               .Case("timg2col", 100)
               .Case("tfillpad", 101)
               .Case("tci", 102)
               .Case("ttri", 103)
               .Case("thistogram", 104)
               .Case("tquant", 106)
               .Case("tdequant", 107)
               .Case("tsort", 108)
               .Case("tmrgsort", 109)
               .Case("ttrans", 110)
               .Case("tgather", 111)
               .Case("tscatter", 112)
               .Case("tpartadd", 113)
               .Case("tpartmul", 114)
               .Case("tpartmax", 115)
               .Case("tpartmin", 116)
               .Default(TileOPTEPL::EMPTY_TileOPTEPL);

  if (TileOP == TileOPTEPL::EMPTY_TileOPTEPL)
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  const MCExpr *Res = MCConstantExpr::create(TileOP, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseTileOPMode(OperandVector &Operands) {
  unsigned TileOP;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  StringRef Identifier = getLexer().getTok().getIdentifier();
  TileOP = StringSwitch<unsigned>(Identifier.lower())
               .Case("vs8", TileOPMode::VS8)
               .Case("vs16", TileOPMode::VS16)
               .Case("vs32", TileOPMode::VS32)
               .Case("vs64", TileOPMode::VS64)
               .Default(TileOPMode::EMPTY_TileOPMode);

  if (TileOP == TileOPMode::EMPTY_TileOPMode)
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  const MCExpr *Res = MCConstantExpr::create(TileOP, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseTEPLMode(OperandVector &Operands) {
  unsigned TileOP;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  StringRef Identifier = getLexer().getTok().getIdentifier();
  TileOP = StringSwitch<unsigned>(Identifier.lower())
               .Case("gprs", TEPLMode::gprs)
               .Case("tile", TEPLMode::tile)
               .Default(TEPLMode::EMPTY_TEPLMode);

  if (TileOP == TEPLMode::EMPTY_TEPLMode)
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  const MCExpr *Res = MCConstantExpr::create(TileOP, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseTileSizeWithBracket(OperandVector &Operands) {
  const MCExpr *ResSymbol;
  StringRef Str = getLexer().getTok().getString();
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  if (!Str.str().compare("<")) {
    getLexer().Lex(); // consume '<'
  } else {
    return MatchOperand_NoMatch;
  }

  // Regarding the situation where tilesize is represented using an enum in inline assembly.
  if (getLexer().getTok().is(AsmToken::Integer) && !getLexer().peekTok().getString().str().compare(">")) {
    unsigned Val = getLexer().getTok().getIntVal();

    if (Val >= 0b10000) {
      getLexer().UnLex(AsmToken(AsmToken::Less, "<"));
      return MatchOperand_NoMatch;
    }

    const MCExpr *Res = MCConstantExpr::create(Val, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));
    getParser().Lex(); //consume enum
    // After consuming the enum value, the current token should be '>'.
    if (!getLexer().getTok().getString().str().compare(">")) {
      getLexer().Lex(); // consume '>'
    }
    return MatchOperand_Success;
  }

  StringRef sizeImm = getLexer().getTok().getString();
  if (!sizeImm.lower().compare("zero")) {
    const MCExpr *Res = MCConstantExpr::create(16, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));
    getLexer().Lex(); // consume zero
    if (!getLexer().getTok().getString().str().compare(">")) {
      getLexer().Lex(); // consume '>'
    }
    return MatchOperand_Success;
  }

  if (getLexer().getKind() == AsmToken::Integer) {
    StringRef sizeUnits = getLexer().peekTok().getString();
    // such as 8kb...
    Str = StringRef((sizeImm + sizeUnits).str());
    unsigned Result = matchTileSizeHelper(Str);
    if (Result >= 0b10000) {
      getLexer().UnLex(AsmToken(AsmToken::Less, "<"));
      return MatchOperand_NoMatch;
    }
    const MCExpr *Res = MCConstantExpr::create(Result, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));
    getLexer().Lex(); // consume imm
    getLexer().Lex(); // comsum size unit
    if (!getLexer().getTok().getString().str().compare(">")) {
      getLexer().Lex(); // consume '>'
    }
  } else {
    if (getLexer().getKind() == AsmToken::Identifier &&
        getLexer().peekTok().is(AsmToken::Greater)) {
      llvm::AsmToken Token = getLexer().getTok();
      getLexer().Lex(); // consume Token
      getLexer().Lex(); // consume '>'
      getLexer().UnLex(Token);
      getParser().parseExpression(ResSymbol, E);
    } else {
      getLexer().UnLex(AsmToken(AsmToken::Less, "<"));
      return MatchOperand_NoMatch;
    }
    Operands.push_back(LinxV5Operand::createImm(ResSymbol, S, E));
  }
  return MatchOperand_Success;
}
OperandMatchResultTy LinxV5AsmParser::parseBIOSTileSizeWithBracket(OperandVector &Operands) {
  const MCExpr *ResSymbol;
  StringRef Str = getLexer().getTok().getString();
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  if (!Str.str().compare("<")) {
    getLexer().Lex(); // consume '<'
  } else {
    return MatchOperand_NoMatch;
  }

  // Regarding the situation where tilesize is represented using an enum in inline assembly.
  if (getLexer().getTok().is(AsmToken::Integer) && !getLexer().peekTok().getString().str().compare(">")) {
    unsigned Val = getLexer().getTok().getIntVal();

    if (Val >= 0b10000) {
      getLexer().UnLex(AsmToken(AsmToken::Less, "<"));
      return MatchOperand_NoMatch;
    }

    const MCExpr *Res = MCConstantExpr::create(Val, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));
    getParser().Lex(); //consume enum
    // After consuming the enum value, the current token should be '>'.
    if (!getLexer().getTok().getString().str().compare(">")) {
      getLexer().Lex(); // consume '>'
    }
    return MatchOperand_Success;
  }

  StringRef sizeImm = getLexer().getTok().getString();
  if (!sizeImm.lower().compare("zero")) {
    const MCExpr *Res = MCConstantExpr::create(16, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));
    getLexer().Lex(); // consume zero
    if (!getLexer().getTok().getString().str().compare(">")) {
      getLexer().Lex(); // consume '>'
    }
    return MatchOperand_Success;
  }

  if (getLexer().getKind() == AsmToken::Integer) {
    StringRef sizeUnits = getLexer().peekTok().getString();
    // such as 8kb...
    Str = StringRef((sizeImm + sizeUnits).str());
    unsigned Result = matchBIOSTileSize(Str);
    if (Result >= 0b10000) {
      getLexer().UnLex(AsmToken(AsmToken::Less, "<"));
      return MatchOperand_NoMatch;
    }
    const MCExpr *Res = MCConstantExpr::create(Result, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));
    getLexer().Lex(); // consume imm
    getLexer().Lex(); // comsum size unit
    if (!getLexer().getTok().getString().str().compare(">")) {
      getLexer().Lex(); // consume '>'
    }
  } else {
    if (getLexer().getKind() == AsmToken::Identifier &&
        getLexer().peekTok().is(AsmToken::Greater)) {
      llvm::AsmToken Token = getLexer().getTok();
      getLexer().Lex(); // consume Token
      getLexer().Lex(); // consume '>'
      getLexer().UnLex(Token);
      getParser().parseExpression(ResSymbol, E);
    } else {
      getLexer().UnLex(AsmToken(AsmToken::Less, "<"));
      return MatchOperand_NoMatch;
    }
    Operands.push_back(LinxV5Operand::createImm(ResSymbol, S, E));
  }
  return MatchOperand_Success;
}
static void splitSIMTVLenSuffix(StringRef &LexStr, StringRef &VLenSuffix) {
  VLenSuffix = StringRef();

  if (LexStr.size() <= 2)
    return;

  StringRef Tail = LexStr.substr(LexStr.size() - 2);
  std::string TailLower = Tail.lower();

  if (TailLower == "x2") {
    LexStr = LexStr.substr(0, LexStr.size() - 2);
    VLenSuffix = "x2";
    return;
  }

  if (TailLower == "x4") {
    LexStr = LexStr.substr(0, LexStr.size() - 2);
    VLenSuffix = "x4";
    return;
  }
}

OperandMatchResultTy
LinxV5AsmParser::parseSIMTIntSrcRegType(OperandVector &Operands) {
  StringRef Str = getLexer().getTok().getString();
  size_t firstDot = Str.find('.');
  if (firstDot == StringRef::npos)
    return MatchOperand_NoMatch;

  size_t secondDot = Str.find('.', firstDot + 1);
  StringRef LexStr = Str;
  StringRef UnlexStr;
  if (secondDot != StringRef::npos) {
    LexStr = Str.substr(0, secondDot);
    UnlexStr = Str.substr(secondDot, Str.size());
  }

  StringRef VLenSuffix;
  splitSIMTVLenSuffix(LexStr, VLenSuffix);

  unsigned SIMTRegType =
      StringSwitch<unsigned>(LexStr.lower())
          .Case(".ud", LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD)
          .Case(".uw", LinxV5Op::SIMT_INT_SRC_REG_TYPE_UW)
          .Case(".uh", LinxV5Op::SIMT_INT_SRC_REG_TYPE_UH)
          .Case(".ub", LinxV5Op::SIMT_INT_SRC_REG_TYPE_UB)
          .Case(".sd", LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD)
          .Case(".sw", LinxV5Op::SIMT_INT_SRC_REG_TYPE_SW)
          .Case(".sh", LinxV5Op::SIMT_INT_SRC_REG_TYPE_SH)
          .Case(".sb", LinxV5Op::SIMT_INT_SRC_REG_TYPE_SB)
          .Default(LinxV5Op::SIMT_INT_SRC_REG_TYPE_NONE);

  if (SIMTRegType == LinxV5Op::SIMT_INT_SRC_REG_TYPE_NONE)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + LexStr.size());

  Operands.push_back(LinxV5Operand::createSIMTRegType(SIMTRegType, S, E));

  if (!VLenSuffix.empty()) {
    SMLoc SuffixLoc = SMLoc::getFromPointer(S.getPointer() + LexStr.size());
    Operands.push_back(LinxV5Operand::createToken(VLenSuffix, SuffixLoc));
  }

  getParser().Lex(); // Eat identifier token.

  if (!UnlexStr.empty())
    getLexer().UnLex(AsmToken(AsmToken::Identifier, UnlexStr));

  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseSIMTFloatSrcRegType(OperandVector &Operands) {
  StringRef Str = getLexer().getTok().getString();
  size_t firstDot = Str.find('.');
  if (firstDot == StringRef::npos)
    return MatchOperand_NoMatch;

  size_t secondDot = Str.find('.', firstDot + 1);
  StringRef LexStr = Str;
  StringRef UnlexStr;
  if (secondDot != StringRef::npos) {
    LexStr = Str.substr(0, secondDot);
    UnlexStr = Str.substr(secondDot, Str.size());
  }

  StringRef VLenSuffix;
  splitSIMTVLenSuffix(LexStr, VLenSuffix);

  unsigned SIMTRegType =
      StringSwitch<unsigned>(LexStr.lower())
          .Case(".fd", LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FD)
          .Case(".fs", LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FS)
          .Case(".fh", LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FH)
          .Case(".fb", LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FB)
          .Case(".bf", LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_BF)
          .Case(".flb", LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FLB)
          .Default(LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_NONE);

  if (SIMTRegType == LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_NONE)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + LexStr.size());

  Operands.push_back(LinxV5Operand::createSIMTRegType(SIMTRegType, S, E));

  if (!VLenSuffix.empty()) {
    SMLoc SuffixLoc = SMLoc::getFromPointer(S.getPointer() + LexStr.size());
    Operands.push_back(LinxV5Operand::createToken(VLenSuffix, SuffixLoc));
  }

  getParser().Lex(); // Eat identifier token.

  if (!UnlexStr.empty())
    getLexer().UnLex(AsmToken(AsmToken::Identifier, UnlexStr));

  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseSIMTDstRegType(OperandVector &Operands) {
  StringRef Str = getLexer().getTok().getString();
  unsigned SIMTRegType = StringSwitch<unsigned>(Str.lower())
                             .Case(".d", LinxV5Op::SIMT_INT_DST_REG_TYPE_D)
                             .Case(".w", LinxV5Op::SIMT_INT_DST_REG_TYPE_W)
                             .Case(".h", LinxV5Op::SIMT_INT_DST_REG_TYPE_H)
                             .Case(".b", LinxV5Op::SIMT_INT_DST_REG_TYPE_B)
                             .Default(LinxV5Op::SIMT_INT_DST_REG_TYPE_NONE);
  if (SIMTRegType == LinxV5Op::SIMT_INT_DST_REG_TYPE_NONE)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createSIMTRegType(SIMTRegType, S, E));
  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseShamtImm(OperandVector &Operands) {
  if (getLexer().getKind() == AsmToken::LessLess &&
      getLexer().peekTok().getKind() == AsmToken::Integer) {
    getLexer().Lex();
    return parseImmediate(Operands);
  }

  // add default zero operand only for std compound srcR shamt.
  if (!static_cast<LinxV5Operand *>(Operands[Operands.size() - 1].get())
           ->isCompound())
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  const MCExpr *Res = MCConstantExpr::create(0, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseSIMTShamtImm(OperandVector &Operands) {
  if (getLexer().getKind() == AsmToken::LessLess &&
      getLexer().peekTok().getKind() == AsmToken::Integer) {
    getLexer().Lex();
    return parseImmediate(Operands);
  }

  auto prevOperand =
      static_cast<LinxV5Operand *>(Operands[Operands.size() - 1].get());
  if (!prevOperand->isSIMTSrcRegType() && prevOperand->Kind != LinxV5Operand::KindTy::SIMTRegOp)
    return MatchOperand_NoMatch;

  // add default zero operand only for simt compound srcR shamt.
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  const MCExpr *Res = MCConstantExpr::create(0, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  return MatchOperand_Success;
}

static unsigned calculateBAttr(StringRef Identifier) {

  unsigned AttrType = StringSwitch<unsigned>(Identifier.lower())
                          .Case("trap", AttrType::TRAP)
                          .Case("atomic", AttrType::ATOMIC)
                          .Case("aq", AttrType::AQ)
                          .Case("rl", AttrType::RL)
                          .Case("aqrl", AttrType::AQRL)
                          .Case("far", AttrType::FAR)
                          .Default(AttrType::NONEATTR);
  return AttrType;
}

OperandMatchResultTy LinxV5AsmParser::parseBAttrType(OperandVector &Operands) {
  unsigned CurAttrType = AttrType::NONEATTR;
  unsigned Result = AttrType::NONEATTR;
  unsigned Size = 0;
  bool ExpectAttr = true;
  bool StoppedByNonAttr = false;

  SMLoc S = getLoc();

  while (getLexer().isNot(AsmToken::EndOfStatement)) {
    switch (getLexer().getKind()) {
    default:
      // If we haven't parsed any attribute yet, this operand does not match
      // BAttrType at all.
      if (Result == AttrType::NONEATTR)
        return MatchOperand_NoMatch;
      return MatchOperand_ParseFail;

    case AsmToken::String:
    case AsmToken::Identifier: {
      StringRef Identifier = getLexer().getTok().getIdentifier();
      CurAttrType = calculateBAttr(Identifier);

      if (CurAttrType == AttrType::NONEATTR) {
        StoppedByNonAttr = true;
        goto Done;
      }

      Result |= CurAttrType;
      Size += Identifier.size();
      getParser().Lex();
      ExpectAttr = false;
      break;
    }

    case AsmToken::Comma: {
      if (ExpectAttr) {
        if (Result == AttrType::NONEATTR)
          return MatchOperand_NoMatch;
        return MatchOperand_ParseFail;
      }

      getParser().Lex();
      Size += 1;
      ExpectAttr = true;
      break;
    }
    }
  }

Done:
  // First token is not a BAttrType => let other operand parsers try.
  if (Result == AttrType::NONEATTR)
    return MatchOperand_NoMatch;

  // Incomplete attribute list like: "atomic,"
  if (ExpectAttr && !StoppedByNonAttr)
    return MatchOperand_ParseFail;

  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Size);
  Operands.push_back(LinxV5Operand::createBAttrType(Result, S, E));

  return MatchOperand_Success;
}

static unsigned calculateBArgFormat(StringRef Identifier) {

  unsigned Format = StringSwitch<unsigned>(Identifier.upper())
#define TRANS(NAME, CODE) .Case(#NAME, ArgFormat::NAME)
#include "MCTargetDesc/LinxV5TileTrans.def"
#undef TRANS
                        .Default(ArgFormat::RESERVE);
  return Format;
}

OperandMatchResultTy LinxV5AsmParser::parseBArgFormat(OperandVector &Operands) {
  unsigned Format = ArgFormat::RESERVE;
  StringRef Str = getLexer().getTok().getString();
  StringRef LexStr = Str;
  StringRef UnLexStr;
  size_t DotPosition = Str.find('.');
  if (DotPosition != StringRef::npos) {
    LexStr = Str.slice(0, DotPosition);
    UnLexStr = Str.substr(DotPosition, Str.size());
  }
  Format = calculateBArgFormat(LexStr);
  if (Format == ArgFormat::RESERVE)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + LexStr.size());
  const MCExpr *Res = MCConstantExpr::create(Format, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  getLexer().Lex(); // Eat identifier token.
  if (!UnLexStr.empty())
    getLexer().UnLex(AsmToken(AsmToken::Identifier, UnLexStr));
  return MatchOperand_Success;
}

static unsigned calculateRMode(StringRef Identifier) {
  unsigned Mode = StringSwitch<unsigned>(Identifier.upper())
#define RMODE(NAME, CODE) .Case(#NAME, RMode::NAME)
#include "MCTargetDesc/LinxV5TileRMode.def"
#undef RMODE
                      .Default(RMode::EMPTY_RMode);
  return Mode;
}

OperandMatchResultTy LinxV5AsmParser::parseRMode(OperandVector &Operands) {
  unsigned Mode = RMode::EMPTY_RMode;
  StringRef Str = getLexer().getTok().getString();

  Mode = calculateRMode(Str);
  if (Mode == RMode::EMPTY_RMode)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Str.size());
  const MCExpr *Res = MCConstantExpr::create(Mode, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));

  getLexer().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

static unsigned calculateCanon(StringRef Identifier) {

  unsigned Format = StringSwitch<unsigned>(Identifier.lower())
                        .Case(".normal", Canon::NORMAL_CANON)
                        .Case(".canon", Canon::CANON)
                        .Case("sat", 1) // Sat 复用 Canon 位
                        .Default(Canon::EMPTY_Canon);
  return Format;
}

OperandMatchResultTy LinxV5AsmParser::parseCanon(OperandVector &Operands) {
  unsigned ret = Canon::EMPTY_Canon;
  StringRef Str = getLexer().getTok().getString();
  ret = calculateCanon(Str);
  if (ret == Canon::EMPTY_Canon)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Str.size());
  Operands.push_back(LinxV5Operand::createCanonType(ret, S, E));
  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

static unsigned calculateSat(StringRef Identifier) {

  unsigned Format = StringSwitch<unsigned>(Identifier.lower())
                        .Case("sat", Sat::SAT)
                        .Case("nosat", Sat::NOSAT)
                        .Default(Sat::EMPTY_Sat);
  return Format;
}

OperandMatchResultTy LinxV5AsmParser::parseSat(OperandVector &Operands) {
  unsigned ret = Sat::EMPTY_Sat;
  StringRef Str = getLexer().getTok().getString();
  ret = calculateSat(Str);
  if (ret == Sat::EMPTY_Sat)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Str.size());
  Operands.push_back(LinxV5Operand::createSatType(ret, S, E));
  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

static unsigned calculateByteID(StringRef Identifier) {
  unsigned Format = StringSwitch<unsigned>(Identifier.lower())
                        .Case("byte0", LinxV5Op::ByteID::BYTE0)
                        .Case("byte1", LinxV5Op::ByteID::BYTE1)
                        .Case("byte2", LinxV5Op::ByteID::BYTE2)
                        .Case("byte3", LinxV5Op::ByteID::BYTE3)
                        .Default(LinxV5Op::ByteID::EMPTY_ByteID);
  return Format;
}

OperandMatchResultTy LinxV5AsmParser::parseByteID(OperandVector &Operands) {
  unsigned ret = LinxV5Op::ByteID::EMPTY_ByteID;
  StringRef Str = getLexer().getTok().getString();
  ret = calculateByteID(Str);
  if (ret == LinxV5Op::ByteID::EMPTY_ByteID)
    return MatchOperand_NoMatch;

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Str.size());
  Operands.push_back(LinxV5Operand::createByteIDType(ret, S, E));
  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

static int calculateBIO(StringRef Name) {
  int LeftBits = -1;
  unsigned RegNo = MatchLinxV5GlobalRegisteName(Name);
  if (RegNo == LinxV5::NoRegister)
    return -1;
  LeftBits = RegNo - LinxV5::R0;
  return LeftBits;
}

OperandMatchResultTy LinxV5AsmParser::parseGPRBitMap(OperandVector &Operands) {
  unsigned Result = 0;
  unsigned Size = 0;
  StringRef Identifier;
  bool isStop = false;
  do {
    switch (getLexer().getKind()) {
    default:
      return MatchOperand_ParseFail;

    case AsmToken::String:
    case AsmToken::Identifier: {
      Identifier = getLexer().getTok().getIdentifier();
      int LeftBits = calculateBIO(Identifier);
      if (LeftBits == -1)
        return MatchOperand_ParseFail;
      Result |= 1 << LeftBits;
      Size += Identifier.size();
      getParser().Lex();
      continue;
    }
    case AsmToken::Comma: {
      getLexer().Lex();
      Size += 1;
      continue;
    }
    case AsmToken::RBrac: {
      isStop = true;
      continue;
    }
    }
  } while (!isStop && getLexer().isNot(AsmToken::EndOfStatement));

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Size);
  const MCExpr *Res = MCConstantExpr::create(Result, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseFenceFlag(OperandVector &Operands) {
  unsigned Flag = 0;
  if (getLexer().getTok().is(AsmToken::EndOfStatement))
    return MatchOperand_ParseFail;
  StringRef Tok = getTok().getIdentifier();
  for (unsigned i = 0; i < Tok.size(); ++i) {
    if (Tok[i] == 'i')
      Flag |= LinxV5Op::FF_DEVI;
    else if (Tok[i] == 'o')
      Flag |= LinxV5Op::FF_DEVO;
    else if (Tok[i] == 'r')
      Flag |= LinxV5Op::FF_MEMR;
    else if (Tok[i] == 'w')
      Flag |= LinxV5Op::FF_MEMW;
    else
      return MatchOperand_ParseFail;
  }

  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer() + Tok.size());
  const MCExpr *Res = MCConstantExpr::create(Flag, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  Lex();

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseRegDepSrc(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());

  // To avoid affecting the logic of the pseudo-instruction parser, execute the
  // following logic only when there is no Src in b.iod.
  LinxV5Operand &mnemonic = static_cast<LinxV5Operand &>(*Operands[0]);
  assert(mnemonic.isToken() && "Leading operand should always be a mnemonic!");
  StringRef Tok = mnemonic.getToken();

  // If the current token is '->', i.e., B.IOD -> D, the default missing DepSrc.
  if (getLexer().is(AsmToken:: MinusGreater) && Tok == "b.iod") {
    Operands.push_back(LinxV5Operand::createReg(0, S, E));
    return MatchOperand_Success;
  }

  StringRef Str = getLexer().getTok().getString();
  MCRegister RegNo = MatchLinxV5DepRegisteName(Str);
  if (RegNo == LinxV5::NoRegister)
    return MatchOperand_NoMatch;

  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getLexer().Lex();  // consume '->'
  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseGPRSrc(OperandVector &Operands) {
  MCRegister RegNo;
  StringRef Str = getLexer().getTok().getString();
  size_t DotPosition = Str.find('.');
  if (DotPosition != StringRef::npos)
    return MatchOperand_NoMatch;
  matchRegisterNameHelper(RegNo, Str);
  if (RegNo == LinxV5::NoRegister)
    return MatchOperand_NoMatch;

  StringRef Type = Str.slice(DotPosition, Str.size());
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseDstRWithArrow(OperandVector &Operands) {
  StringRef Name = getLexer().getTok().getString();
  // v5: when AsmString has "->" as a literal before $DstTile, the "->"
  // token is consumed by the matcher, so we may see the register directly.
  if (!Name.str().compare("->")) {
    Name = getLexer().peekTok().getIdentifier();
  } else {
    // v5: no "->" prefix (it was a literal consumed by AsmString).
    // Try to match the current token as a register name directly.
    Name = getLexer().getTok().getIdentifier();
    if (Name.empty())
      return MatchOperand_NoMatch;
  }

  size_t DotPosition = Name.find('.');

  if (DotPosition != StringRef::npos)
    return MatchOperand_NoMatch;

  MCRegister RegNo;
  matchRegisterNameHelper(RegNo, Name);

  if (RegNo != LinxV5::NoRegister) {
    SMLoc S = getLoc();
    SMLoc E = SMLoc::getFromPointer(S.getPointer());
    Operands.push_back(LinxV5Operand::createReg(RegNo, S, E));
    // Only consume '->' if we saw it.
    if (!getLexer().getTok().getString().compare("->"))
      getLexer().Lex();  // consume '->'
    getParser().Lex(); // Eat identifier token.
    return MatchOperand_Success;
  } else {
    return MatchOperand_ParseFail;
  }
}

OperandMatchResultTy
LinxV5AsmParser::parseLoopBDstRWithArrow(OperandVector &Operands) {
  StringRef Name = getLexer().getTok().getString();
  if (!Name.str().compare("->")) {
    Name = getLexer().peekTok().getIdentifier();
  } else {
    return MatchOperand_NoMatch;
  }
  getLexer().Lex(); // consume '->'
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  if (!Name.str().compare("lb0") && !Name.str().compare("lb1") &&
      !Name.str().compare("lb2"))
    return MatchOperand_ParseFail;
  int imm = Name[2] - '0';
  const MCExpr *Res = MCConstantExpr::create(imm, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));
  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

OperandMatchResultTy
LinxV5AsmParser::parseBStartWithoutTargetBrType(OperandVector &Operands) {
  unsigned BrType = BranchType::FALL;
  SMLoc S = getLoc();
  SMLoc E = SMLoc::getFromPointer(S.getPointer());
  if (getLexer().getTok().is(AsmToken::EndOfStatement)) {
    const MCExpr *Res = MCConstantExpr::create(BrType, getContext());
    Operands.push_back(LinxV5Operand::createImm(Res, S, E));
    return MatchOperand_Success;
  }

  if (getLexer().getKind() != AsmToken::String &&
      getLexer().getKind() != AsmToken::Identifier)
    return MatchOperand_NoMatch;

  StringRef Identifier = getLexer().getTok().getIdentifier();
  BrType = StringSwitch<unsigned>(Identifier.lower())
               .Case("fall", BranchType::FALL)
               .Case("ind", BranchType::IND)
               .Case("icall", BranchType::ICALL)
               .Case("ret", BranchType::RET)
               .Default(BranchType::EMPTY);

  if (BrType == BranchType::EMPTY)
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  const MCExpr *Res = MCConstantExpr::create(BrType, getContext());
  Operands.push_back(LinxV5Operand::createImm(Res, S, E));

  return MatchOperand_Success;
}

OperandMatchResultTy LinxV5AsmParser::parseJALOffset(OperandVector &Operands) {
  assert(0 && "Delete me! This is the LinxV3 define!");
  return MatchOperand_NoMatch;
}

OperandMatchResultTy LinxV5AsmParser::parseBareSymbol(OperandVector &Operands) {
  assert(0 && "Delete me! This is the LinxV5V3 define!");
  return MatchOperand_NoMatch;
}

OperandMatchResultTy LinxV5AsmParser::parseCallSymbol(OperandVector &Operands) {
  assert(0 && "Delete me! This is the LinxV5V3 define!");
  return MatchOperand_NoMatch;
}

OperandMatchResultTy
LinxV5AsmParser::parsePseudoJumpSymbol(OperandVector &Operands) {
  assert(0 && "Delete me! This is the LinxV5V3 define!");
  return MatchOperand_NoMatch;
}

OperandMatchResultTy
LinxV5AsmParser::parseOperandWithModifier(OperandVector &Operands) {
  SMLoc S = getLoc();
  SMLoc E;

  if (getLexer().getKind() != AsmToken::Percent) {
    Error(getLoc(), "expected '%' for operand modifier");
    return MatchOperand_ParseFail;
  }

  getParser().Lex(); // Eat '%'

  if (getLexer().getKind() != AsmToken::Identifier) {
    Error(getLoc(), "expected valid identifier for operand modifier");
    return MatchOperand_ParseFail;
  }
  StringRef Identifier = getParser().getTok().getIdentifier();
  LinxV5MCExpr::VariantKind VK =
      LinxV5MCExpr::getVariantKindForName(Identifier);
  if (VK == LinxV5MCExpr::VK_LinxV5_Invalid) {
    Error(getLoc(), "unrecognized operand modifier");
    return MatchOperand_ParseFail;
  }

  getParser().Lex(); // Eat the identifier
  if (getLexer().getKind() != AsmToken::LParen) {
    Error(getLoc(), "expected '('");
    return MatchOperand_ParseFail;
  }
  getParser().Lex(); // Eat '('

  const MCExpr *SubExpr;
  if (getParser().parseParenExpression(SubExpr, E)) {
    return MatchOperand_ParseFail;
  }

  const MCExpr *ModExpr = LinxV5MCExpr::create(SubExpr, VK, getContext());
  Operands.push_back(LinxV5Operand::createImm(ModExpr, S, E));
  return MatchOperand_Success;
}

bool LinxV5AsmParser::classifySymbolRef(const MCExpr *Expr,
                                        LinxV5MCExpr::VariantKind &Kind) {
  Kind = LinxV5MCExpr::VK_LinxV5_None;

  if (const LinxV5MCExpr *RE = dyn_cast<LinxV5MCExpr>(Expr)) {
    Kind = RE->getKind();
    Expr = RE->getSubExpr();
  }

  MCValue Res;
  MCFixup Fixup;
  if (Expr->evaluateAsRelocatable(Res, nullptr, &Fixup))
    return Res.getRefKind() == LinxV5MCExpr::VK_LinxV5_None;
  return false;
}

bool LinxV5AsmParser::ParseDirective(AsmToken DirectiveID) {
  // This returns false if this function recognizes the directive
  // regardless of whether it is successfully handles or reports an
  // error. Otherwise it returns true to give the generic parser a
  // chance at recognizing it.
  StringRef IDVal = DirectiveID.getString();

  if (IDVal == ".option")
    return parseDirectiveOption();
  else if (IDVal == ".unsafeasm") {
    IAVS = IA_UNSAFE;
    return false;
  }

  return true;
}

bool LinxV5AsmParser::parseDirectiveOption() {
  MCAsmParser &Parser = getParser();
  // Get the option token.
  AsmToken Tok = Parser.getTok();
  // At the moment only identifiers are supported.
  if (Tok.isNot(AsmToken::Identifier))
    return Error(Parser.getTok().getLoc(),
                 "unexpected token, expected identifier");

  StringRef Option = Tok.getIdentifier();

  if (Option == "relax") {
    getTargetStreamer().emitDirectiveOptionRelax();

    Parser.Lex();
    if (Parser.getTok().isNot(AsmToken::EndOfStatement))
      return Error(Parser.getTok().getLoc(),
                   "unexpected token, expected end of statement");

    setFeatureBits(LinxV5::FeatureRelax, "relax");
    return false;
  }

  if (Option == "norelax") {
    getTargetStreamer().emitDirectiveOptionNoRelax();

    Parser.Lex();
    if (Parser.getTok().isNot(AsmToken::EndOfStatement))
      return Error(Parser.getTok().getLoc(),
                   "unexpected token, expected end of statement");

    clearFeatureBits(LinxV5::FeatureRelax, "relax");
    return false;
  }

  // Unknown option.
  Warning(Parser.getTok().getLoc(),
          "unknown option, expected 'push', 'pop', 'rvc', 'norvc', 'relax' or "
          "'norelax'");
  Parser.eatToEndOfStatement();
  return false;
}

bool LinxV5AsmParser::ParseInstruction(ParseInstructionInfo &Info,
                                       StringRef Name, SMLoc NameLoc,
                                       OperandVector &Operands) {
  if (Name.startswith_insensitive("TLOAD.")) {
    getLexer().UnLex(llvm::AsmToken(AsmToken::Identifier, Name.substr(6)));
    Name = "tload.";
  }
  if (Name.startswith_insensitive("TSTORE.")) {
    getLexer().UnLex(llvm::AsmToken(AsmToken::Identifier, Name.substr(7)));
    Name = "tstore.";
  }

  // First operand is token for instruction
  Operands.push_back(LinxV5Operand::createToken(Name, NameLoc));

  do {
    switch (getLexer().getKind()) {
    default: {
      // v5: intercept "mask=" and "TSize=" before generic operand parse.
      // These are multi-token sequences (identifier + '=' + integer) that
      // the generic parser can't handle as a single operand.
      if (getLexer().getTok().is(AsmToken::Identifier)) {
        StringRef Id = getLexer().getTok().getIdentifier();
        if (Id.startswith_insensitive("mask")) {
          if (parsePE_MASK(Operands) == MatchOperand_Success)
            continue;
        }
        if (Id.startswith_insensitive("tsize")) {
          if (parseTSize(Operands) == MatchOperand_Success)
            continue;
        }
      }
      if (parseOperand(Operands, Name))
        return false;
      else
        continue;
    }
    case AsmToken::Comma: {
      getLexer().Lex();
      continue;
    }
    }
  } while (getLexer().isNot(AsmToken::EndOfStatement));

  getParser().Lex(); // Consume the EndOfStatement.
  return false;
}

static bool isTileBSTART(MCInst &Inst) {
  return Inst.getOpcode() == LinxV5::BSTART_VPAR ||
         Inst.getOpcode() == LinxV5::BSTART_MPAR ||
         Inst.getOpcode() == LinxV5::BSTART_MSEQ ||
         Inst.getOpcode() == LinxV5::BSTART_VSEQ ||
         Inst.getOpcode() == LinxV5::BSTART_TMA ||
         Inst.getOpcode() == LinxV5::BSTART_CUBE;
}

static bool isBSTOP(MCInst &Inst) {
  return Inst.getOpcode() == LinxV5::BSTOP ||
         Inst.getOpcode() == LinxV5::BSTOP_C ||
         Inst.getOpcode() == LinxV5::SIMT_BSTOP;
}

bool LinxV5AsmParser::maybeValidateInlineAsm(MCInst &Inst, SMLoc IDLoc) {
  if (IAVS == IA_SHUTDOWN || IAVS == IA_UNSAFE)
    return false;

  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  if (IAVS == IA_START) {
    if (LinxV5II::isTileOp(TSFlags) || isTileBSTART(Inst))
      IAVS = IA_TILE;
    else if (LinxV5II::isBSTART(TSFlags) || LinxV5II::isHeaderOnly(TSFlags))
      IAVS = IA_BLOCK;
    else
      return Error(IDLoc, "inline-asm should start from BSTART or Tile Call.");
  } else if (IAVS == IA_TILE) {
    if (!LinxV5II::isBlockModifier(TSFlags))
      return Error(IDLoc, "tile inline-asm only accept block-modifiers.");
  } else if (IAVS == IA_BLOCK) {
    // do nothing.
  } else if (IAVS == IA_SIMT_START) {
    if (!LinxV5II::isMicroInstr(TSFlags)) {
      return Error(IDLoc, "simt inline-asm only accept micro instruction.");
    }
    IAVS = IA_SIMT_SINGLE;
  } else if (IAVS == IA_SIMT_SINGLE) {
    if (!LinxV5II::isMicroInstr(TSFlags)) {
      return Error(IDLoc, "simt inline-asm only accept micro instruction.");
    }
    IAVS = IA_SIMT_MULTI;
  } else if (IAVS == IA_SIMT_MULTI) {
    if (!LinxV5II::isMicroInstr(TSFlags)) {
      return Error(IDLoc, "simt inline-asm only accept micro instruction.");
    }
    if (isBSTOP(Inst))
      IAVS = IA_SIMT_BSTOP;
  } else if (IAVS == IA_SIMT_BSTOP) {
    return Error(IDLoc, "simt inline-asm already ends at bstop.");
  }
  return false;
}

// TODO: Add legality check for instruction.
bool LinxV5AsmParser::validateInstruction(MCInst &Inst, OperandVector &Operands,
                                          SMLoc IDLoc) {
  if (MII.get(Inst.getOpcode()).TSFlags &
      llvm::LinxV5II::IsDisassembleOnlyMask) {
    return true;
  }
  // P0-2: B.FPATR field/combo legality per PTO 0.58.1 B.FPATR.asl.
  if (Inst.getOpcode() == LinxV5::B_FPATR) {
    auto GetImm = [&](unsigned OpNo) -> int64_t {
      return Inst.getOperand(OpNo).getImm();
    };
    int64_t PreQuant = GetImm(0), Relu = GetImm(1), GroupN = GetImm(2);
    int64_t RowMaxEn = GetImm(3), GroupMaxEn = GetImm(4);
    int64_t RowMaxInit = GetImm(5), MaxAbs = GetImm(6);
    const int64_t LegalPreQuant[] = {
        0, 1, 2, 3, 4, 5, 12, 13, 16, 17, 18, 19, 20,
        23, 24, 25, 26, 27, 28, 32, 33, 34, 35, 36, 37, 38, 39};
    if (llvm::is_contained(LegalPreQuant, PreQuant) == false)
      return true; // invalid PreQuantMode
    if (Relu > 3 || Relu < 0)
      return true;
    if (GroupN > 9 || GroupN < 0)
      return true;
    if (RowMaxEn == 0 && RowMaxInit != 0)
      return true;
    if (GroupMaxEn == 0 && GroupN != 0)
      return true;
    if (GroupMaxEn == 1 && GroupN == 0)
      return true;
    if (RowMaxEn == 0 && GroupMaxEn == 0 && MaxAbs != 0)
      return true;
  }
  return false;
}

void LinxV5AsmParser::emitToStreamer(MCStreamer &S, const MCInst &Inst) {
  MCInst CInst;
  bool Res =
      llvm::LinxV5::tryCompressInst(CInst, Inst, getSTI(), S.getContext());
  S.emitInstruction((Res ? CInst : Inst), getSTI());
}

void LinxV5AsmParser::emitBStartWithTarget(MCInst &Inst, SMLoc IDLoc,
                                           MCStreamer &Out) {
  unsigned Opcode = Inst.getOpcode();

  switch (Opcode) {
  case LinxV5::BSTART_STD_WITH_TARGET_25_DIRECT:
  case LinxV5::BSTART_STD_WITH_TARGET_17_DIRECT:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_STD_WITH_TARGET_64_DIRECT)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_STD_WITH_TARGET_25_COND:
  case LinxV5::BSTART_STD_WITH_TARGET_17_COND:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_STD_WITH_TARGET_64_COND)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_STD_WITH_TARGET_17_CALL:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_STD_WITH_TARGET_64_CALL)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_STD_WITH_FIXUP_17_FALL:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_STD_WITH_FIXUP_64_FALL)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_AUX_WITH_TARGET_17_DIRECT:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_AUX_WITH_TARGET_64_DIRECT)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_AUX_WITH_TARGET_17_COND:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_AUX_WITH_TARGET_64_COND)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_AUX_WITH_TARGET_17_CALL:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_AUX_WITH_TARGET_64_CALL)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_AUX_WITH_FIXUP_17_FALL:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_AUX_WITH_FIXUP_64_FALL)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_FP_WITH_TARGET_17_DIRECT:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_FP_WITH_TARGET_64_DIRECT)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_FP_WITH_TARGET_17_COND:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_FP_WITH_TARGET_64_COND)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_FP_WITH_TARGET_17_CALL:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_FP_WITH_TARGET_64_CALL)
                            .addOperand(Inst.getOperand(0)));
    break;
  case LinxV5::BSTART_FP_WITH_FIXUP_17_FALL:
    emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_FP_WITH_FIXUP_64_FALL)
                            .addOperand(Inst.getOperand(0)));
    break;
  }
}

void LinxV5AsmParser::emitMcInstVecToStreamer(llvm::SmallVector<MCInst> McVec,
                                              MCStreamer &Out) {
  for (MCInst &inst : McVec) {
    emitToStreamer(Out, inst);
  }
}

void LinxV5AsmParser::emitVCall(MCInst &Inst, MCStreamer &Out) {
  emitToStreamer(
      Out, MCInstBuilder(LinxV5::BSTART_VPAR)
               .addOperand(MCOperand::createImm(LinxV5Op::TileOPMode::VS16)));
  // b.catr DR
  emitMcInstVecToStreamer(getBATTRFromInst(Inst, MII), Out);
  // emit b.iot
  emitMcInstVecToStreamer(getBIOTFromInst(Inst, MII), Out);
  // emit b.ior
  emitMcInstVecToStreamer(getBIORFromInst(Inst, MII), Out);
  // emit b.dim ->lb0
  emitMcInstVecToStreamer(getBDIMFromInst(Inst, MII), Out);
  // emit b.iod
  emitMcInstVecToStreamer(getBIODFromInst(Inst, MII), Out);
  // emit b.text
  emitMcInstVecToStreamer(getBTEXTTFromInst(Inst, MII), Out);
}

void LinxV5AsmParser::emitMCall(MCInst &Inst, MCStreamer &Out) {
  emitToStreamer(
      Out, MCInstBuilder(LinxV5::BSTART_MPAR)
               .addOperand(MCOperand::createImm(LinxV5Op::TileOPMode::VS16)));
  // b.catr DR
  emitMcInstVecToStreamer(getBATTRFromInst(Inst, MII), Out);
  // emit b.iot
  emitMcInstVecToStreamer(getBIOTFromInst(Inst, MII), Out);
  // emit b.ior
  emitMcInstVecToStreamer(getBIORFromInst(Inst, MII), Out);
  // emit b.dim ->lb0
  emitMcInstVecToStreamer(getBDIMFromInst(Inst, MII), Out);
  // emit b.iod
  emitMcInstVecToStreamer(getBIODFromInst(Inst, MII), Out);
  // emit b.text
  emitMcInstVecToStreamer(getBTEXTTFromInst(Inst, MII), Out);
}

void LinxV5AsmParser::emitTCOPY(MCInst &Inst, MCStreamer &Out) {
  emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_TMA)
                          .addOperand(MCOperand::createImm(
                              llvm::LinxV5Op::DataType::EMPTY_DataType))
                          .addOperand(MCOperand::createImm(TileOPTMA::TMOV)));

  // emit b.iot
  emitMcInstVecToStreamer(llvm::getBIOTFromInst(Inst, MII), Out);
}

// void LinxV5AsmParser::emitTEPL(MCInst &Inst, MCStreamer &Out,
//                                unsigned TEPLOpc) {
//   unsigned TEPLModeID = 0;
//   if (TEPLOpc == LinxV5Op::TileOPTEPL::ESAVE)
//     TEPLModeID = 2;
//   else if (TEPLOpc == LinxV5Op::TileOPTEPL::ERCOV)
//     TEPLModeID = 1;
//   emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_TEPL)
//                           .addOperand(MCOperand::createImm(
//                               llvm::LinxV5Op::DataType::EMPTY_DataType))
//                           .addOperand(MCOperand::createImm(TEPLOpc))
//                           .addOperand(Inst.getOperand(TEPLModeID)));

//   // emit b.iot
//   emitMcInstVecToStreamer(llvm::getBIOTFromInst(Inst, MII), Out);
// }

void LinxV5AsmParser::emitEmptyTile(MCInst &Inst, MCStreamer &Out) {
  emitToStreamer(
      Out, MCInstBuilder(LinxV5::BSTART_VPAR)
               .addOperand(MCOperand::createImm(LinxV5Op::TileOPMode::VS16)));

  // emit b.iot
  emitMcInstVecToStreamer(getBIOTFromInst(Inst, MII), Out);
}

void LinxV5AsmParser::emitMAMULBAC(MCInst &Inst, MCStreamer &Out) {
  if (Inst.getOperand(12).getReg() == LinxV5::Tile_ACCOS1)
    Inst.setOpcode(LinxV5::PseudoMAMULBACC_SizeI);
  else
    Inst.setOpcode(LinxV5::PseudoMAMULBAC_SizeI);
  emitToStreamer(Out, Inst);
}

void LinxV5AsmParser::emitMAMULBMXAC(MCInst &Inst, MCStreamer &Out) {
  if (Inst.getOperand(14).getReg() == LinxV5::Tile_ACCOS1)
    Inst.setOpcode(LinxV5::PseudoMAMULBMXACC_SizeI);
  else
    Inst.setOpcode(LinxV5::PseudoMAMULBMXAC_SizeI);
  emitToStreamer(Out, Inst);
}

void LinxV5AsmParser::emitMAMULBMXBAC(MCInst &Inst, MCStreamer &Out) {
  if (Inst.getOperand(13).getReg() == LinxV5::Tile_ACCOS1)
    Inst.setOpcode(LinxV5::PseudoMAMULBMXBACC_SizeI);
  else
    Inst.setOpcode(LinxV5::PseudoMAMULBMXBAC_SizeI);
  emitToStreamer(Out, Inst);
}

void LinxV5AsmParser::emitTCOPYIO(MCInst &Inst, MCStreamer &Out,  unsigned MemOpc) {
  emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_TMA)
                          .addOperand(Inst.getOperand(7))
                          .addOperand(MCOperand::createImm(MemOpc)));
  // emit b.arg
  emitMcInstVecToStreamer(getBARGFromInst(Inst, MII), Out);
  // emit b.iot
  emitMcInstVecToStreamer(getBIOTFromInst(Inst, MII), Out);
  // emit b.ior
  emitMcInstVecToStreamer(getBIORFromInst(Inst, MII), Out);
  // emit b.dim ->lb0
  emitMcInstVecToStreamer(getBDIMFromInst(Inst, MII), Out);
  // emit b.iod
  emitMcInstVecToStreamer(getBIODFromInst(Inst, MII), Out);
}

void LinxV5AsmParser::emitCCall(MCInst &Inst, MCStreamer &Out,
                                unsigned CUBEOpc) {
  emitToStreamer(Out, MCInstBuilder(LinxV5::BSTART_CUBE)
                          .addOperand(Inst.getOperand(7))
                          .addOperand(MCOperand::createImm(CUBEOpc)));
  if (isActiveMatrixPseudo(Inst.getOpcode())) {
    // b.catr DR + b.datr datatype
    emitMcInstVecToStreamer(getBATTRFromInst(Inst, MII), Out);
  }
  // emit b.dim ->lb0
  emitMcInstVecToStreamer(getBDIMFromInst(Inst, MII), Out);
  // emit b.iot
  emitMcInstVecToStreamer(getBIOTFromInst(Inst, MII), Out);
}

bool LinxV5AsmParser::processInstruction(MCInst &Inst, SMLoc IDLoc,
                                         OperandVector &Operands,
                                         MCStreamer &Out) {
  Inst.setLoc(IDLoc);
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  if (LinxV5II::isTileOp(TSFlags)) {
    if (LinxV5II::isTileOpAtVEC(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
      emitVCall(Inst, Out);
      return false;
    }

    if (LinxV5II::isTileOpAtMTC(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
      emitMCall(Inst, Out);
      return false;
    }

    if (Inst.getOpcode() == LinxV5::PseudoTCOPY) {
      emitTCOPY(Inst, Out);
      return false;
    }

    if (Inst.getOpcode() == LinxV5::PseudoMAMULBAC_Higher_SizeI) {
      emitMAMULBAC(Inst, Out);
      return false;
    }

    if (Inst.getOpcode() == LinxV5::PseudoMAMULBMXAC_Higher_SizeI) {
      emitMAMULBMXAC(Inst, Out);
      return false;
    }

    if (Inst.getOpcode() == LinxV5::PseudoMAMULBMXBAC_Higher_SizeI) {
      emitMAMULBMXBAC(Inst, Out);
      return false;
    }

    if (LinxV5II::isTileOpAtCUBE(TSFlags) && LinxV5II::isHeaderOnly(TSFlags)) {
      unsigned Opc = getPseudoTILEOpcode(Inst.getOpcode());
      emitCCall(Inst, Out, Opc);
      return false;
    }
    // TCOPY instruction is not processed here; it is separately excluded in the preceding PseudoTCOPY conditions.
    if (LinxV5II::isTileOpAtMTC(TSFlags) && LinxV5II::isHeaderOnly(TSFlags)) {
      unsigned Opc = getPseudoTILEOpcode(Inst.getOpcode());
      emitTCOPYIO(Inst, Out, Opc);
      return false;
    }

    if (Inst.getOpcode() == LinxV5::PseudoEmptyTile) {
      emitEmptyTile(Inst, Out);
      return false;
    }

    // if (LinxV5II::isTileOpAtTEPL(TSFlags) && LinxV5II::isHeaderOnly(TSFlags))
    // {
    //   unsigned Opc = getPseudoTILEOpcode(Inst.getOpcode());
    //   emitTEPL(Inst, Out, Opc);
    //   return false;
    // }

    assert(0 && "All TileOp need Expand!");
  }

  unsigned MIFrm = LinxV5II::getFormat(Desc.TSFlags);
  if (MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_25 ||
      MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_17) {
    emitBStartWithTarget(Inst, IDLoc, Out);
    return false;
  }

  emitToStreamer(Out, Inst);
  return false;
}

/// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLinxV5AsmParser() {
  RegisterMCAsmParser<LinxV5AsmParser> V4(getTheLinx64V5Target());
  RegisterMCAsmParser<LinxV5AsmParser> BE(getTheLinx64V5beTarget());
}
