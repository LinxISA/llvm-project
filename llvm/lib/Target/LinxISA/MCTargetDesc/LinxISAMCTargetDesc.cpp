#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "MCTargetDesc/LinxISAInstPrinter.h"
#include "MCTargetDesc/LinxISAMCAsmInfo.h"
#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "TargetInfo/LinxISATargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/TargetParser/Triple.h"
#include <string>

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "LinxISAGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "LinxISAGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "LinxISAGenRegisterInfo.inc"

using namespace llvm;

namespace {

constexpr StringLiteral PTOISAIdentity =
    R"({"encoding_abi":"pto-isa-0.58.0-mode-function-v1","encoding_projection_sha256":"0cad2272ada8f53fc8354e22568099fe8d6bd4b7832c837260cd370b0fc76ffa","release":"0.58.0"})";

class LinxISAObjectTargetStreamer final : public MCTargetStreamer {
public:
  explicit LinxISAObjectTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

  void finish() override {
    MCStreamer &Out = getStreamer();
    MCContext &Ctx = Out.getContext();
    MCSectionELF *Note =
        Ctx.getELFSection(".note.pto.isa", ELF::SHT_NOTE, ELF::SHF_ALLOC);
    if (Note->isRegistered()) {
      Ctx.reportError(
          SMLoc(), "LinxISA: input assembly must not define .note.pto.isa; the "
                   "assembler emits the canonical PTO ISA identity");
      return;
    }

    MCSection *Previous = Out.getCurrentSectionOnly();
    Out.switchSection(Note);
    Out.emitValueToAlignment(Align(4));
    Out.emitIntValue(4, 4); // namesz: PTO\0
    Out.emitIntValue(PTOISAIdentity.size(), 4);
    Out.emitIntValue(1, 4); // PTO_NT_ISA_IDENTITY
    Out.emitBytes(StringRef("PTO", 4));
    Out.emitBytes(PTOISAIdentity);
    Out.emitValueToAlignment(Align(4));
    Out.endSection(Note);
    Out.switchSection(Previous);
  }
};

static MCTargetStreamer *
createLinxISAObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  if (!STI.getTargetTriple().isOSBinFormatELF())
    return nullptr;
  return new LinxISAObjectTargetStreamer(S);
}

} // namespace

static MCInstrInfo *createLinxISAMCInstrInfo() {
  // Build a minimal MCInstrInfo that can safely name all spec-defined opcodes.
  // Operand and implicit-reg metadata is intentionally left empty for bring-up.
  static const MCInstrDesc *Descs = nullptr;
  static const unsigned *NameIdx = nullptr;
  static const char *NameData = nullptr;
  static const uint8_t *Deprecated = nullptr;

  if (!Descs) {
    const unsigned N = static_cast<unsigned>(linxisa_inst_forms_count);
    auto *D = new MCInstrDesc[N]();
    auto *NI = new unsigned[N]();
    auto *DF = new uint8_t[N]();
    auto *Pool = new std::string();
    Pool->reserve(N * 24);

    for (unsigned Opcode = 0; Opcode < N; ++Opcode) {
      const linxisa_inst_form &F = linxisa_inst_forms[Opcode];
      NI[Opcode] = static_cast<unsigned>(Pool->size());
      if (F.id && F.id[0])
        Pool->append(F.id);
      else if (F.mnemonic && F.mnemonic[0])
        Pool->append(F.mnemonic);
      else
        Pool->append("linxisa.invalid");
      Pool->push_back('\0');

      DF[Opcode] = uint8_t(-1U);

      MCInstrDesc Desc{};
      Desc.Opcode = static_cast<unsigned short>(Opcode);
      Desc.NumOperands = 0;
      Desc.NumDefs = 0;
      Desc.Size = static_cast<unsigned char>(F.length_bits / 8);
      Desc.SchedClass = 0;
      Desc.NumImplicitUses = 0;
      Desc.NumImplicitDefs = 0;
      Desc.OpInfoOffset = 0;
      Desc.ImplicitOffset = 0;
      Desc.Flags = 0;
      Desc.TSFlags = 0;

      // MCInstrInfo indexes from the end: get(Opcode) returns
      // *(LastDesc-Opcode).
      D[N - 1 - Opcode] = Desc;
    }

    Descs = D;
    NameIdx = NI;
    NameData = Pool->c_str();
    Deprecated = DF;
  }

  const unsigned N = static_cast<unsigned>(linxisa_inst_forms_count);
  MCInstrInfo *Info = new MCInstrInfo();
  Info->InitMCInstrInfo(Descs, NameIdx, NameData, Deprecated, /*CDI=*/nullptr,
                        N);
  return Info;
}

static MCRegisterInfo *createLinxISAMCRegisterInfo(const Triple & /*TT*/) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitLinxISAMCRegisterInfo(X, /*RA=*/LinxISA::R10);
  return X;
}

static MCSubtargetInfo *
createLinxISAMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  return createLinxISAMCSubtargetInfoImpl(TT, CPU, /*TuneCPU=*/CPU, FS);
}

static MCAsmInfo *createLinx32MCAsmInfo(const MCRegisterInfo & /*MRI*/,
                                        const Triple &TT,
                                        const MCTargetOptions & /*Options*/) {
  return new LinxISAMCAsmInfo(TT, /*Is64Bit=*/false);
}

static MCAsmInfo *createLinx64MCAsmInfo(const MCRegisterInfo & /*MRI*/,
                                        const Triple &TT,
                                        const MCTargetOptions & /*Options*/) {
  return new LinxISAMCAsmInfo(TT, /*Is64Bit=*/true);
}

static MCInstPrinter *createLinxISAMCInstPrinter(const Triple & /*T*/,
                                                 unsigned /*SyntaxVariant*/,
                                                 const MCAsmInfo &MAI,
                                                 const MCInstrInfo &MII,
                                                 const MCRegisterInfo &MRI) {
  return new LinxISAInstPrinter(MAI, MII, MRI);
}

static MCStreamer *createMCStreamer(const Triple &T, MCContext &Context,
                                    std::unique_ptr<MCAsmBackend> &&MAB,
                                    std::unique_ptr<MCObjectWriter> &&OW,
                                    std::unique_ptr<MCCodeEmitter> &&Emitter) {
  if (!T.isOSBinFormatELF())
    report_fatal_error("Linx: only ELF is supported");
  return createELFStreamer(Context, std::move(MAB), std::move(OW),
                           std::move(Emitter));
}

static MCRelocationInfo *createLinxISAMCRelocationInfo(const Triple &TT,
                                                       MCContext &Ctx) {
  return llvm::createMCRelocationInfo(TT, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLinxISATargetMC() {
  RegisterMCAsmInfoFn X32(getTheLinx32Target(), createLinx32MCAsmInfo);
  RegisterMCAsmInfoFn X64(getTheLinx64Target(), createLinx64MCAsmInfo);

  for (Target *T : {&getTheLinx32Target(), &getTheLinx64Target()}) {
    TargetRegistry::RegisterMCInstrInfo(*T, createLinxISAMCInstrInfo);
    TargetRegistry::RegisterMCRegInfo(*T, createLinxISAMCRegisterInfo);
    TargetRegistry::RegisterMCSubtargetInfo(*T, createLinxISAMCSubtargetInfo);
    TargetRegistry::RegisterMCCodeEmitter(*T, createLinxISAMCCodeEmitter);
    TargetRegistry::RegisterMCAsmBackend(*T, createLinxISAAsmBackend);
    TargetRegistry::RegisterMCInstPrinter(*T, createLinxISAMCInstPrinter);
    TargetRegistry::RegisterMCRelocationInfo(*T, createLinxISAMCRelocationInfo);
    TargetRegistry::RegisterELFStreamer(*T, createMCStreamer);
    TargetRegistry::RegisterObjectTargetStreamer(
        *T, createLinxISAObjectTargetStreamer);
  }
}
