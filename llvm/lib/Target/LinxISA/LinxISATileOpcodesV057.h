//===-- LinxISATileOpcodesV057.h - v0.57 tile selectors ------*- C++ -*-===//
//
// Canonical v0.57 PTO tile selector names shared by the assembler, printer,
// instruction selector, and blockifier.  Keep this list synchronized with
// isa/v0.57/state/pto_encoding_map.json in the LinxISA superproject.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_LINXISATILEOPCODESV057_H
#define LLVM_LIB_TARGET_LINXISA_LINXISATILEOPCODESV057_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include <optional>

namespace llvm {
namespace LinxISA {

#define LINXISA_V057_TEPL_OPCODE_LIST(X)                                      \
  X(TADD, 0x000u)                                                             \
  X(TSUB, 0x001u)                                                             \
  X(TMUL, 0x002u)                                                             \
  X(TDIV, 0x003u)                                                             \
  X(TMAX, 0x004u)                                                             \
  X(TMIN, 0x005u)                                                             \
  X(TAND, 0x006u)                                                             \
  X(TOR, 0x007u)                                                              \
  X(TXOR, 0x008u)                                                             \
  X(TSHL, 0x009u)                                                             \
  X(TSHR, 0x00au)                                                             \
  X(TRELU, 0x00bu)                                                            \
  X(TPRELU, 0x00cu)                                                           \
  X(TCVT, 0x00du)                                                             \
  X(TEXP, 0x00eu)                                                             \
  X(TLOG, 0x00fu)                                                             \
  X(TSQRT, 0x010u)                                                            \
  X(TRSQRT, 0x011u)                                                           \
  X(TROWMAX, 0x012u)                                                          \
  X(TROWMIN, 0x013u)                                                          \
  X(TROWSUM, 0x014u)                                                          \
  X(TCOLMAX, 0x015u)                                                          \
  X(TCOLMIN, 0x016u)                                                          \
  X(TCOLSUM, 0x017u)                                                          \
  X(TRECIP, 0x018u)                                                           \
  X(TEXPANDS, 0x019u)                                                         \
  X(TGATHER, 0x01au)                                                          \
  X(TSCATTER, 0x01bu)                                                         \
  X(TRESHAPE, 0x01cu)                                                         \
  X(TTRANSPOSE, 0x01du)                                                       \
  X(TCOLEXPAND, 0x01eu)                                                       \
  X(TROWEXPAND, 0x01fu)                                                       \
  X(TADDS, 0x020u)                                                            \
  X(TSUBS, 0x021u)                                                            \
  X(TMULS, 0x022u)                                                            \
  X(TDIVS, 0x023u)                                                            \
  X(TMAXS, 0x024u)                                                            \
  X(TMINS, 0x025u)                                                            \
  X(TANDS, 0x026u)                                                            \
  X(TORS, 0x027u)                                                             \
  X(TXORS, 0x028u)                                                            \
  X(TSHLS, 0x029u)                                                            \
  X(TSHRS, 0x02au)                                                            \
  X(TCMP, 0x02bu)                                                             \
  X(TSEL, 0x02cu)                                                             \
  X(TABS, 0x02du)                                                             \
  X(TNOT, 0x02eu)                                                             \
  X(TNEG, 0x02fu)                                                             \
  X(TREM, 0x030u)                                                             \
  X(TAXPY, 0x031u)                                                            \
  X(TREMS, 0x032u)                                                            \
  X(TCMPS, 0x033u)                                                            \
  X(TSELS, 0x034u)                                                            \
  X(TROWPROD, 0x035u)                                                         \
  X(TROWARGMAX, 0x036u)                                                       \
  X(TROWARGMIN, 0x037u)                                                       \
  X(TCOLPROD, 0x038u)                                                         \
  X(TCOLARGMAX, 0x039u)                                                       \
  X(TCOLARGMIN, 0x03au)                                                       \
  X(TROWEXPANDADD, 0x03bu)                                                    \
  X(TROWEXPANDSUB, 0x03cu)                                                    \
  X(TROWEXPANDMUL, 0x03du)                                                    \
  X(TROWEXPANDDIV, 0x03eu)                                                    \
  X(TROWEXPANDMAX, 0x03fu)                                                    \
  X(TROWEXPANDMIN, 0x040u)                                                    \
  X(TROWEXPANDEXPDIF, 0x041u)                                                 \
  X(TCOLEXPANDADD, 0x042u)                                                    \
  X(TCOLEXPANDSUB, 0x043u)                                                    \
  X(TCOLEXPANDMUL, 0x044u)                                                    \
  X(TCOLEXPANDDIV, 0x045u)                                                    \
  X(TCOLEXPANDMAX, 0x046u)                                                    \
  X(TCOLEXPANDMIN, 0x047u)                                                    \
  X(TCOLEXPANDEXPDIF, 0x048u)                                                 \
  X(TCI, 0x080u)                                                              \
  X(TTRI, 0x081u)                                                             \
  X(TFILLPAD, 0x082u)                                                         \
  X(TQUANT, 0x083u)                                                           \
  X(TDEQUANT, 0x084u)                                                         \
  X(TEXTRACT, 0x085u)                                                         \
  X(TINSERT, 0x086u)                                                          \
  X(TCONCAT, 0x087u)                                                          \
  X(TIMG2COL, 0x088u)                                                         \
  X(TGATHERB, 0x089u)                                                         \
  X(TDEINTERLEAVE, 0x08au)                                                    \
  X(TINTERLEAVE, 0x08bu)                                                      \
  X(TSORT, 0x0c0u)                                                            \
  X(TMRGSORT, 0x0c1u)                                                         \
  X(THISTOGRAM, 0x0c2u)                                                       \
  X(TPARTADD, 0x0c3u)                                                         \
  X(TPARTMUL, 0x0c4u)                                                         \
  X(TPARTMAX, 0x0c5u)                                                         \
  X(TPARTMIN, 0x0c6u)                                                         \
  X(TPARTARGMAX, 0x0c7u)                                                      \
  X(TPARTARGMIN, 0x0c8u)                                                      \
  X(TPUSH, 0x0e0u)                                                            \
  X(TPOP, 0x0e1u)                                                             \
  X(TALLOC, 0x0e2u)                                                           \
  X(TFREE, 0x0e3u)

inline bool isCanonicalTEPLTileOpcodeV057(unsigned Opcode) {
  return Opcode <= 0x048u || (Opcode >= 0x080u && Opcode <= 0x08bu) ||
         (Opcode >= 0x0c0u && Opcode <= 0x0c8u) ||
         (Opcode >= 0x0e0u && Opcode <= 0x0e3u);
}

inline std::optional<unsigned>
parseCanonicalTEPLTileOpcodeV057(StringRef UpperName) {
  return StringSwitch<std::optional<unsigned>>(UpperName)
#define X(Name, Value) .Case(#Name, Value)
      LINXISA_V057_TEPL_OPCODE_LIST(X)
#undef X
          .Default(std::nullopt);
}

inline StringRef canonicalTEPLAliasMnemonicV057(unsigned Opcode) {
  switch (Opcode) {
#define X(Name, Value)                                                         \
  case Value:                                                                 \
    return "BSTART." #Name;
    LINXISA_V057_TEPL_OPCODE_LIST(X)
#undef X
  default:
    return StringRef();
  }
}

#undef LINXISA_V057_TEPL_OPCODE_LIST

} // namespace LinxISA
} // namespace llvm

#endif
