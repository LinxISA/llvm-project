//===------------------------- LinxV5.cpp -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "OutputSections.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"
#include "llvm/Support/TimeProfiler.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {

class LinxV5 final : public TargetInfo {
public:
  LinxV5();
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  bool relaxOnce(int pass) const override;
};

} // end anonymous namespace

// Extract bits v[begin:end], where range is inclusive, and begin must be < 63.
static uint64_t extractBits(uint64_t v, uint32_t begin, uint32_t end) {
  return (v & ((1ULL << (begin + 1)) - 1)) >> end;
}

static uint32_t setLO12_I(uint32_t insn, uint32_t imm) {
  return (insn & 0xfffff) | (imm << 20);
}
static uint32_t setLO12_S(uint32_t insn, uint32_t imm) {
  return (insn & 0x1fff07f) | (extractBits(imm, 6, 0) << 25) |
         (extractBits(imm, 11, 7) << 7);
}

// l/hl.bstart.std/fp/aux
static bool islhlHeader(uint32_t opcode) {
  return ((opcode & 0x7f) != 0b0001001 && (opcode & 0x7f) != 0b0010001);
}
// l/hl.bstart.std direct/call/cond
static bool isstdfixupHeader(uint32_t stdOpcode) {
  return ((stdOpcode & 0x1f) == 0b00000 && (stdOpcode & 0xff) != 0b00100000);
}
static bool isnotstdfixupHeader(uint32_t stdOpcode) {
  return ((stdOpcode & 0xff) != 0b01000000 &&
          (stdOpcode & 0xff) != 0b01100000 && (stdOpcode & 0xff) != 0b10000000);
}
// l/hl.bstart.std cond
static bool isstdcondHeader(uint32_t stdOpcode) {
  return ((stdOpcode & 0xff) == 0b01100000);
}
// hl.bstart.fp/aux direct/call/cond
// hl.bstart.std direct/call/cond
static bool ishlHeader(uint64_t opcode) { return ((opcode & 0xf) == 0b1110); }

LinxV5::LinxV5() {}

RelExpr LinxV5::getRelExpr(const RelType type, const Symbol &s,
                           const uint8_t *loc) const {
  const unsigned bits = config->wordsize * 8;

  switch (type) {
  case R_LinxV5_NONE:
    return R_NONE;
  case R_LinxV5_32:
  case R_LinxV5_64:
    return R_ABS;
  case R_LinxV5_ADD8:
  case R_LinxV5_ADD16:
  case R_LinxV5_ADD32:
  case R_LinxV5_ADD64:
  case R_LinxV5_SUB8:
  case R_LinxV5_SUB16:
  case R_LinxV5_SUB32:
  case R_LinxV5_SUB64:
    return R_LinxV4_ADD;
  case R_LinxV5_BNEXT:
  case R_LinxV5_BTEXT:
  case R_LinxV5_BNEXT_C:
  case R_LinxV5_C_ADDPC:
  case R_LinxV5_ADDPC:
  case R_LinxV5_HLSETRET:
  case R_LinxV5_Load_Symbol:
  case R_LinxV5_Store_Symbol:
  case R_LinxV5_Load_Symbol_Target_29:
  case R_LinxV5_Store_Symbol_Target_29:
  case R_LinxV5_Load_Symbol_Target_42:
  case R_LinxV5_Store_Symbol_Target_42:
  case R_LinxV5_32_BNEXT:
  case R_LinxV5_48_BNEXT:
  case R_LinxV5_64_BNEXT:
  case R_LinxV5_SIMT_BRANCH:
  case R_LinxV5_BRANCH:
  case R_LinxV5_BRANCH_22:
  case R_LinxV5_STACK_SIZE:
  case R_LinxV5_SIMT_BRANCH_RC:
  case R_LinxV5_SIMT_JUMP:
  case R_LinxV5_TPCREL_HI20:
  case R_LinxV5_TPCREL_HI32:
  case R_LinxV5_32_PCREL:
    return R_PC;
  case R_LinxV5_TPREL_HI20:
  case R_LinxV5_TPREL_LO12_I:
  case R_LinxV5_TPREL_LO12_L:
  case R_LinxV5_TPREL_LO12_S:
    return R_TPREL;
  case R_LinxV5_TPCREL_LO12_I:
  case R_LinxV5_TPCREL_LO12_L:
  case R_LinxV5_TPCREL_LO12_S:
  case R_LinxV5_SIMT_TPCREL_LO12_I:
  case R_LinxV5_SIMT_TPCREL_LO12_L:
  case R_LinxV5_SIMT_TPCREL_LO12_S:
    return R_LinxV4_TPC_INDIRECT;
  case R_LinxV5_ALIGN:
    return R_RELAX_HINT;
  case R_LinxV5_RELAX:
    return config->relax ? R_RELAX_HINT : R_NONE;
  default:
    error(getErrorLocation(loc) + "unknown relocation (" + Twine(type) +
          ") against symbol " + toString(s));
    return R_NONE;
  }
}

void LinxV5::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  const unsigned bits = config->wordsize * 8;

  switch (rel.type) {
  case R_LinxV5_32:
    write32(loc, val);
    return;
  case R_LinxV5_64:
    write64(loc, val);
    return;
  case R_LinxV5_ADD8:
    *loc += val;
    return;
  case R_LinxV5_ADD16:
    write16(loc, read16(loc) + val);
    return;
  case R_LinxV5_ADD32:
    write32(loc, read32(loc) + val);
    return;
  case R_LinxV5_ADD64:
    write64(loc, read64(loc) + val);
    return;
  case R_LinxV5_SUB8:
    *loc -= val;
    return;
  case R_LinxV5_SUB16:
    write16(loc, read16(loc) - val);
    return;
  case R_LinxV5_SUB32:
    write32(loc, read32(loc) - val);
    return;
  case R_LinxV5_SUB64:
    write64(loc, read64(loc) - val);
    return;
  case R_LinxV5_32_PCREL:
    write32(loc, val);
    return;
  case R_LinxV5_BTEXT:
  case R_LinxV5_BNEXT: {
    checkInt(loc, val, 26, rel);
    checkAlignment(loc, val, 2, rel);

    uint32_t insn = read32le(loc) & 0x7F;
    uint32_t imm25 = extractBits(val, 25, 1) << 7;

    insn |= imm25;

    write32le(loc, insn);
    return;
  }
  case R_LinxV5_BNEXT_C: {
    checkInt(loc, val, 13, rel);
    checkAlignment(loc, val, 2, rel);

    uint32_t insn = read16le(loc) & 0xF;
    uint32_t imm12 = extractBits(val, 12, 1) << 4;

    insn |= imm12;

    write16le(loc, insn);
    return;
  }

  case R_LinxV5_C_ADDPC: {
    checkUInt(loc, val, 6, rel);
    checkAlignment(loc, val, 2, rel);

    uint32_t insn = read16le(loc);
    uint32_t imm5 = extractBits(val, 5, 1) << 6;

    insn |= imm5;

    write16le(loc, insn);
    return;
  }

  case R_LinxV5_ADDPC: {
    checkUInt(loc, val, 21, rel);
    checkAlignment(loc, val, 2, rel);

    uint32_t insn = read32le(loc);
    uint32_t imm20 = extractBits(val, 20, 1) << 12;
    insn |= imm20;

    write32le(loc, insn);
    return;
  }

  case R_LinxV5_HLSETRET: {
    checkUInt(loc, val, 33, rel);
    checkAlignment(loc, val, 2, rel);

    uint64_t insn = read64le(loc) & 0xFFFF00000FFF000F;
    uint64_t imm32 = extractBits(val, 20, 1) << 28 | extractBits(val, 12, 21) << 4;
    insn |= imm32;

    write64le(loc, insn);
    return;
  }

  case R_LinxV5_Load_Symbol: {
    checkInt(loc, val, 17, rel);
    checkAlignment(loc, val, 1, rel);

    uint64_t insn = read32le(loc);
    uint64_t imm17 = (uint64_t)extractBits(val, 16, 0) << 15;
    insn |= imm17;
    write32le(loc, insn);
    return;
  }

  case R_LinxV5_Store_Symbol: {
    checkInt(loc, val, 12, rel);
    checkAlignment(loc, val, 1, rel);

    uint64_t insn = read32le(loc);
    uint64_t imm12 = ((uint64_t)extractBits(val, 11, 0) << 20) |
                     ((uint64_t)extractBits(val, 16, 12) << 7);
    insn |= imm12;
    write32le(loc, insn);
    return;
  }

  case R_LinxV5_BRANCH: {
    checkInt(loc, val, 13, rel);
    checkAlignment(loc, val, 2, rel);

    uint64_t insn = read32le(loc);
    uint64_t imm12 = ((uint64_t)extractBits(val, 7, 1) << 25) |
                     ((uint64_t)extractBits(val, 12, 8) << 7);
    insn |= imm12;
    write32le(loc, insn);
    return;
  }

  case R_LinxV5_BRANCH_22: {
    checkInt(loc, val, 23, rel);
    checkAlignment(loc, val, 2, rel);

    uint64_t insn = read32le(loc);
    uint64_t imm22 = ((uint64_t)extractBits(val, 17, 1) << 15) |
                     ((uint64_t)extractBits(val, 22, 18) << 7);
    insn |= imm22;
    write32le(loc, insn);
    return;
  }

  case R_LinxV5_Load_Symbol_Target_42: {
    checkInt(loc, val, 42, rel);
    checkAlignment(loc, val, 1, rel);
    uint64_t insn = read64le(loc);
    uint64_t imm42 = ((uint64_t)extractBits(val, 16, 0) << 47) |
                     ((uint64_t)extractBits(val, 41, 17) << 7);
    insn |= imm42;
    write64le(loc, insn);
    return;
  }

  case R_LinxV5_Store_Symbol_Target_42: {
    checkInt(loc, val, 42, rel);
    checkAlignment(loc, val, 1, rel);
    uint64_t insn = read64le(loc);
    uint64_t imm42 = ((uint64_t)extractBits(val, 11, 0) << 52) |
                     ((uint64_t)extractBits(val, 16, 12) << 39) |
                     ((uint64_t)extractBits(val, 41, 17) << 7);
    insn |= imm42;
    write64le(loc, insn);
    return;
  }

  case R_LinxV5_32_BNEXT: {
    checkInt(loc, val, 18, rel);
    checkAlignment(loc, val, 2, rel);

    uint32_t insn = read32le(loc);
    uint32_t imm17 = extractBits(val, 17, 1) << 15;

    insn |= imm17;

    write32le(loc, insn);
    return;
  }

  case R_LinxV5_48_BNEXT: {
    checkInt(loc, val, 30, rel);
    checkAlignment(loc, val, 2, rel);

    uint64_t insn = read64le(loc) & 0xFFFF00007FFF000F;
    uint64_t imm29 = ((uint64_t)extractBits(val, 17, 1) << 31) |
                     ((uint64_t)extractBits(val, 29, 18) << 4);

    insn |= imm29;

    write64le(loc, insn);
    return;
  }

  case R_LinxV5_Load_Symbol_Target_29: {
    checkInt(loc, val, 29, rel);
    checkAlignment(loc, val, 2, rel);
    uint64_t insn = read64le(loc) & 0xFFFF00007FFF000F;
    uint64_t imm29 = ((uint64_t)extractBits(val, 16, 0) << 31) |
                     ((uint64_t)extractBits(val, 28, 17) << 4);
    insn |= imm29;
    write64le(loc, insn);
    return;
  }

  case R_LinxV5_Store_Symbol_Target_29: {
    checkInt(loc, val, 29, rel);
    checkAlignment(loc, val, 2, rel);
    uint64_t insn = read64le(loc) & 0xFFFF000FF07F000F;
    uint64_t imm29 = ((uint64_t)extractBits(val, 11, 0) << 36) |
                     ((uint64_t)extractBits(val, 16, 12) << 23) |
                     ((uint64_t)extractBits(val, 28, 17) << 4);
    insn |= imm29;
    write64le(loc, insn);
    return;
  }

  case R_LinxV5_64_BNEXT: {
    checkInt(loc, val, 43, rel);
    checkAlignment(loc, val, 2, rel);

    uint64_t insn = read64le(loc);
    uint64_t imm42 = ((uint64_t)extractBits(val, 17, 1) << 47) |
                     ((uint64_t)extractBits(val, 42, 18) << 7);

    insn |= imm42;

    write64le(loc, insn);
    return;
  }

  case R_LinxV5_SIMT_BRANCH: {
    checkInt(loc, val, 15, rel);
    checkAlignment(loc, val, 8, rel);

    uint64_t insn = read64le(loc);
    uint64_t imm12 = ((uint64_t)extractBits(val, 9, 3) << 57) |
                     ((uint64_t)extractBits(val, 14, 10) << 39);
    insn |= imm12;
    write64le(loc, insn);
    return;
  }

  case R_LinxV5_SIMT_BRANCH_RC: {
    checkInt(loc, val, 15, rel);
    checkAlignment(loc, val, 8, rel);

    uint64_t insn = read64le(loc);
    uint64_t imm12 = ((uint64_t)extractBits(val, 9, 3) << 25) |
                     ((uint64_t)extractBits(val, 14, 10) << 7);
    insn |= imm12;
    write64le(loc, insn);
    return;
  }

  case R_LinxV5_SIMT_JUMP: {
    checkInt(loc, val, 25, rel);
    checkAlignment(loc, val, 8, rel);

    // Inst{63-52} = Imm{12-1}
    // Inst{51-47} = Imm{17-13}
    // Inst{43-39} = Imm{22-18}
    uint64_t insn = read64le(loc) & 0x707fffffffff;
    uint64_t lo12 = (uint64_t)extractBits(val, 14, 3) << 52;
    uint64_t mid5 = (uint64_t)extractBits(val, 19, 15) << 47;
    uint64_t hi5 = (uint64_t)extractBits(val, 24, 20) << 39;
    insn = insn | lo12 | mid5 | hi5;
    write64le(loc, insn);
    return;
  }

  case R_LinxV5_TPCREL_HI20:
  case R_LinxV5_TPREL_HI20: {
    uint64_t hi = val + 0x800;
    checkInt(loc, SignExtend64(hi, bits) >> 12, 20, rel);
    write32le(loc, (read32le(loc) & 0xFFF) | (hi & 0xFFFFF000));
    return;
  }

  case R_LinxV5_TPCREL_HI32: {
    uint64_t hi = val + 0x800;
    checkInt(loc, SignExtend64(hi, bits) >> 12, 32, rel);
    write64le(loc, (read64le(loc) & 0xFFFF00000FFF000F) | (hi & 0xFFFFFFFF000));
    return;
  }

  case R_LinxV5_TPCREL_LO12_I:
  case R_LinxV5_TPREL_LO12_I: {
    uint64_t hi = (val + 0x800) >> 12;
    uint64_t lo = val - (hi << 12);
    uint32_t insn = read32le(loc);
    const uint32_t addiMask = 0b111000001111111;
    const uint32_t addiOpcode = 0b000000000010101;
    const uint32_t subiOpcode = 0b001000000010101;
    if ((insn & addiMask) == addiOpcode && (val & 0x800)) {
      insn |= subiOpcode;
      lo = 0 - lo;
    }
    write32le(loc, setLO12_I(insn, lo & 0xfff));
    return;
  }

  case R_LinxV5_TPCREL_LO12_L:
  case R_LinxV5_TPREL_LO12_L: {
    uint64_t hi = (val + 0x800) >> 12;
    uint64_t lo = val - (hi << 12);
    write32le(loc, setLO12_I(read32le(loc), lo & 0xfff));
    return;
  }

  case R_LinxV5_TPCREL_LO12_S:
  case R_LinxV5_TPREL_LO12_S: {
    uint64_t hi = (val + 0x800) >> 12;
    uint64_t lo = val - (hi << 12);
    write32le(loc, setLO12_S(read32le(loc), lo));
    return;
  }

  case R_LinxV5_SIMT_TPCREL_LO12_I: {
    uint64_t hi = (val + 0x800) >> 12;
    uint64_t lo = val - (hi << 12);
    uint64_t insn = read64le(loc) & 0xfffffffffffff;
    const uint64_t FuncMask = 0b111000000000000ul << 32;
    const uint64_t addiFunc = 0b000000000000000ul << 32;
    const uint64_t subiFunc = 0b001000000000000ul << 32;
    if ((insn & FuncMask) == addiFunc && (val & 0x800)) {
      insn |= subiFunc;
      lo = 0 - lo;
    }

    write64le(loc, insn | (lo << 52));
    return;
  }

  case R_LinxV5_SIMT_TPCREL_LO12_L: {
    uint64_t hi = (val + 0x800) >> 12;
    uint64_t lo = val - (hi << 12);
    uint64_t insn = read64le(loc) & 0xfffffffffffff;
    write64le(loc, insn | (lo << 52));
    return;
  }

  case R_LinxV5_SIMT_TPCREL_LO12_S: {
    uint64_t hi = (val + 0x800) >> 12;
    uint64_t lo = val - (hi << 12);
    uint64_t insn = read64le(loc) & 0x1fff07fffffffff;

    // Inst{63-57} = Imm{6-0}
    // Inst{43-39} = Imm{11-7}
    uint64_t hi5 = (uint64_t)extractBits(lo, 11, 7) << 39;
    uint64_t lo7 = (uint64_t)extractBits(lo, 6, 0) << 57;
    insn = insn | hi5 | lo7;
    write64le(loc, insn);
    return;
  }

  case R_LinxV5_STACK_SIZE: {
    const Symbol &sym = *rel.sym;
    const Defined *defSym = dyn_cast<Defined>(&sym);
    auto *inSec = dyn_cast<InputSection>(defSym->section);
    const uint64_t size_offset = sym.getVA() - inSec->getVA();
    const uint32_t size = read32le(inSec->rawData.data() + size_offset);
    uint32_t val = 0;
    uint32_t insn = 0;
      if (size == 0)
        val = 0;
      else if (size <= 32)
        val = 1;
      else if (size <= 64)
        val = 2;
      else if (size <= 128)
        val = 3;
      else if (size <= 256)
        val = 4;
      else if (size <= 512)
        val = 5;
      else if (size <= 1024 * 1)
        val = 6;
      else if (size <= 1024 * 2)
        val = 7;
      else if (size <= 1024 * 4)
        val = 8;
      else if (size <= 1024 * 8)
        val = 9;
      else if (size <= 1024 * 16)
        val = 10;
      else if (size <= 1024 * 32)
        val = 11;
      else if (size <= 1024 * 64)
        val = 12;
      else if (size <= 1024 * 128)
        val = 13;
      else if (size <= 1024 * 256)
        val = 14;
      else if (size <= 1024 * 512)
        val = 15;
      insn = read32le(loc) & 0xfff87fff | ((val & 0xf) << 15);
    write32le(loc, insn);
    return;
  }

  case R_LinxV5_STACK_SIZE_NONE:
  case R_LinxV5_RELAX:
    return; // Ignored (for now)

  default:
    llvm_unreachable("unknown relocation");
  }
}

namespace {
struct SymbolAnchor {
  uint64_t offset;
  Defined *d;
  bool end; // true for the anchor of st_value+st_size
};
} // namespace

struct elf::LinxV5RelaxAux {
  // This records symbol start and end offsets which will be adjusted according
  // to the nearest relocDeltas element.
  SmallVector<SymbolAnchor, 0> anchors;
  // For relocations[i], the actual offset is r_offset - (i ? relocDeltas[i-1] :
  // 0).
  std::unique_ptr<uint32_t[]> relocDeltas;
  // relocLastDeltas is the relocDeltas of last pass.
  std::unique_ptr<uint32_t[]> relocLastDeltas;
  // For relocations[i], the actual type is relocTypes[i].
  std::unique_ptr<RelType[]> relocTypes;
  SmallVector<uint64_t, 0> writes;
};

static void initSymbolAnchors() {
  SmallVector<InputSection *, 0> storage;
  for (OutputSection *osec : outputSections) {
    if (!(osec->flags & SHF_EXECINSTR))
      continue;
    for (InputSection *sec : getInputSections(*osec, storage)) {
      sec->relaxAuxLinxV5 = make<LinxV5RelaxAux>();
      if (sec->relocations.size()) {
        sec->relaxAuxLinxV5->relocDeltas =
            std::make_unique<uint32_t[]>(sec->relocations.size());
        sec->relaxAuxLinxV5->relocLastDeltas =
            std::make_unique<uint32_t[]>(sec->relocations.size());
        sec->relaxAuxLinxV5->relocTypes =
            std::make_unique<RelType[]>(sec->relocations.size());
      }
    }
  }
  // Store anchors (st_value and st_value+st_size) for symbols relative to text
  // sections.
  for (InputFile *file : ctx->objectFiles)
    for (Symbol *sym : file->getSymbols()) {
      auto *d = dyn_cast<Defined>(sym);
      if (!d || d->file != file)
        continue;
      if (auto *sec = dyn_cast_or_null<InputSection>(d->section))
        if ((sec->flags & SHF_EXECINSTR) && sec->relaxAuxLinxV5) {
          // If sec is discarded, relaxAuxLinxV5 will be nullptr.
          sec->relaxAuxLinxV5->anchors.push_back({d->value, d, false});
          sec->relaxAuxLinxV5->anchors.push_back({d->value + d->size, d, true});
        }
    }
  // Sort anchors by offset so that we can find the closest relocation
  // efficiently. For a zero size symbol, ensure that its start anchor precedes
  // its end anchor. For two symbols with anchors at the same offset, their
  // order does not matter.
  for (OutputSection *osec : outputSections) {
    if (!(osec->flags & SHF_EXECINSTR))
      continue;
    for (InputSection *sec : getInputSections(*osec, storage)) {
      llvm::sort(sec->relaxAuxLinxV5->anchors, [](auto &a, auto &b) {
        return std::make_pair(a.offset, a.end) <
               std::make_pair(b.offset, b.end);
      });
    }
  }
}

// Relax R_LinxV5_BNEXT bstart.std to c.bstart.std.
static void relaxstdHeader(const InputSection &sec, size_t i, uint64_t loc,
                           Relocation &r, uint32_t &remove) {
  const Symbol &sym = *r.sym;
  const uint64_t insnPair = read32le(sec.rawData.data() + r.offset);
  const uint32_t opcode = extractBits(insnPair, 6, 0);
  uint32_t cOpcode = 0;
  const uint64_t dest = sym.getVA() + r.addend;
  const int64_t displace = dest - loc;

  if (isInt<13>(displace)) {
    sec.relaxAuxLinxV5->relocTypes[i] = R_LinxV5_BNEXT_C;
    if ((opcode & 0x7f) == 0b0010001) // c.bstart.std direct/call
      cOpcode = 0b0010;
    else // c.bstart.std cond
      cOpcode = 0b0100;
    sec.relaxAuxLinxV5->writes.push_back(cOpcode);
    remove = 2;
  }
}

// Relax R_LinxV5_64/48_BNEXT
// l.bstart.std/aux/fp to hl.bsatrt.std/aux/fp or bstart.std/aux/fp.
// 2.hl.bsatrt.std/aux/fp to bstart.std/aux/fp.
static void relaxHeader(const InputSection &sec, size_t i, uint64_t loc,
                        Relocation &r, uint32_t &remove) {
  const Symbol &sym = *r.sym;
  const uint64_t insnPair = read64le(sec.rawData.data() + r.offset);
  uint64_t opcode = extractBits(insnPair, 46, 0);
  if (islhlHeader(opcode)) {
    const uint64_t dest = sym.getVA() + r.addend;
    const int64_t displace = dest - loc;

    uint64_t realOpcode = opcode;
    if ((opcode & 0xf) == 0b1110)
      realOpcode = opcode >> 16;
    else if ((opcode & 0xf) == 0b1111)
      realOpcode = opcode >> 32;
    uint64_t stdOpcode = extractBits(realOpcode, 14, 7);
    // l/hl.bstart.std direct/call/cond ->c.bstart
    if (isInt<13>(displace) && isstdfixupHeader(stdOpcode)) {
      sec.relaxAuxLinxV5->relocTypes[i] = R_LinxV5_BNEXT_C;
      if (isstdcondHeader(stdOpcode))
        stdOpcode = 0b0100;
      // l/hl.bstart.std direct/call
      else
        stdOpcode = 0b0010;
      sec.relaxAuxLinxV5->writes.push_back(stdOpcode);
      remove = 6;
      if (ishlHeader(opcode))
        remove = 4;
    }
    // l/hl.bstart.aux/fp direct/call/cond->bstart.aux/fp
    // l/hl.bstart.std fall->bstart.std
    else if (isInt<18>(displace) && isnotstdfixupHeader(stdOpcode)) {
      sec.relaxAuxLinxV5->relocTypes[i] = R_LinxV5_32_BNEXT;

      realOpcode &= 0x7fff;
      sec.relaxAuxLinxV5->writes.push_back(realOpcode);
      remove = 4;
      if (ishlHeader(opcode))
        remove = 2;
    }
    // l/hl.bstart.std direct/call/cond -> bstart
    // EX: todo Branch Prediction Switch
    else if (isInt<26>(displace) && isstdfixupHeader(stdOpcode)) {
      sec.relaxAuxLinxV5->relocTypes[i] = R_LinxV5_BNEXT;
      if (isstdcondHeader(stdOpcode))
        stdOpcode = 0b0100001;
      // l/hl.bstart.std direct/call
      else
        stdOpcode = 0b0010001;
      sec.relaxAuxLinxV5->writes.push_back(stdOpcode);
      remove = 4;
      if (ishlHeader(opcode))
        remove = 2;
    }
    // l.bstart.std/fp/aux direct/call/cond
    else if (isInt<30>(displace) && !ishlHeader(opcode)) {
      sec.relaxAuxLinxV5->relocTypes[i] = R_LinxV5_48_BNEXT;
      realOpcode = (realOpcode & 0xffffffff) << 16;
      realOpcode |= 0x00000000e;
      sec.relaxAuxLinxV5->writes.push_back(realOpcode);
      remove = 2;
    }
  } else {
    relaxstdHeader(sec, i, loc, r, remove);
  }
}

// Relax R_LinxV5_ADDPC addpc to c.addpc.
static void relaxAddpc(const InputSection &sec, size_t i, uint64_t loc,
                       Relocation &r, uint32_t &remove, uint64_t delta) {
  const Symbol &sym = *r.sym;
  const uint64_t insnPair = read32le(sec.rawData.data() + r.offset);
  uint32_t cOpcode = 0;
  const uint64_t dest = sym.getVA() + r.addend;
  // The addpc instruction address needs to be modified to the address of last
  // pass because the symbol address is the address of last pass. If the symbol
  // address is not modified, the `displace` will be too large and distorted. As
  // a result, many extra pass are required.
  const int64_t displace =
      dest - (loc + delta - sec.relaxAuxLinxV5->relocLastDeltas[i - 1]);

  if (isUInt<6>(displace)) {
    sec.relaxAuxLinxV5->relocTypes[i] = R_LinxV5_C_ADDPC;
    cOpcode = 0x5016;
    sec.relaxAuxLinxV5->writes.push_back(cOpcode);
    remove = 2;
  }
}

static void relaxStackSize(const InputSection &sec, size_t i, uint64_t loc,
                           Relocation &r, uint32_t &remove, uint64_t delta) {
  const Symbol &sym = *r.sym;
  const Defined *defSym = dyn_cast<Defined>(&sym);
  auto *inSec = dyn_cast<InputSection>(defSym->section);
  const uint64_t size_offset = sym.getVA() - inSec->getVA();
  const uint32_t size = read32le(inSec->rawData.data() + size_offset);
  uint32_t val = 0;
  if (size == 0) {
    sec.relaxAuxLinxV5->relocTypes[i] = R_LinxV5_STACK_SIZE_NONE;
    remove = 4;
  }
}

static bool relax(InputSection &sec) {
  const uint64_t secAddr = sec.getVA();
  auto &aux = *sec.relaxAuxLinxV5;
  bool changed = false;

  // Get st_value delta for symbols relative to this section from the previous
  // iteration.
  ArrayRef<SymbolAnchor> sa = makeArrayRef(aux.anchors);
  uint64_t delta = 0;

  std::fill_n(aux.relocTypes.get(), sec.relocations.size(), R_LinxV5_NONE);
  aux.writes.clear();
  for (auto it : llvm::enumerate(sec.relocations)) {
    Relocation &r = it.value();
    const size_t i = it.index();
    const uint64_t loc = secAddr + r.offset - delta;
    uint32_t &cur = aux.relocDeltas[i], remove = 0;
    aux.relocLastDeltas[i] = aux.relocDeltas[i];
    switch (r.type) {
    case R_LinxV5_ALIGN: {
      const uint64_t nextLoc = loc + r.addend;
      const uint64_t align = PowerOf2Ceil(r.addend + 2);
      // All bytes beyond the alignment boundary should be removed.
      remove = nextLoc - ((loc + align - 1) & -align);
      assert(static_cast<int32_t>(remove) >= 0 &&
             "R_LinxV5_ALIGN needs expanding the content");
      break;
    }
    case R_LinxV5_BNEXT:
      if (i + 1 != sec.relocations.size() &&
          sec.relocations[i + 1].type == R_LinxV5_RELAX)
        relaxHeader(sec, i, loc, r, remove);
      break;
    case R_LinxV5_48_BNEXT:
      if (i + 1 != sec.relocations.size() &&
          sec.relocations[i + 1].type == R_LinxV5_RELAX)
        relaxHeader(sec, i, loc, r, remove);
      break;
    case R_LinxV5_64_BNEXT:
      if (i + 1 != sec.relocations.size() &&
          sec.relocations[i + 1].type == R_LinxV5_RELAX)
        relaxHeader(sec, i, loc, r, remove);
      break;
    case R_LinxV5_ADDPC:
      if (i + 1 != sec.relocations.size() &&
          sec.relocations[i + 1].type == R_LinxV5_RELAX)
        relaxAddpc(sec, i, loc, r, remove, delta);
      break;
    case R_LinxV5_STACK_SIZE:
      if (i + 1 != sec.relocations.size() &&
          sec.relocations[i + 1].type == R_LinxV5_RELAX)
        relaxStackSize(sec, i, loc, r, remove, delta);
      break;
    }

    // For all anchors whose offsets are <= r.offset, they are preceded by
    // the previous relocation whose `relocDeltas` value equals `delta`.
    // Decrease their st_value and update their st_size.
    for (; sa.size() && sa[0].offset <= r.offset; sa = sa.slice(1)) {
      if (sa[0].end)
        sa[0].d->size = sa[0].offset - delta - sa[0].d->value;
      else
        sa[0].d->value = sa[0].offset - delta;
    }
    delta += remove;
    if (delta != cur) {
      cur = delta;
      changed = true;
    }
  }

  for (const SymbolAnchor &a : sa) {
    if (a.end)
      a.d->size = a.offset - delta - a.d->value;
    else
      a.d->value = a.offset - delta;
  }
  // Inform assignAddresses that the size has changed.
  if (!isUInt<32>(delta))
    fatal("section size decrease is too large");
  sec.bytesDropped = delta;
  return changed;
}

// When relaxing just R_LinxV5_ALIGN, relocDeltas is usually changed only once
// in the absence of a linker script. For call and load/store R_LinxV5_RELAX,
// code shrinkage may reduce displacement and make more relocations eligible for
// relaxation. Code shrinkage may increase displacement to a call/load/store
// target at a higher fixed address, invalidating an earlier relaxation. Any
// change in section sizes can have cascading effect and require another
// relaxation pass.
bool LinxV5::relaxOnce(int pass) const {
  llvm::TimeTraceScope timeScope("LinxV5 relaxOnce");
  if (config->relocatable)
    return false;

  if (pass == 0)
    initSymbolAnchors();

  SmallVector<InputSection *, 0> storage;
  bool changed = false;
  for (OutputSection *osec : outputSections) {
    if (!(osec->flags & SHF_EXECINSTR))
      continue;
    for (InputSection *sec : getInputSections(*osec, storage))
      changed |= relax(*sec);
  }
  return changed;
}

void elf::linxv5FinalizeRelax(int passes) {
  llvm::TimeTraceScope timeScope("Finalize LinxV5 relaxation");
  log("relaxation passes: " + Twine(passes));
  SmallVector<InputSection *, 0> storage;
  for (OutputSection *osec : outputSections) {
    if (!(osec->flags & SHF_EXECINSTR))
      continue;
    for (InputSection *sec : getInputSections(*osec, storage)) {
      LinxV5RelaxAux &aux = *sec->relaxAuxLinxV5;
      if (!aux.relocDeltas)
        continue;

      auto &rels = sec->relocations;
      ArrayRef<uint8_t> old = sec->rawData;
      size_t newSize =
          old.size() - aux.relocDeltas[sec->relocations.size() - 1];
      size_t writesIdx = 0;
      uint8_t *p = context().bAlloc.Allocate<uint8_t>(newSize);
      uint64_t offset = 0;
      int64_t delta = 0;
      sec->rawData = makeArrayRef(p, newSize);
      sec->bytesDropped = 0;

      // Update section content: remove NOPs for R_LinxV5_ALIGN and rewrite
      // instructions for relaxed relocations.
      for (size_t i = 0, e = rels.size(); i != e; ++i) {
        uint32_t remove = aux.relocDeltas[i] - delta;
        delta = aux.relocDeltas[i];
        if (remove == 0 && aux.relocTypes[i] == R_LinxV5_NONE)
          continue;

        // Copy from last location to the current relocated location.
        const Relocation &r = rels[i];
        uint64_t size = r.offset - offset;
        memcpy(p, old.data() + offset, size);
        p += size;

        // For R_LinxV5_ALIGN, we will place `offset` in a location (among NOPs)
        // to satisfy the alignment requirement. If both `remove` and r.addend
        // are multiples of 4, it is as if we have skipped some NOPs. Otherwise
        // we are in the middle of a 4-byte NOP, and we need to rewrite the NOP
        // sequence.
        int64_t skip = 0;
        if (r.type == R_LinxV5_ALIGN) {
          if (remove % 4 || r.addend % 4) {
            skip = r.addend - remove;
            int64_t j = 0;
            for (; j + 4 <= skip; j += 4)
              write32le(p + j, 0x0000300b); // nop
            if (j != skip) {
              assert(j + 2 == skip);
              write16le(p + j, 0xc002); // c.nop
            }
          }
        } else if (RelType newType = aux.relocTypes[i]) {
          switch (newType) {
          case R_LinxV5_RELAX:
            // Used by relaxTlsLe to indicate the relocation is ignored.
            break;
          case R_LinxV5_BNEXT_C:
            skip = 2;
            write16le(p, aux.writes[writesIdx++]);
            break;
          case R_LinxV5_STACK_SIZE_NONE:
            skip = 0;
            break;
          case R_LinxV5_BNEXT:
            skip = 4;
            write32le(p, aux.writes[writesIdx++]);
            break;
          case R_LinxV5_32_BNEXT:
            skip = 4;
            write32le(p, aux.writes[writesIdx++]);
            break;
          case R_LinxV5_48_BNEXT:
            skip = 6;
            write64le(p, aux.writes[writesIdx++]);
            break;
          case R_LinxV5_C_ADDPC:
            skip = 2;
            write16le(p, aux.writes[writesIdx++]);
            break;
          default:
            llvm_unreachable("unsupported type");
          }
        }

        p += skip;
        offset = r.offset + skip + remove;
      }
      memcpy(p, old.data() + offset, old.size() - offset);

      // Subtract the previous relocDeltas value from the relocation offset.
      // For a pair of R_LinxV5_BNEXT/R_LinxV5_RELAX with the same offset,
      // decrease their r_offset by the same delta.
      delta = 0;
      for (size_t i = 0, e = rels.size(); i != e;) {
        uint64_t cur = rels[i].offset;
        do {
          rels[i].offset -= delta;
          if (aux.relocTypes[i] != R_LinxV5_NONE)
            rels[i].type = aux.relocTypes[i];
          ++i;
        } while (i != e && rels[i].offset == cur);
        delta = aux.relocDeltas[i - 1];
      }
    }
  }
}

TargetInfo *elf::getLinxV5TargetInfo() {
  static LinxV5 target;
  return &target;
}
