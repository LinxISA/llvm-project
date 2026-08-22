//===-- LinxISATileSSABalance.cpp - Tile SSA edge balancing --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "LinxISAInstrInfo.h"
#include "LinxISARegisterInfo.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <optional>
#include <string>

using namespace llvm;

#define DEBUG_TYPE "linx-tile-ssa-balance"

namespace {

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
  uint8_t TSize = 0;
  uint8_t DataType = 17;
  int64_t Layout = 0;
  bool HasSize = false;
  bool HasDataType = false;
  bool HasLayout = false;
};

struct TileCopyOp {
  MachineInstr *MI = nullptr;
  Register Dst;
  Register Src;
  TileMeta Meta;
  bool PreserveIdentity = false;
  bool DstDead = false;
  bool SrcUndef = false;
  bool SrcKillHint = false;
  bool SrcFromSpill = false;
  int SrcSpillFI = -1;
};

static bool isTilePhysReg(Register Reg) {
  return Reg && Reg.isPhysical() && LinxISA::TILERegClass.contains(Reg);
}

static Register reservedPhiCycleTempTile() { return LinxISA::TILE31; }

static std::optional<uint64_t> tSizeToBytes(unsigned TSize) {
  if (TSize < 1u || TSize > 10u)
    return std::nullopt;
  return 1ull << (TSize + 6u);
}

static bool isStrictTileTSizeCode(unsigned TSize) {
  std::optional<uint64_t> Bytes = tSizeToBytes(TSize);
  return Bytes && *Bytes >= 128u && *Bytes <= 65536u;
}

static unsigned getTileRegId(const TargetRegisterInfo &TRI, Register Reg) {
  if (!isTilePhysReg(Reg))
    report_fatal_error("Linx: expected physical tile register");
  return TRI.getEncodingValue(Reg) & 0x1fu;
}

static TileHand handFromTileId(unsigned TileId) {
  if (TileId < 8)
    return TileHand::T;
  if (TileId < 16)
    return TileHand::U;
  if (TileId < 24)
    return TileHand::M;
  return TileHand::N;
}

static unsigned handBase(TileHand Hand) {
  switch (Hand) {
  case TileHand::T:
    return 0;
  case TileHand::U:
    return 8;
  case TileHand::M:
    return 16;
  case TileHand::N:
    return 24;
  case TileHand::ACC:
    return 32;
  }
  llvm_unreachable("invalid tile hand");
}

static TileRelRef decodeTileRelRef(unsigned TileId, bool Reuse = false) {
  TileRelRef Ref;
  Ref.Hand = handFromTileId(TileId & 0x1fu);
  Ref.Depth = static_cast<uint8_t>((TileId & 0x7u) + 1u);
  Ref.Reuse = Reuse;
  return Ref;
}

static unsigned encodeTileRelRef(const TileRelRef &Ref) {
  if (Ref.Depth < 1 || Ref.Depth > 8)
    report_fatal_error("Linx: invalid tile relref depth (expected 1..8)");
  if (Ref.Hand == TileHand::ACC)
    report_fatal_error("Linx: ACC is not encodable as a source tile relref");
  return handBase(Ref.Hand) + static_cast<unsigned>(Ref.Depth - 1u);
}

static bool metadataCompatible(const TileMeta &A, const TileMeta &B,
                               std::string &Reason) {
  if (!A.HasSize || !B.HasSize) {
    Reason = "missing required size metadata";
    return false;
  }
  if (!isStrictTileTSizeCode(A.TSize) || !isStrictTileTSizeCode(B.TSize)) {
    Reason = "SizeCode outside 128B..64KB policy";
    return false;
  }
  if (A.TSize != B.TSize) {
    Reason = "TSize mismatch";
    return false;
  }
  if (A.HasDataType && B.HasDataType && A.DataType != B.DataType) {
    Reason = "DataType mismatch";
    return false;
  }
  if (A.HasLayout && B.HasLayout && A.Layout != B.Layout) {
    Reason = "layout mismatch";
    return false;
  }
  return true;
}

static TileMeta mergeMetadata(const TileMeta &Primary,
                              const TileMeta &Secondary) {
  TileMeta Out = Primary;
  if (!Out.HasDataType && Secondary.HasDataType) {
    Out.HasDataType = true;
    Out.DataType = Secondary.DataType;
  }
  if (!Out.HasLayout && Secondary.HasLayout) {
    Out.HasLayout = true;
    Out.Layout = Secondary.Layout;
  }
  if (!Out.HasSize && Secondary.HasSize) {
    Out.HasSize = true;
    Out.TSize = Secondary.TSize;
  }
  return Out;
}

[[noreturn]] static void reportTileBalanceError(const MachineFunction &MF,
                                                const MachineInstr &MI,
                                                const Twine &Msg) {
  std::string S;
  raw_string_ostream OS(S);
  OS << "LinxISATileSSABalance: " << Msg << " in function '" << MF.getName()
     << "' (bb." << MI.getParent()->getNumber() << ")\n";
  OS << "  instr: ";
  MI.print(OS);
  report_fatal_error(Twine(OS.str()));
}

static bool extractDefMetadata(const MachineInstr &MI, TileMeta &Meta) {
  switch (MI.getOpcode()) {
  case LinxISA::PSEUDO_TLSU_TLOAD:
  case LinxISA::PSEUDO_TLSU_TLOAD_ANY:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(2).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = 17; // canonical LinxISA 0.58 default INT32
    return true;

  case LinxISA::PSEUDO_TLSU_TLOAD_DESC:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(6).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = static_cast<uint8_t>(MI.getOperand(2).getImm() & 0x1f);
    Meta.HasLayout = true;
    Meta.Layout = MI.getOperand(3).getImm();
    return true;

  case LinxISA::PSEUDO_TLSU_TLOAD_SHAPE:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(7).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = static_cast<uint8_t>(MI.getOperand(2).getImm() & 0x1f);
    Meta.HasLayout = true;
    Meta.Layout = MI.getOperand(3).getImm();
    return true;

  case LinxISA::PSEUDO_CUBE_TMATMUL:
  case LinxISA::PSEUDO_CUBE_TMATMUL_ACC:
    Meta.HasSize = true;
    Meta.TSize = 6; // PTO 0.58: TSize 6 denotes a 4KB tile value
    Meta.HasDataType = true;
    Meta.DataType = 17;
    return true;

  case LinxISA::PSEUDO_VPAR_TADD:
  case LinxISA::PSEUDO_VPAR_TSUB:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(3).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = 17;
    return true;

  case LinxISA::PSEUDO_VTILE_ADD:
  case LinxISA::PSEUDO_VTILE_SUB:
    Meta.HasSize = true;
    Meta.TSize = 6; // PTO 0.58 blockify default: 4KB per PE
    Meta.HasDataType = true;
    Meta.DataType = 17;
    return true;

  case LinxISA::PSEUDO_TILEOP_UNARY:
  case LinxISA::PSEUDO_TILEOP_UNARY_SHAPE:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(3).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = static_cast<uint8_t>(MI.getOperand(4).getImm() & 0x1f);
    return true;

  case LinxISA::PSEUDO_TILEOP_BINARY:
  case LinxISA::PSEUDO_TILEOP_BINARY_SHAPE:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(4).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = static_cast<uint8_t>(MI.getOperand(5).getImm() & 0x1f);
    return true;

  case LinxISA::PSEUDO_TILEOP_BINARY_SCALAR:
  case LinxISA::PSEUDO_TILEOP_BINARY_SCALAR_SHAPE:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(4).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = static_cast<uint8_t>(MI.getOperand(5).getImm() & 0x1f);
    return true;

  case LinxISA::PSEUDO_TILEOP_SPLAT:
  case LinxISA::PSEUDO_TILEOP_SPLAT_SHAPE:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(3).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = static_cast<uint8_t>(MI.getOperand(4).getImm() & 0x1f);
    return true;

  case LinxISA::PSEUDO_TLSU_TMOV:
    Meta.HasSize = true;
    Meta.TSize = static_cast<uint8_t>(MI.getOperand(2).getImm() & 0x1f);
    Meta.HasDataType = true;
    Meta.DataType = static_cast<uint8_t>(MI.getOperand(3).getImm() & 0x1f);
    Meta.HasLayout = (MI.getOperand(5).getImm() & 1) != 0;
    if (Meta.HasLayout)
      Meta.Layout = MI.getOperand(4).getImm();
    return true;

  default:
    return false;
  }
}

static bool isTileTMOVPseudo(const MachineInstr &MI) {
  return MI.getOpcode() == LinxISA::PSEUDO_TLSU_TMOV;
}

static bool isTileCopyInstr(const MachineInstr &MI,
                            const TargetRegisterInfo &TRI, Register *DstOut,
                            Register *SrcOut) {
  if ((MI.getOpcode() != TargetOpcode::COPY &&
       MI.getOpcode() != LinxISA::PSEUDO_TILE_EDGE_COPY) ||
      MI.getNumOperands() < 2)
    return false;
  const MachineOperand &DstMO = MI.getOperand(0);
  const MachineOperand &SrcMO = MI.getOperand(1);
  if (!DstMO.isReg() || !SrcMO.isReg())
    return false;
  Register Dst = DstMO.getReg();
  Register Src = SrcMO.getReg();
  if (!isTilePhysReg(Dst) || !isTilePhysReg(Src))
    return false;

  // Enforce this helper only for tile physical registers.
  (void)getTileRegId(TRI, Dst);
  (void)getTileRegId(TRI, Src);
  if (DstOut)
    *DstOut = Dst;
  if (SrcOut)
    *SrcOut = Src;
  return true;
}

static bool canRelayFoldTMOV(const MachineInstr &PrevTMOV,
                             const MachineInstr &NextTMOV) {
  if (!isTileTMOVPseudo(PrevTMOV) || !isTileTMOVPseudo(NextTMOV))
    return false;
  if (PrevTMOV.getFlag(MachineInstr::NoMerge) ||
      NextTMOV.getFlag(MachineInstr::NoMerge))
    return false;
  if (PrevTMOV.getNumOperands() < 7 || NextTMOV.getNumOperands() < 7)
    return false;

  // Fold only if descriptor-affecting metadata is identical.
  // Operands: [2]=size [3]=dtype [4]=layout [5]=has_layout [6]=reuse
  return PrevTMOV.getOperand(2).getImm() == NextTMOV.getOperand(2).getImm() &&
         PrevTMOV.getOperand(3).getImm() == NextTMOV.getOperand(3).getImm() &&
         PrevTMOV.getOperand(4).getImm() == NextTMOV.getOperand(4).getImm() &&
         PrevTMOV.getOperand(5).getImm() == NextTMOV.getOperand(5).getImm();
}

class LinxISATileSSABalance : public MachineFunctionPass {
public:
  static char ID;

  LinxISATileSSABalance() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Linx Tile SSA Edge Balance";
  }

  bool prepareTilePHIEdges(MachineFunction &MF) {
    const auto &TRI = *MF.getSubtarget().getRegisterInfo();
    const auto &TII = *MF.getSubtarget().getInstrInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    SmallVector<MachineBasicBlock *, 8> TilePHIBlocks;

    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB.phis()) {
        Register Dst = MI.getOperand(0).getReg();
        if (!Dst.isVirtual())
          continue;
        if (!TRI.getCommonSubClass(MRI.getRegClass(Dst),
                                   &LinxISA::TILERegClass))
          continue;
        TilePHIBlocks.push_back(&MBB);
        break;
      }
    }

    bool Changed = false;
    for (MachineBasicBlock *MBB : TilePHIBlocks) {
      // The marker must execute on exactly one PHI edge. Split critical edges
      // before inserting it so sibling control-flow paths are unaffected.
      bool SplitOne = false;
      do {
        SplitOne = false;
        for (MachineBasicBlock *Pred : MBB->predecessors()) {
          if (Pred->succ_size() <= 1 || MBB->pred_size() <= 1)
            continue;
          MachineBasicBlock *EdgeMBB = Pred->SplitCriticalEdge(MBB, *this);
          if (!EdgeMBB)
            report_fatal_error(
                "Linx: cannot split critical edge for tile PHI balancing");
          Changed = true;
          SplitOne = true;
          break;
        }
      } while (SplitOne);

      for (MachineInstr &MI : MBB->phis()) {
        Register Dst = MI.getOperand(0).getReg();
        if (!Dst.isVirtual() || !TRI.getCommonSubClass(MRI.getRegClass(Dst),
                                                       &LinxISA::TILERegClass))
          continue;

        for (unsigned I = 1; I + 1 < MI.getNumOperands(); I += 2) {
          MachineOperand &SrcMO = MI.getOperand(I);
          MachineBasicBlock *Pred = MI.getOperand(I + 1).getMBB();
          Register Src = SrcMO.getReg();
          if (!Src.isVirtual())
            report_fatal_error(
                "Linx: tile PHI source must be virtual before allocation");

          if (MachineInstr *Def = MRI.getVRegDef(Src);
              Def && Def->getOpcode() == LinxISA::PSEUDO_TILE_EDGE_COPY &&
              Def->getParent() == Pred)
            continue;

          Register EdgeValue =
              MRI.createVirtualRegister(&LinxISA::TILERegClass);
          BuildMI(*Pred, Pred->getFirstTerminator(), MI.getDebugLoc(),
                  TII.get(LinxISA::PSEUDO_TILE_EDGE_COPY), EdgeValue)
              .addReg(Src, getUndefRegState(SrcMO.isUndef()));
          SrcMO.setReg(EdgeValue);
          SrcMO.setIsUndef(false);
          Changed = true;
        }
      }
    }
    return Changed;
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    if (!MF.getProperties().hasProperty(
            MachineFunctionProperties::Property::NoVRegs))
      return prepareTilePHIEdges(MF);

    const auto &TRI = *MF.getSubtarget().getRegisterInfo();
    const auto &TII = *MF.getSubtarget().getInstrInfo();
    MachineRegisterInfo &MRI = MF.getRegInfo();
    MachineFrameInfo &MFI = MF.getFrameInfo();

    DenseMap<unsigned, TileMeta> RegMetaById;

    auto mergeDefMetadata = [&](Register Reg, const TileMeta &Incoming,
                                const MachineInstr &DefMI) {
      if (!isTilePhysReg(Reg))
        return;

      if (Incoming.HasSize && !isStrictTileTSizeCode(Incoming.TSize)) {
        reportTileBalanceError(
            MF, DefMI,
            Twine("SizeCode outside 128B..64KB policy (size=") +
                Twine(unsigned(Incoming.TSize)) + ")");
      }

      const unsigned TileId = getTileRegId(TRI, Reg);
      auto It = RegMetaById.find(TileId);
      if (It == RegMetaById.end()) {
        RegMetaById.insert({TileId, Incoming});
        return;
      }

      std::string Reason;
      if (!metadataCompatible(It->second, Incoming, Reason)) {
        // Physical tile registers are reused across independent defs in a
        // function. Strict metadata rejection is enforced on COPY/PHI edge
        // normalization where both source and destination values participate
        // in the same flow relation.
        It->second = Incoming;
        return;
      }
      It->second = mergeMetadata(It->second, Incoming);
    };

    auto emitTMOVPseudo = [&](MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator InsertPt,
                              const DebugLoc &DL, Register Dst, Register Src,
                              const TileMeta &Meta, bool SrcKill, bool SrcReuse,
                              bool DstDead, bool SrcUndef) -> MachineInstr * {
      MachineInstrBuilder MIB =
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::PSEUDO_TLSU_TMOV), Dst)
              .addReg(Src, getKillRegState(SrcKill))
              .addImm(Meta.TSize)
              .addImm(Meta.HasDataType ? Meta.DataType : 0)
              .addImm(Meta.HasLayout ? Meta.Layout : 0)
              .addImm(Meta.HasLayout ? 1 : 0)
              .addImm(SrcReuse ? 1 : 0);

      if (DstDead)
        MIB->getOperand(0).setIsDead();
      if (SrcUndef)
        MIB->getOperand(1).setIsUndef();
      return MIB.getInstr();
    };

    auto emitSpillStore =
        [&](MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt,
            const DebugLoc &DL, int SpillSlotFI, Register Src, uint8_t TSize) {
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::PSEUDO_TLSU_TSTORE))
              .addFrameIndex(SpillSlotFI)
              .addReg(Src)
              .addImm(TSize);
        };

    auto emitSpillLoad = [&](MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator InsertPt,
                             const DebugLoc &DL, Register Dst, int SpillSlotFI,
                             uint8_t TSize, bool DstDead) {
      MachineInstrBuilder MIB =
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::PSEUDO_TLSU_TLOAD_ANY),
                  Dst)
              .addFrameIndex(SpillSlotFI)
              .addImm(TSize);
      if (DstDead)
        MIB->getOperand(0).setIsDead();
    };

    // Seed metadata from tile-value defining pseudos.
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        TileMeta Meta;
        if (!extractDefMetadata(MI, Meta))
          continue;

        for (const MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.isDef() || MO.isImplicit())
            continue;
          mergeDefMetadata(MO.getReg(), Meta, MI);
        }
      }
    }

    bool Changed = false;

    for (MachineBasicBlock &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end();) {
        MachineInstr &MI = *It;
        if (!isTileCopyInstr(MI, TRI, /*DstOut=*/nullptr,
                             /*SrcOut=*/nullptr)) {
          ++It;
          continue;
        }

        SmallVector<MachineInstr *, 8> CopyBundle;
        auto Scan = It;
        while (Scan != MBB.end()) {
          Register BundleDst;
          Register BundleSrc;
          if (!isTileCopyInstr(*Scan, TRI, &BundleDst, &BundleSrc))
            break;
          CopyBundle.push_back(&*Scan);
          ++Scan;
        }
        It = Scan;
        if (CopyBundle.empty())
          continue;

        DenseMap<unsigned, unsigned> BundleSrcByDst;
        for (MachineInstr *CopyMI : CopyBundle) {
          const unsigned DstId =
              getTileRegId(TRI, CopyMI->getOperand(0).getReg());
          const unsigned SrcId =
              getTileRegId(TRI, CopyMI->getOperand(1).getReg());
          BundleSrcByDst.try_emplace(DstId, SrcId);
        }
        DenseMap<unsigned, TileMeta> BundleResolvedMeta;
        auto resolveMetaForTile = [&](auto &&Self, unsigned TileId,
                                      SmallVectorImpl<unsigned> &Visiting)
            -> std::optional<TileMeta> {
          auto ResolvedIt = BundleResolvedMeta.find(TileId);
          if (ResolvedIt != BundleResolvedMeta.end())
            return ResolvedIt->second;

          auto GlobalIt = RegMetaById.find(TileId);
          if (GlobalIt != RegMetaById.end()) {
            BundleResolvedMeta[TileId] = GlobalIt->second;
            return GlobalIt->second;
          }

          for (unsigned V : Visiting)
            if (V == TileId)
              return std::nullopt;

          auto EdgeIt = BundleSrcByDst.find(TileId);
          if (EdgeIt == BundleSrcByDst.end())
            return std::nullopt;

          Visiting.push_back(TileId);
          std::optional<TileMeta> SrcMeta =
              Self(Self, EdgeIt->second, Visiting);
          Visiting.pop_back();
          if (!SrcMeta)
            return std::nullopt;

          BundleResolvedMeta[TileId] = *SrcMeta;
          return *SrcMeta;
        };

        SmallVector<TileCopyOp, 8> Ops;
        Ops.reserve(CopyBundle.size());
        for (MachineInstr *CopyMI : CopyBundle) {
          TileCopyOp Op;
          Op.MI = CopyMI;
          Op.Dst = CopyMI->getOperand(0).getReg();
          Op.Src = CopyMI->getOperand(1).getReg();
          Op.PreserveIdentity =
              CopyMI->getOpcode() == LinxISA::PSEUDO_TILE_EDGE_COPY;
          Op.DstDead = CopyMI->getOperand(0).isDead();
          Op.SrcUndef = CopyMI->getOperand(1).isUndef();
          Op.SrcKillHint = CopyMI->getOperand(1).isKill();

          const unsigned SrcId = getTileRegId(TRI, Op.Src);
          const unsigned DstId = getTileRegId(TRI, Op.Dst);

          SmallVector<unsigned, 8> Visiting;
          std::optional<TileMeta> SrcMeta =
              resolveMetaForTile(resolveMetaForTile, SrcId, Visiting);
          if (!SrcMeta || !SrcMeta->HasSize) {
            reportTileBalanceError(
                MF, *CopyMI,
                Twine(
                    "tile COPY source lacks required size metadata (src id=") +
                    Twine(SrcId) + ")");
          }

          TileMeta CopyMeta = *SrcMeta;
          if (!isStrictTileTSizeCode(CopyMeta.TSize)) {
            reportTileBalanceError(
                MF, *CopyMI,
                Twine("SizeCode outside 128B..64KB policy (src id=") +
                    Twine(SrcId) + ", size=" + Twine(unsigned(CopyMeta.TSize)) +
                    ")");
          }

          if (auto DstMetaIt = RegMetaById.find(DstId);
              DstMetaIt != RegMetaById.end()) {
            std::string Reason;
            if (!metadataCompatible(DstMetaIt->second, CopyMeta, Reason)) {
              reportTileBalanceError(
                  MF, *CopyMI,
                  Twine(
                      "tile COPY metadata mismatch across CFG edges (dst id=") +
                      Twine(DstId) + ", src id=" + Twine(SrcId) +
                      "): " + Reason);
            }
            CopyMeta = mergeMetadata(CopyMeta, DstMetaIt->second);
          }
          if (auto DstResolvedIt = BundleResolvedMeta.find(DstId);
              DstResolvedIt != BundleResolvedMeta.end()) {
            std::string Reason;
            if (!metadataCompatible(DstResolvedIt->second, CopyMeta, Reason)) {
              reportTileBalanceError(MF, *CopyMI,
                                     Twine("tile COPY metadata mismatch in "
                                           "entry normalization (dst id=") +
                                         Twine(DstId) + ", src id=" +
                                         Twine(SrcId) + "): " + Reason);
            }
            CopyMeta = mergeMetadata(CopyMeta, DstResolvedIt->second);
          }

          const TileRelRef SrcRef = decodeTileRelRef(SrcId, /*Reuse=*/false);
          const TileRelRef DstRef = decodeTileRelRef(DstId, /*Reuse=*/false);
          if (SrcRef.Depth < 1 || SrcRef.Depth > 8 || DstRef.Depth < 1 ||
              DstRef.Depth > 8) {
            reportTileBalanceError(MF, *CopyMI,
                                   "invalid relref depth outside 1..8");
          }
          (void)encodeTileRelRef(SrcRef);
          (void)encodeTileRelRef(DstRef);

          Op.Meta = CopyMeta;
          BundleResolvedMeta[DstId] = CopyMeta;
          Ops.push_back(Op);
        }

        auto FirstNonMeta = MBB.begin();
        while (FirstNonMeta != MBB.end() &&
               (FirstNonMeta->isPHI() || FirstNonMeta->isDebugInstr() ||
                FirstNonMeta->isCFIInstruction()))
          ++FirstNonMeta;
        const bool AtBlockEntry =
            CopyBundle.front()->getIterator() == FirstNonMeta;
        bool AtBlockTail = true;
        for (auto After = std::next(CopyBundle.back()->getIterator());
             After != MBB.end(); ++After) {
          if (After->isDebugInstr() || After->isCFIInstruction())
            continue;
          if (!After->isTerminator()) {
            AtBlockTail = false;
            break;
          }
        }
        const bool ParallelBundle = AtBlockEntry || AtBlockTail;

        MachineBasicBlock::iterator InsertPt =
            CopyBundle.front()->getIterator();
        if (!ParallelBundle) {
          // Non-edge COPY chains keep sequential semantics.
          for (TileCopyOp &Op : Ops) {
            const bool SrcReuse = !Op.SrcKillHint;
            const bool SrcKill = !SrcReuse;
            MachineInstr *TMOV = emitTMOVPseudo(
                MBB, InsertPt, Op.MI->getDebugLoc(), Op.Dst, Op.Src, Op.Meta,
                SrcKill, SrcReuse, Op.DstDead, Op.SrcUndef);
            if (Op.PreserveIdentity)
              TMOV->setFlag(MachineInstr::NoMerge);
            const unsigned DstId = getTileRegId(TRI, Op.Dst);
            RegMetaById[DstId] = Op.Meta;
          }
          for (MachineInstr *CopyMI : CopyBundle)
            CopyMI->eraseFromParent();
          Changed = true;
          continue;
        }

        SmallVector<uint8_t, 8> Pending(Ops.size(), 1u);
        unsigned PendingCount = Ops.size();

        auto pendingSourceUsesReg = [&](Register Reg,
                                        std::optional<unsigned> Excl) -> bool {
          for (unsigned I = 0; I < Ops.size(); ++I) {
            if (!Pending[I])
              continue;
            if (Excl && *Excl == I)
              continue;
            if (Ops[I].SrcFromSpill)
              continue;
            if (Ops[I].Src == Reg)
              return true;
          }
          return false;
        };

        auto reservedTempUsable = [&]() -> bool {
          Register Temp = reservedPhiCycleTempTile();
          if (!isTilePhysReg(Temp))
            return false;
          if (pendingSourceUsesReg(Temp, std::nullopt))
            return false;
          for (unsigned I = 0; I < Ops.size(); ++I) {
            if (!Pending[I])
              continue;
            if (Ops[I].Dst == Temp)
              return false;
          }
          return true;
        };

        auto replacePendingSrcReg = [&](Register OldSrc, Register NewSrc) {
          for (unsigned I = 0; I < Ops.size(); ++I) {
            if (!Pending[I] || Ops[I].SrcFromSpill)
              continue;
            if (Ops[I].Src != OldSrc)
              continue;
            Ops[I].Src = NewSrc;
            Ops[I].SrcKillHint = false;
            Ops[I].SrcUndef = false;
          }
        };

        auto replacePendingSrcWithSpill = [&](Register OldSrc,
                                              int SpillSlotFI) {
          for (unsigned I = 0; I < Ops.size(); ++I) {
            if (!Pending[I] || Ops[I].SrcFromSpill)
              continue;
            if (Ops[I].Src != OldSrc)
              continue;
            Ops[I].SrcFromSpill = true;
            Ops[I].SrcSpillFI = SpillSlotFI;
            Ops[I].Src = Register();
            Ops[I].SrcKillHint = false;
            Ops[I].SrcUndef = false;
          }
        };

        while (PendingCount > 0) {
          bool Progress = false;

          for (unsigned I = 0; I < Ops.size(); ++I) {
            if (!Pending[I])
              continue;

            TileCopyOp &Op = Ops[I];
            // Parallel-copy legality: we can write Dst only when no pending op
            // still needs that register as a source. Identity copies are
            // always legal.
            if (Op.Dst != Op.Src && pendingSourceUsesReg(Op.Dst, std::nullopt))
              continue;

            if (Op.SrcFromSpill) {
              if (Op.SrcSpillFI < 0) {
                reportTileBalanceError(MF, *Op.MI,
                                       "internal error: missing spill slot");
              }
              emitSpillLoad(MBB, InsertPt, Op.MI->getDebugLoc(), Op.Dst,
                            Op.SrcSpillFI, Op.Meta.TSize, Op.DstDead);
            } else {
              const bool SrcStillNeeded = pendingSourceUsesReg(Op.Src, I);
              // Edge normalization must leave every predecessor with the
              // same queue state before appending the common destination.
              // A branch-local kill hint cannot consume a source on only one
              // incoming edge; doing so makes the hidden queue order diverge
              // at the join. Parallel edge copies therefore always reuse
              // their sources. Non-edge sequential copies retain the
              // last-use optimization above.
              const bool SrcReuse =
                  ParallelBundle || SrcStillNeeded || !Op.SrcKillHint;
              const bool SrcKill = !SrcReuse;
              MachineInstr *TMOV = emitTMOVPseudo(
                  MBB, InsertPt, Op.MI->getDebugLoc(), Op.Dst, Op.Src, Op.Meta,
                  SrcKill, SrcReuse, Op.DstDead, Op.SrcUndef);
              if (Op.PreserveIdentity)
                TMOV->setFlag(MachineInstr::NoMerge);
            }

            const unsigned DstId = getTileRegId(TRI, Op.Dst);
            RegMetaById[DstId] = Op.Meta;
            Pending[I] = 0u;
            --PendingCount;
            Progress = true;
          }

          if (Progress)
            continue;

          // Cycle: no legal destination can be written yet.
          unsigned BreakI = 0;
          while (BreakI < Ops.size() && !Pending[BreakI])
            ++BreakI;
          if (BreakI >= Ops.size())
            break;

          TileCopyOp &BreakOp = Ops[BreakI];
          if (BreakOp.SrcFromSpill || !isTilePhysReg(BreakOp.Src)) {
            reportTileBalanceError(
                MF, *BreakOp.MI,
                "unable to break tile COPY cycle (invalid source)");
          }

          const Register BreakSrc = BreakOp.Src;
          const TileMeta BreakMeta = BreakOp.Meta;

          if (reservedTempUsable()) {
            const Register Temp = reservedPhiCycleTempTile();
            emitTMOVPseudo(MBB, InsertPt, BreakOp.MI->getDebugLoc(), Temp,
                           BreakSrc, BreakMeta,
                           /*SrcKill=*/false,
                           /*SrcReuse=*/true,
                           /*DstDead=*/false,
                           /*SrcUndef=*/false);
            const unsigned TempId = getTileRegId(TRI, Temp);
            RegMetaById[TempId] = BreakMeta;
            replacePendingSrcReg(BreakSrc, Temp);
          } else {
            // Fallback: spill one cycle source to stack, then materialize it
            // via TLOAD_ANY at the consuming destination.
            const int SpillSlotFI =
                MFI.CreateStackObject(/*Size=*/4096, Align(64),
                                      /*isSpillSlot=*/false);
            emitSpillStore(MBB, InsertPt, BreakOp.MI->getDebugLoc(),
                           SpillSlotFI, BreakSrc, BreakMeta.TSize);
            replacePendingSrcWithSpill(BreakSrc, SpillSlotFI);
          }
        }

        for (MachineInstr *CopyMI : CopyBundle)
          CopyMI->eraseFromParent();
        Changed = true;
      }
    }

    // PR3 relay-minimization: fold trivial TMOV relay chains.
    for (MachineBasicBlock &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end();) {
        MachineInstr &MI = *It;
        ++It;

        if (!isTileTMOVPseudo(MI))
          continue;
        if (MI.getNumOperands() < 7)
          continue;

        MachineOperand &DstMO = MI.getOperand(0);
        MachineOperand &SrcMO = MI.getOperand(1);
        if (!DstMO.isReg() || !SrcMO.isReg())
          continue;
        Register Dst = DstMO.getReg();
        Register Src = SrcMO.getReg();
        if (!isTilePhysReg(Dst) || !isTilePhysReg(Src))
          continue;

        // Identity relay: no semantic effect.
        if (Dst == Src && !MI.getFlag(MachineInstr::NoMerge)) {
          MI.eraseFromParent();
          Changed = true;
          continue;
        }

        // One-hop relay fold:
        //   t1 = TMOV t0
        //   t2 = TMOV t1   =>   t2 = TMOV t0
        auto NextIt = It;
        while (NextIt != MBB.end() &&
               (NextIt->isDebugInstr() || NextIt->isCFIInstruction()))
          ++NextIt;
        if (NextIt == MBB.end())
          continue;
        MachineInstr &NextMI = *NextIt;
        if (!isTileTMOVPseudo(NextMI) || !canRelayFoldTMOV(MI, NextMI))
          continue;
        if (NextMI.getNumOperands() < 7)
          continue;
        if (!NextMI.getOperand(1).isReg() ||
            NextMI.getOperand(1).getReg() != Dst)
          continue;
        if (!MRI.hasOneNonDBGUse(Dst))
          continue;

        Register NewSrc = Src;
        NextMI.getOperand(1).setReg(NewSrc);
        // Keep conservative liveness/relay semantics after rewiring.
        NextMI.getOperand(1).setIsKill(false);
        NextMI.getOperand(6).setImm(1); // src_reuse=1

        MI.eraseFromParent();
        Changed = true;
      }
    }

    return Changed;
  }
};

char LinxISATileSSABalance::ID = 0;

} // namespace

INITIALIZE_PASS(LinxISATileSSABalance, "linx-tile-ssa-balance",
                "Linx Tile SSA edge balancing", false, false)

FunctionPass *llvm::createLinxISATileSSABalancePass() {
  return new LinxISATileSSABalance();
}
