//===-- LinxISATileOpcodesV057.h - v0.57.1 tile selectors ----*- C++ -*-===//
//
// Canonical v0.57.1 PTO TEPL Mode/Function assignments shared by the
// assembler, printer, instruction selector, and blockifier.  The packed value
// is (Mode << 5) | Function. Keep this list synchronized with the pto-spec
// release projection in isa/v0.57/state/pto_ops.json.
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
  X(TMAX, 0x00bu)                                                             \
  X(TMIN, 0x00cu)                                                             \
  X(TAND, 0x006u)                                                             \
  X(TOR, 0x007u)                                                              \
  X(TXOR, 0x008u)                                                             \
  X(TSHL, 0x009u)                                                             \
  X(TSHR, 0x00au)                                                             \
  X(TCMP, 0x00du)                                                             \
  X(TSEL, 0x01au)                                                             \
  X(TABS, 0x00fu)                                                             \
  X(TNOT, 0x010u)                                                             \
  X(TNEG, 0x011u)                                                             \
  X(TRELU, 0x017u)                                                            \
  X(TPRELU, 0x00eu)                                                           \
  X(TDIV, 0x003u)                                                             \
  X(TREM, 0x004u)                                                             \
  X(TSQRT, 0x015u)                                                            \
  X(TLOG, 0x013u)                                                             \
  X(TRECIP, 0x014u)                                                           \
  X(TEXP, 0x012u)                                                             \
  X(TRSQRT, 0x016u)                                                           \
  X(TADDS, 0x020u)                                                            \
  X(TAXPY, 0x02fu)                                                            \
  X(TSUBS, 0x021u)                                                            \
  X(TMULS, 0x022u)                                                            \
  X(TDIVS, 0x023u)                                                            \
  X(TMINS, 0x02cu)                                                            \
  X(TMAXS, 0x02bu)                                                            \
  X(TREMS, 0x024u)                                                            \
  X(TANDS, 0x026u)                                                            \
  X(TORS, 0x027u)                                                             \
  X(TXORS, 0x028u)                                                            \
  X(TCMPS, 0x02du)                                                            \
  X(TSELS, 0x03au)                                                            \
  X(TSHLS, 0x029u)                                                            \
  X(TSHRS, 0x02au)                                                            \
  X(TROWSUM, 0x040u)                                                          \
  X(TROWPROD, 0x043u)                                                         \
  X(TROWMAX, 0x041u)                                                          \
  X(TROWMIN, 0x042u)                                                          \
  X(TROWARGMAX, 0x04cu)                                                       \
  X(TROWARGMIN, 0x04du)                                                       \
  X(TCOLSUM, 0x050u)                                                          \
  X(TCOLPROD, 0x053u)                                                         \
  X(TCOLMAX, 0x051u)                                                          \
  X(TCOLMIN, 0x052u)                                                          \
  X(TCOLARGMAX, 0x05cu)                                                       \
  X(TCOLARGMIN, 0x05du)                                                       \
  X(TROWEXPAND, 0x044u)                                                       \
  X(TROWEXPANDADD, 0x045u)                                                    \
  X(TROWEXPANDSUB, 0x046u)                                                    \
  X(TROWEXPANDMUL, 0x047u)                                                    \
  X(TROWEXPANDDIV, 0x048u)                                                    \
  X(TROWEXPANDMAX, 0x049u)                                                    \
  X(TROWEXPANDMIN, 0x04au)                                                    \
  X(TROWEXPANDEXPDIF, 0x04bu)                                                 \
  X(TCOLEXPAND, 0x054u)                                                       \
  X(TCOLEXPANDADD, 0x055u)                                                    \
  X(TCOLEXPANDSUB, 0x056u)                                                    \
  X(TCOLEXPANDMUL, 0x057u)                                                    \
  X(TCOLEXPANDDIV, 0x058u)                                                    \
  X(TCOLEXPANDMAX, 0x059u)                                                    \
  X(TCOLEXPANDMIN, 0x05au)                                                    \
  X(TCOLEXPANDEXPDIF, 0x05bu)                                                 \
  X(TEXPANDS, 0x03bu)                                                         \
  X(TCI, 0x066u)                                                              \
  X(TTRI, 0x067u)                                                             \
  X(TFILLPAD, 0x065u)                                                         \
  X(TCVT, 0x01bu)                                                             \
  X(TQUANT, 0x06au)                                                           \
  X(TDEQUANT, 0x06bu)                                                         \
  X(TEXTRACT, 0x062u)                                                         \
  X(TINSERT, 0x063u)                                                          \
  X(TGATHER, 0x06fu)                                                          \
  X(TSCATTER, 0x070u)                                                         \
  X(TCONCAT, 0x060u)                                                          \
  X(TTRANS, 0x06eu)                                                           \
  X(TIMG2COL, 0x064u)                                                         \
  X(TGATHERB, 0x061u)                                                         \
  X(TDEINTERLEAVE, 0x078u)                                                    \
  X(TINTERLEAVE, 0x079u)                                                      \
  X(TRESHAPE, 0x077u)                                                         \
  X(TSORT, 0x06cu)                                                            \
  X(TMRGSORT, 0x06du)                                                         \
  X(THISTOGRAM, 0x068u)                                                       \
  X(TPUSH, 0x07au)                                                            \
  X(TPOP, 0x07bu)                                                             \
  X(TALLOC, 0x07cu)                                                           \
  X(TFREE, 0x07du)                                                            \
  X(TPARTADD, 0x071u)                                                         \
  X(TPARTMUL, 0x072u)                                                         \
  X(TPARTMAX, 0x073u)                                                         \
  X(TPARTMIN, 0x074u)                                                         \
  X(TPARTARGMAX, 0x075u)                                                      \
  X(TPARTARGMIN, 0x076u)

inline bool isCanonicalTEPLTileOpcodeV057(unsigned Opcode) {
  switch (Opcode) {
#define X(Name, Value) case Value:
    LINXISA_V057_TEPL_OPCODE_LIST(X)
#undef X
    return true;
  default:
    return false;
  }
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
