//===- LinxISA.cpp --------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "OutputSections.h"
#include "Symbols.h"
#include "Target.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/Endian.h"
#include <cstring>
#include <optional>

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

static uint32_t encodeB12Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                              const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned branch target";

  // The split field carries a signed halfword delta.
  int64_t imm = value >> 1;
  checkInt(ctx, loc, imm, 12, rel);
  uint32_t uimm = static_cast<uint32_t>(imm) & 0x0FFFu;
  return ((uimm & 0x07Fu) << 25) | (((uimm >> 7) & 0x01Fu) << 7);
}

static uint32_t encodeJ22Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                              const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned jump target";

  int64_t imm = value >> 1;
  checkInt(ctx, loc, imm, 22, rel);
  uint32_t uimm = static_cast<uint32_t>(imm) & 0x003FFFFFu;
  return ((uimm & 0x1FFFFu) << 15) | (((uimm >> 17) & 0x01Fu) << 7);
}

static uint16_t encodeCBStart12Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                                     const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned C.BSTART target";

  int64_t imm = value >> 1;
  checkInt(ctx, loc, imm, 12, rel);
  uint16_t uimm = static_cast<uint16_t>(imm) & 0x0FFFu;
  return uimm << 4;
}

static uint32_t encodeB17Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                              const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned BSTART target";

  int64_t imm = value >> 1;
  checkInt(ctx, loc, imm, 17, rel);
  uint32_t uimm = static_cast<uint32_t>(imm) & 0x1FFFFu;
  return uimm << 15;
}

static uint32_t encodeB25Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                               const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned B.TEXT target";

  int64_t imm = value >> 1;
  checkInt(ctx, loc, imm, 25, rel);
  uint32_t uimm = static_cast<uint32_t>(imm) & 0x01FFFFFFu; // 25 bits
  return uimm << 7;
}

static uint64_t encodeHLBStart30Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                                      const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned HL.BSTART target";

  checkInt(ctx, loc, value, 30, rel);
  uint64_t uimm = static_cast<uint64_t>(value) & 0x3FFFFFFFull;
  uint64_t patch = 0;
  patch |= ((uimm >> 1) & 0x1FFFFull) << 31;   // simm[17:1] -> insn[47:31]
  patch |= ((uimm >> 18) & 0x0FFFull) << 4;    // simm[29:18] -> insn[15:4]
  return patch;
}

static uint64_t encodeLBStart42Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                                     const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned L.BSTART target";

  int64_t imm = value >> 1;
  checkInt(ctx, loc, imm, 42, rel);
  uint64_t uimm = static_cast<uint64_t>(imm) & 0x3FF'FFFF'FFFFull;
  uint64_t patch = 0;
  // simm[24:0] -> insn[31:7]
  patch |= (uimm & 0x1FF'FFFFull) << 7;
  // simm[41:25] -> insn[63:47]
  patch |= ((uimm >> 25) & 0x1FFFFull) << 47;
  return patch;
}

static uint16_t encodeCSetRet5Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                                   const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned C.SETRET target";

  uint64_t imm = static_cast<uint64_t>(value) >> 1;
  checkUInt(ctx, loc, imm, 5, rel);
  return (static_cast<uint16_t>(imm) & 0x001Fu) << 6;
}

static uint32_t encodeSetRet20Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                                   const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned SETRET target";

  uint64_t imm = static_cast<uint64_t>(value) >> 1;
  checkUInt(ctx, loc, imm, 20, rel);
  return (static_cast<uint32_t>(imm) & 0x000FFFFFu) << 12;
}

static uint64_t encodeHLSetRet32Pcrel(Ctx &ctx, uint8_t *loc, int64_t value,
                                      const Relocation &rel) {
  if (value & 0x1)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned HL.SETRET target";

  int64_t imm = value >> 1;
  checkInt(ctx, loc, imm, 32, rel);
  uint64_t uimm = static_cast<uint64_t>(imm) & 0xFFFF'FFFFull;
  uint64_t patch = 0;
  patch |= (uimm & 0xFFFFFull) << 28;          // imm[19:0] -> insn[47:28]
  patch |= ((uimm >> 20) & 0x0FFFull) << 4;    // imm[31:20] -> insn[15:4]
  return patch;
}

static uint32_t encodePcrelHi20(Ctx &ctx, uint8_t *loc, int64_t value,
                                const Relocation &rel) {
  // ADDTPC uses an imm20 field in bits [31:12] which is added to the current
  // TPC/PC page base (i.e. the immediate is scaled by 4KiB).
  if (value & 0xFFF)
    Err(ctx) << getErrorLoc(ctx, loc) << "unaligned ADDTPC target";

  int64_t imm = value >> 12;
  checkInt(ctx, loc, imm, 20, rel);
  uint32_t uimm = static_cast<uint32_t>(imm) & 0x000FFFFFu;
  return uimm << 12;
}

static uint32_t encodeLo12(Ctx &ctx, uint8_t *loc, int64_t value,
                           const Relocation &rel) {
  // Low 12 bits for ADDI/ADDIW uimm12 (bits [31:20]).
  (void)ctx;
  (void)loc;
  (void)rel;
  uint32_t uimm = static_cast<uint32_t>(value) & 0x00000FFFu;
  return uimm << 20;
}

static uint32_t encodePcr17Load(Ctx &ctx, uint8_t *loc, int64_t value,
                                const Relocation &rel) {
  // *.PCR loads use a signed 17-bit byte offset in bits [31:15].
  checkInt(ctx, loc, value, 17, rel);
  uint32_t uimm = static_cast<uint32_t>(value) & 0x1FFFFu;
  return uimm << 15;
}

static uint32_t encodePcr17Store(Ctx &ctx, uint8_t *loc, int64_t value,
                                 const Relocation &rel) {
  // *.PCR stores use a signed 17-bit byte offset split across:
  //   simm[11:0]  -> insn[31:20]
  //   simm[16:12] -> insn[11:7]
  checkInt(ctx, loc, value, 17, rel);
  uint32_t uimm = static_cast<uint32_t>(value) & 0x1FFFFu;
  uint32_t patch = 0;
  patch |= (uimm & 0x0FFFu) << 20;
  patch |= ((uimm >> 12) & 0x1Fu) << 7;
  return patch;
}

static uint64_t encodeHLPcr29Load(Ctx &ctx, uint8_t *loc, int64_t value,
                                  const Relocation &rel) {
  // HL.*.PCR loads use a signed 29-bit byte offset split across:
  //   simm[16:0]  -> insn[47:31]
  //   simm[28:17] -> insn[15:4]
  checkInt(ctx, loc, value, 29, rel);
  uint64_t uimm = static_cast<uint64_t>(value) & 0x1FFF'FFFFull;
  uint64_t patch = 0;
  patch |= (uimm & 0x1FFFFull) << 31;
  patch |= ((uimm >> 17) & 0x0FFFull) << 4;
  return patch;
}

static uint64_t encodeHLPcr29Store(Ctx &ctx, uint8_t *loc, int64_t value,
                                   const Relocation &rel) {
  // HL.*.PCR stores use a signed 29-bit byte offset split across:
  //   simm[11:0]  -> insn[47:36]
  //   simm[16:12] -> insn[27:23]
  //   simm[28:17] -> insn[15:4]
  checkInt(ctx, loc, value, 29, rel);
  uint64_t uimm = static_cast<uint64_t>(value) & 0x1FFF'FFFFull;
  uint64_t patch = 0;
  patch |= (uimm & 0x0FFFull) << 36;
  patch |= ((uimm >> 12) & 0x01Full) << 23;
  patch |= ((uimm >> 17) & 0x0FFFull) << 4;
  return patch;
}

static uint64_t read48le(const uint8_t *loc) {
  uint64_t v = 0;
  for (unsigned i = 0; i < 6; ++i)
    v |= static_cast<uint64_t>(loc[i]) << (i * 8);
  return v;
}

static bool isEvenUnsignedN(int64_t v, unsigned bitsAfterShift) {
  if (v < 0 || (v & 1))
    return false;
  return isUIntN(bitsAfterShift, static_cast<uint64_t>(v >> 1));
}

// Internal marker used by Linx relaxation to rewrite a relocation into
// R_LINX_NONE while still participating in section-shrink accounting.
static constexpr RelType INTERNAL_R_LINX_REMOVE =
    static_cast<RelType>(0x80000000u);

struct SeqFusionMatch {
  RelType newType;
  uint32_t newInsnBits;
};

static std::optional<SeqFusionMatch>
tryRelaxAddrSeqToPcr(Ctx &ctx, const Relocation &hiRel, const Relocation &loRel,
                     ArrayRef<uint8_t> content) {
  if (!ctx.arg.linxRelaxSeqFusion)
    return std::nullopt;
  if (hiRel.type != R_LINX_PCREL_HI20 || loRel.type != R_LINX_LO12)
    return std::nullopt;
  if (loRel.offset != hiRel.offset + 4)
    return std::nullopt;
  if (!hiRel.sym || hiRel.sym != loRel.sym)
    return std::nullopt;
  if (hiRel.addend != loRel.addend)
    return std::nullopt;
  if (hiRel.sym->isPreemptible || hiRel.sym->isGnuIFunc())
    return std::nullopt;
  if (ctx.arg.isPic) {
    if (auto *d = dyn_cast<Defined>(hiRel.sym); d && !d->section)
      return std::nullopt;
  }
  if (hiRel.offset + 12 > content.size())
    return std::nullopt;

  const uint32_t addtpc = read32le(content.data() + hiRel.offset);
  const uint32_t addi = read32le(content.data() + loRel.offset);
  const uint32_t mem = read32le(content.data() + loRel.offset + 4);

  // ADDTPC + ADDI/ADDIW.
  if ((addtpc & 0x7fu) != 0x07u)
    return std::nullopt;
  const uint32_t addiOpc = addi & 0x7fu;
  if (addiOpc != 0x15u && addiOpc != 0x35u)
    return std::nullopt;

  // Queue-based destinations (->t/->u) don't use the same encoded register
  // number as queue consumers (t#1/u#1). Accept the known producer->consumer
  // pairs in addition to direct register equality.
  auto isQueueFlow = [](uint32_t producerDst, uint32_t consumerSrc) {
    if (producerDst == 31u && consumerSrc == 24u) // ->t  to t#1
      return true;
    if (producerDst == 30u && consumerSrc == 28u) // ->u  to u#1
      return true;
    return producerDst == consumerSrc;
  };

  const uint32_t addtpcDst = (addtpc >> 7) & 0x1fu;
  const uint32_t addiSrc = (addi >> 15) & 0x1fu;
  if (!isQueueFlow(addtpcDst, addiSrc))
    return std::nullopt;
  // Keep the first cut conservative: only fuse when ADDI(W) carries no
  // explicit immediate bits in-place (symbol addends still come from relocs).
  if (((addi >> 20) & 0x0fffu) != 0)
    return std::nullopt;

  const uint32_t memKind = mem & 0x707fu;

  // LDI/LWI [addiDst, 0], ->rd  ==>  LD/LW.PCR [sym], ->rd
  if (memKind == 0x3019u || memKind == 0x2019u) {
    const uint32_t base = (mem >> 15) & 0x1fu;
    const uint32_t imm12 = (mem >> 20) & 0x0fffu;
    if (base != addiSrc || imm12 != 0)
      return std::nullopt;
    const uint32_t rd = (mem >> 7) & 0x1fu;
    const uint32_t pcrBase = (memKind == 0x3019u) ? 0x3039u : 0x2039u;
    return SeqFusionMatch{R_LINX_PCR17_LOAD, pcrBase | (rd << 7)};
  }

  // SDI/SWI rs, [addiDst, 0]  ==>  SD/SW.PCR rs, [sym]
  if (memKind == 0x3059u || memKind == 0x2059u) {
    const uint32_t base = (mem >> 20) & 0x1fu;
    const uint32_t imm12 =
        (((mem >> 25) & 0x7fu) << 5) | ((mem >> 7) & 0x1fu);
    if (base != addiSrc || imm12 != 0)
      return std::nullopt;
    const uint32_t src = (mem >> 15) & 0x1fu;
    const uint32_t pcrBase = (memKind == 0x3059u) ? 0x3069u : 0x2069u;
    return SeqFusionMatch{R_LINX_PCR17_STORE, pcrBase | (src << 15)};
  }

  return std::nullopt;
}

static bool relax(Ctx &ctx, InputSection &sec) {
  if (!ctx.arg.relax)
    return false;

  MutableArrayRef<Relocation> relocs = sec.relocs();
  if (relocs.empty())
    return false;

  auto &aux = *sec.relaxAux;
  ArrayRef<uint8_t> content = sec.content();
  const uint64_t secAddr = sec.getVA();
  ArrayRef<SymbolAnchor> sa = ArrayRef(aux.anchors);
  uint64_t delta = 0;
  bool changed = false;
  struct PendingFusion {
    size_t relocIndex;
    RelType newType;
    uint32_t newInsnBits;
    uint32_t remove;
  };
  std::optional<PendingFusion> pendingFusion;

  aux.writes.clear();
  for (auto [i, rel] : llvm::enumerate(relocs)) {
    uint32_t &curDelta = aux.relocDeltas[i];
    const RelType prevType = aux.relocTypes[i];
    RelType newType = R_LINX_NONE;
    uint32_t remove = 0;
    uint32_t newInsnBits = 0;

    const uint64_t loc = secAddr + rel.offset - delta;
    const int64_t sval =
        static_cast<int64_t>(sec.getRelocTargetVA(ctx, rel, loc));
    const uint8_t *insn = content.data() + rel.offset;

    if (pendingFusion && pendingFusion->relocIndex == i) {
      newType = pendingFusion->newType;
      remove = pendingFusion->remove;
      newInsnBits = pendingFusion->newInsnBits;
      pendingFusion.reset();
    } else {
      if (i + 1 < relocs.size()) {
        if (std::optional<SeqFusionMatch> m =
                tryRelaxAddrSeqToPcr(ctx, rel, relocs[i + 1], content)) {
          // Ensure the fused relocation fits the signed 17-bit *.PCR range at
          // the final fused instruction location.
          Relocation fusedRel{R_PC, m->newType, rel.offset, rel.addend, rel.sym};
          const int64_t pcrVal = static_cast<int64_t>(
              sec.getRelocTargetVA(ctx, fusedRel, loc));
          if (isInt<17>(pcrVal)) {
            // Reloc i: drop ADDTPC (4 bytes) and clear its relocation.
            // Reloc i+1: rewrite ADDI(W)+mem into a single *.PCR op and
            //            retarget the LO12 relocation to PCR17_*.
            newType = INTERNAL_R_LINX_REMOVE;
            remove = 4;
            pendingFusion =
                PendingFusion{i + 1, m->newType, m->newInsnBits, 4};
          }
        }
      }
    }

    if (newType == R_LINX_NONE) {
      switch (rel.type) {
      case R_LINX_HL_PCR29_LOAD:
        if (isInt<17>(sval) && rel.offset + 6 <= content.size()) {
          newType = R_LINX_PCR17_LOAD;
          remove = 2;
          newInsnBits = static_cast<uint32_t>(read48le(insn) >> 16);
        }
        break;
      case R_LINX_HL_PCR29_STORE:
        if (isInt<17>(sval) && rel.offset + 6 <= content.size()) {
          newType = R_LINX_PCR17_STORE;
          remove = 2;
          newInsnBits = static_cast<uint32_t>(read48le(insn) >> 16);
        }
        break;
      case R_LINX_HL_SETRET32_PCREL:
        if (rel.offset + 6 > content.size())
          break;
        if (isEvenUnsignedN(sval, /*bitsAfterShift=*/5)) {
          const uint64_t oldInsn = read48le(insn);
          const uint16_t csetret =
              static_cast<uint16_t>(0x5016u | (((oldInsn >> 28) & 0x1Fu) << 6));
          newType = R_LINX_CSETRET5_PCREL;
          remove = 4;
          newInsnBits = csetret;
        } else if (isEvenUnsignedN(sval, /*bitsAfterShift=*/20)) {
          newType = R_LINX_SETRET20_PCREL;
          remove = 2;
          newInsnBits = static_cast<uint32_t>(read48le(insn) >> 16);
        }
        break;
      case R_LINX_HL_BSTART30_PCREL:
        if (isInt<17>(sval) && (sval & 1) == 0 &&
            rel.offset + 6 <= content.size()) {
          newType =
              (rel.expr == R_PLT_PC) ? R_LINX_B17_PLT : R_LINX_B17_PCREL;
          remove = 2;
          newInsnBits = static_cast<uint32_t>(read48le(insn) >> 16);
        }
        break;
      default:
        break;
      }
    }

    // For all anchors whose offsets are <= rel.offset, they are preceded by
    // the previous relocation whose relocDeltas value equals `delta`.
    for (; sa.size() && sa[0].offset <= rel.offset; sa = sa.slice(1)) {
      if (sa[0].end)
        sa[0].d->size = sa[0].offset - delta - sa[0].d->value;
      else
        sa[0].d->value = sa[0].offset - delta;
    }

    aux.relocTypes[i] = newType;
    if (newType != R_LINX_NONE && newType != INTERNAL_R_LINX_REMOVE)
      aux.writes.push_back(newInsnBits);

    delta += remove;
    if (curDelta != delta || prevType != newType) {
      changed = true;
      curDelta = delta;
    }
  }

  for (const SymbolAnchor &a : sa) {
    if (a.end)
      a.d->size = a.offset - delta - a.d->value;
    else
      a.d->value = a.offset - delta;
  }

  if (!isUInt<32>(delta))
    Err(ctx) << "section size decrease is too large: " << delta;
  sec.bytesDropped = delta;
  return changed;
}

class LinxISA final : public TargetInfo {
public:
  LinxISA(Ctx &ctx);
  RelType getDynRel(RelType type) const override;
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  bool usesOnlyLowPageBits(RelType type) const override;
  void writeGotPlt(uint8_t *buf, const Symbol &s) const override;
  void writePlt(uint8_t *buf, const Symbol &sym,
                uint64_t pltEntryAddr) const override;
  bool relaxOnce(int pass) const override;
  void finalizeRelax(int passes) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
};

} // namespace

LinxISA::LinxISA(Ctx &ctx) : TargetInfo(ctx) {
  copyRel = R_LINX_COPY;
  pltRel = R_LINX_JUMP_SLOT;
  relativeRel = R_LINX_RELATIVE;
  iRelativeRel = R_LINX_IRELATIVE;
  symbolicRel = ctx.arg.is64 ? R_LINX_64 : R_LINX_32;
  tlsModuleIndexRel = R_LINX_TLS_DTPMOD64;
  tlsOffsetRel = R_LINX_TLS_DTPREL64;
  tlsGotRel = R_LINX_TLS_TPREL64;
  tlsDescRel = R_LINX_TLSDESC;
  gotRel = symbolicRel;

  // The LinxISA PLT is implemented as a sequence of standalone blocks. Each
  // entry begins with a BSTART marker so that control-flow is only transferred
  // to valid block boundaries. The entry loads the resolved address from the
  // corresponding .got.plt slot and performs an indirect jump via SETC.TGT.
  //
  // Bring-up note: this is a non-lazy PLT design. It relies on the dynamic
  // loader eagerly applying R_LINX_JUMP_SLOT relocations (e.g. link with
  // -z now).
  gotPltHeaderEntriesNum = 0;
  pltHeaderSize = 0;
  pltEntrySize = 20;
  ipltEntrySize = pltEntrySize;
}

RelType LinxISA::getDynRel(RelType type) const {
  switch (type) {
  case R_LINX_RELATIVE:
  case R_LINX_JUMP_SLOT:
  case R_LINX_GLOB_DAT:
  case R_LINX_COPY:
  case R_LINX_IRELATIVE:
  case R_LINX_TLS_DTPMOD64:
  case R_LINX_TLS_DTPREL64:
  case R_LINX_TLS_TPREL64:
  case R_LINX_TLSDESC:
  case R_LINX_32:
  case R_LINX_64:
    return type;
  default:
    return R_LINX_NONE;
  }
}

RelExpr LinxISA::getRelExpr(RelType type, const Symbol &s,
                            const uint8_t *loc) const {
  switch (type) {
  case R_LINX_B17_PLT:
  case R_LINX_HL_BSTART30_PCREL:
    return R_PLT_PC;
  case R_LINX_B12_PCREL:
  case R_LINX_J22_PCREL:
  case R_LINX_CBSTART12_PCREL:
  case R_LINX_B17_PCREL:
  case R_LINX_B25_PCREL:
  case R_LINX_CSETRET5_PCREL:
  case R_LINX_SETRET20_PCREL:
  case R_LINX_HL_SETRET32_PCREL:
  case R_LINX_L_BSTART42_PCREL:
  case R_LINX_PCR17_LOAD:
  case R_LINX_PCR17_STORE:
  case R_LINX_HL_PCR29_LOAD:
  case R_LINX_HL_PCR29_STORE:
  case R_LINX_32_PCREL:
    return R_PC;
  case R_LINX_GOT_HI20:
    return RE_AARCH64_GOT_PAGE_PC;
  case R_LINX_PCREL_HI20:
    return RE_AARCH64_PAGE_PC;
  case R_LINX_GOT_LO12:
    return R_GOT;
  case R_LINX_LO12:
    return RE_LINX_PC_INDIRECT;
  case R_LINX_TLS_DTPREL64:
    return R_DTPREL;
  case R_LINX_TLS_TPREL64:
    return R_TPREL;
  case R_LINX_TLSDESC:
    return R_TLSDESC;
  default:
    return R_ABS;
  }
}

bool LinxISA::usesOnlyLowPageBits(RelType type) const {
  switch (type) {
  case R_LINX_LO12:
  case R_LINX_GOT_LO12:
    return true;
  default:
    return false;
  }
}

void LinxISA::writeGotPlt(uint8_t *buf, const Symbol &s) const {
  // Non-lazy PLT bring-up: slots are resolved by the dynamic loader (via
  // R_LINX_JUMP_SLOT) before first use. Keep initial contents zero.
  if (ctx.arg.is64)
    write64le(buf, 0);
  else
    write32le(buf, 0);
}

void LinxISA::writePlt(uint8_t *buf, const Symbol &sym,
                       uint64_t pltEntryAddr) const {
  // PLT entry (5 x 32-bit):
  //   0: BSTART.STD IND
  //   4: ADDTPC  %pcrel_hi(.got.plt[sym]), ->t
  //   8: LD/LW   [t#1, off], ->u
  //  12: SETC.TGT u#1
  //  16: BSTOP
  constexpr uint32_t BSTART_IND = 0x00005001u;
  constexpr uint32_t ADDTPC_ZERO = 0x00000f87u;
  constexpr uint32_t LWI_ZERO = 0x00002019u;
  constexpr uint32_t LDI_ZERO = 0x00003019u;
  constexpr uint32_t SETC_TGT_ZERO = 0x0000403bu;
  constexpr uint32_t BSTOP = 0x00000001u;

  constexpr uint32_t REG_T1 = 24;
  constexpr uint32_t REG_U_OUT = 30;
  constexpr uint32_t REG_U1 = 28;

  write32le(buf + 0, BSTART_IND);

  const uint64_t gotPltVA = sym.getGotPltVA(ctx);
  const uint64_t addtpcAddr = pltEntryAddr + 4;
  const int64_t pageDelta =
      static_cast<int64_t>(getAArch64Page(gotPltVA)) -
      static_cast<int64_t>(getAArch64Page(addtpcAddr));
  Relocation rel{R_PC, R_LINX_PCREL_HI20, 0, 0, nullptr};
  const uint32_t addtpc = ADDTPC_ZERO | encodePcrelHi20(ctx, buf + 4, pageDelta, rel);
  write32le(buf + 4, addtpc);

  const uint64_t offInPage = gotPltVA & 0xFFFu;
  const unsigned scale = ctx.arg.is64 ? 3 : 2;
  const uint64_t mask = (1ull << scale) - 1;
  if (offInPage & mask)
    Err(ctx) << "unaligned .got.plt entry for " << sym.getName();

  const uint32_t imm12 = static_cast<uint32_t>(offInPage >> scale);
  if (imm12 >= (1u << 12))
    Err(ctx) << "out of range .got.plt offset for " << sym.getName();

  uint32_t load = ctx.arg.is64 ? LDI_ZERO : LWI_ZERO;
  load |= (imm12 << 20);          // imm12
  load |= (REG_T1 << 15);         // rs1 = t#1
  load |= (REG_U_OUT << 7);       // rd = ->u
  write32le(buf + 8, load);

  const uint32_t setc = SETC_TGT_ZERO | (REG_U1 << 15);
  write32le(buf + 12, setc);

  write32le(buf + 16, BSTOP);
}

bool LinxISA::relaxOnce(int pass) const {
  (void)pass;
  /*
   * Runtime-stability mode: keep Linx relaxation disabled.
   *
   * Current Linx relaxation shrinks instruction streams based on relocations.
   * Some intra-section BSTART/CALL encodings are resolved by the assembler and
   * carry no relocation records; shrinking code before those instructions can
   * leave stale immediates that jump to the wrong block target at runtime.
   *
   * Until non-relocated intra-section control-flow encodings are rewritten or
   * represented with relocations, disabling relaxation is safer than producing
   * silently misdirected branch/call targets.
   */
  return false;
}

void LinxISA::finalizeRelax(int passes) const {
  (void)passes;
}

void LinxISA::relocate(uint8_t *loc, const Relocation &rel,
                       uint64_t val) const {
  int64_t sval = static_cast<int64_t>(val);
  switch (rel.type) {
  case R_LINX_NONE:
    return;
  case R_LINX_32:
    checkIntUInt(ctx, loc, val, 32, rel);
    write32le(loc, val);
    return;
  case R_LINX_32_PCREL:
    checkInt(ctx, loc, sval, 32, rel);
    write32le(loc, val);
    return;
  case R_LINX_64:
    write64le(loc, val);
    return;
  case R_LINX_RELATIVE:
  case R_LINX_JUMP_SLOT:
  case R_LINX_GLOB_DAT:
  case R_LINX_IRELATIVE:
  case R_LINX_TLS_DTPMOD64:
  case R_LINX_TLS_DTPREL64:
  case R_LINX_TLS_TPREL64:
  case R_LINX_TLSDESC:
    write64le(loc, val);
    return;

  case R_LINX_B12_PCREL: {
    uint32_t cur = read32le(loc);
    cur |= encodeB12Pcrel(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_J22_PCREL: {
    uint32_t cur = read32le(loc);
    cur |= encodeJ22Pcrel(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_CBSTART12_PCREL: {
    uint16_t cur = read16le(loc);
    cur |= encodeCBStart12Pcrel(ctx, loc, sval, rel);
    write16le(loc, cur);
    return;
  }
  case R_LINX_B17_PLT:
  case R_LINX_B17_PCREL: {
    uint32_t cur = read32le(loc);
    cur |= encodeB17Pcrel(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_B25_PCREL: {
    uint32_t cur = read32le(loc);
    cur |= encodeB25Pcrel(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_HL_BSTART30_PCREL: {
    uint64_t cur = 0;
    for (unsigned i = 0; i < 6; ++i)
      cur |= static_cast<uint64_t>(loc[i]) << (i * 8);
    cur |= encodeHLBStart30Pcrel(ctx, loc, sval, rel);
    for (unsigned i = 0; i < 6; ++i)
      loc[i] = static_cast<uint8_t>((cur >> (i * 8)) & 0xFF);
    return;
  }
  case R_LINX_L_BSTART42_PCREL: {
    uint64_t cur = read64le(loc);
    cur |= encodeLBStart42Pcrel(ctx, loc, sval, rel);
    write64le(loc, cur);
    return;
  }
  case R_LINX_CSETRET5_PCREL: {
    uint16_t cur = read16le(loc);
    cur |= encodeCSetRet5Pcrel(ctx, loc, sval, rel);
    write16le(loc, cur);
    return;
  }
  case R_LINX_SETRET20_PCREL: {
    uint32_t cur = read32le(loc);
    cur |= encodeSetRet20Pcrel(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_HL_SETRET32_PCREL: {
    uint64_t cur = 0;
    for (unsigned i = 0; i < 6; ++i)
      cur |= static_cast<uint64_t>(loc[i]) << (i * 8);
    cur |= encodeHLSetRet32Pcrel(ctx, loc, sval, rel);
    for (unsigned i = 0; i < 6; ++i)
      loc[i] = static_cast<uint8_t>((cur >> (i * 8)) & 0xFF);
    return;
  }
  case R_LINX_PCREL_HI20: {
    uint32_t cur = read32le(loc);
    cur |= encodePcrelHi20(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_GOT_HI20: {
    uint32_t cur = read32le(loc);
    cur |= encodePcrelHi20(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_LO12: {
    uint32_t cur = read32le(loc);
    cur |= encodeLo12(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_GOT_LO12: {
    uint32_t cur = read32le(loc);
    cur |= encodeLo12(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_PCR17_LOAD: {
    uint32_t cur = read32le(loc);
    cur |= encodePcr17Load(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_PCR17_STORE: {
    uint32_t cur = read32le(loc);
    cur |= encodePcr17Store(ctx, loc, sval, rel);
    write32le(loc, cur);
    return;
  }
  case R_LINX_HL_PCR29_LOAD: {
    uint64_t cur = 0;
    for (unsigned i = 0; i < 6; ++i)
      cur |= static_cast<uint64_t>(loc[i]) << (i * 8);
    cur |= encodeHLPcr29Load(ctx, loc, sval, rel);
    for (unsigned i = 0; i < 6; ++i)
      loc[i] = static_cast<uint8_t>((cur >> (i * 8)) & 0xFF);
    return;
  }
  case R_LINX_HL_PCR29_STORE: {
    uint64_t cur = 0;
    for (unsigned i = 0; i < 6; ++i)
      cur |= static_cast<uint64_t>(loc[i]) << (i * 8);
    cur |= encodeHLPcr29Store(ctx, loc, sval, rel);
    for (unsigned i = 0; i < 6; ++i)
      loc[i] = static_cast<uint8_t>((cur >> (i * 8)) & 0xFF);
    return;
  }

  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unrecognized relocation " << rel.type;
    return;
  }
}

void elf::setLinxTargetInfo(Ctx &ctx) { ctx.target.reset(new LinxISA(ctx)); }
