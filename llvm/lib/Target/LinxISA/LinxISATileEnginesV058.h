//===-- LinxISATileEnginesV058.h - PTO 0.58 tile engines -*- C++ -*-===//
//
// Canonical PTO 0.58 tile-operation Mode/Function assignments and their
// architectural execution-engine classifications. The packed selector remains
// (Mode << 5) | Function; VEC and SFU are assembly aliases for that unchanged
// TEPL encoding carrier. TLSU and CUBE complete the architectural engine set.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_LINXISATILEENGINESV058_H
#define LLVM_LIB_TARGET_LINXISA_LINXISATILEENGINESV058_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
#include <optional>

namespace llvm {
namespace LinxISA {

enum class TileEngineV058 { VEC, SFU, TLSU, CUBE };

#define LINXISA_V058_TILE_OPERATION_LIST(X)                                    \
  X(TADD, 0x000u, VEC)                                                         \
  X(TSUB, 0x001u, VEC)                                                         \
  X(TMUL, 0x002u, VEC)                                                         \
  X(TDIV, 0x003u, SFU)                                                         \
  X(TREM, 0x004u, SFU)                                                         \
  X(TAND, 0x006u, VEC)                                                         \
  X(TOR, 0x007u, VEC)                                                          \
  X(TXOR, 0x008u, VEC)                                                         \
  X(TSHL, 0x009u, VEC)                                                         \
  X(TSHR, 0x00au, VEC)                                                         \
  X(TMAX, 0x00bu, VEC)                                                         \
  X(TMIN, 0x00cu, VEC)                                                         \
  X(TCMP, 0x00du, VEC)                                                         \
  X(TABS, 0x00fu, VEC)                                                         \
  X(TNOT, 0x010u, VEC)                                                         \
  X(TNEG, 0x011u, VEC)                                                         \
  X(TEXP, 0x012u, SFU)                                                         \
  X(TLOG, 0x013u, SFU)                                                         \
  X(TRECIP, 0x014u, SFU)                                                       \
  X(TSQRT, 0x015u, SFU)                                                        \
  X(TRSQRT, 0x016u, SFU)                                                       \
  X(TRELU, 0x017u, VEC)                                                        \
  X(TSEL, 0x01au, VEC)                                                         \
  X(TCVT, 0x01bu, VEC)                                                         \
  X(TFMA, 0x01cu, VEC)                                                         \
  X(TADDS, 0x020u, VEC)                                                        \
  X(TSUBS, 0x021u, VEC)                                                        \
  X(TMULS, 0x022u, VEC)                                                        \
  X(TDIVS, 0x023u, SFU)                                                        \
  X(TREMS, 0x024u, SFU)                                                        \
  X(TANDS, 0x026u, VEC)                                                        \
  X(TORS, 0x027u, VEC)                                                         \
  X(TXORS, 0x028u, VEC)                                                        \
  X(TSHLS, 0x029u, VEC)                                                        \
  X(TSHRS, 0x02au, VEC)                                                        \
  X(TMAXS, 0x02bu, VEC)                                                        \
  X(TMINS, 0x02cu, VEC)                                                        \
  X(TCMPS, 0x02du, VEC)                                                        \
  X(TSELS, 0x03au, VEC)                                                        \
  X(TEXPANDS, 0x03bu, VEC)                                                     \
  X(TROWSUM, 0x040u, SFU)                                                      \
  X(TROWMAX, 0x041u, SFU)                                                      \
  X(TROWMIN, 0x042u, SFU)                                                      \
  X(TROWPROD, 0x043u, SFU)                                                     \
  X(TROWEXPAND, 0x044u, SFU)                                                   \
  X(TROWEXPANDADD, 0x045u, SFU)                                                \
  X(TROWEXPANDSUB, 0x046u, SFU)                                                \
  X(TROWEXPANDMUL, 0x047u, SFU)                                                \
  X(TROWEXPANDDIV, 0x048u, SFU)                                                \
  X(TROWEXPANDMAX, 0x049u, SFU)                                                \
  X(TROWEXPANDMIN, 0x04au, SFU)                                                \
  X(TROWEXPANDEXPDIF, 0x04bu, SFU)                                             \
  X(TROWARGMAX, 0x04cu, SFU)                                                   \
  X(TROWARGMIN, 0x04du, SFU)                                                   \
  X(TCOLSUM, 0x050u, SFU)                                                      \
  X(TCOLMAX, 0x051u, SFU)                                                      \
  X(TCOLMIN, 0x052u, SFU)                                                      \
  X(TCOLPROD, 0x053u, SFU)                                                     \
  X(TCOLEXPAND, 0x054u, SFU)                                                   \
  X(TCOLEXPANDADD, 0x055u, SFU)                                                \
  X(TCOLEXPANDSUB, 0x056u, SFU)                                                \
  X(TCOLEXPANDMUL, 0x057u, SFU)                                                \
  X(TCOLEXPANDDIV, 0x058u, SFU)                                                \
  X(TCOLEXPANDMAX, 0x059u, SFU)                                                \
  X(TCOLEXPANDMIN, 0x05au, SFU)                                                \
  X(TCOLEXPANDEXPDIF, 0x05bu, SFU)                                             \
  X(TCOLARGMAX, 0x05cu, SFU)                                                   \
  X(TCOLARGMIN, 0x05du, SFU)                                                   \
  X(TCONCAT, 0x060u, SFU)                                                      \
  X(TEXTRACT, 0x062u, SFU)                                                     \
  X(TINSERT, 0x063u, SFU)                                                      \
  X(TIMG2COL, 0x064u, SFU)                                                     \
  X(TCI, 0x066u, SFU)                                                          \
  X(TTRI, 0x067u, SFU)                                                         \
  X(THISTOGRAM, 0x068u, SFU)                                                   \
  X(TQUANT, 0x06au, SFU)                                                       \
  X(TDEQUANT, 0x06bu, SFU)                                                     \
  X(TSORT, 0x06cu, SFU)                                                        \
  X(TMRGSORT, 0x06du, SFU)                                                     \
  X(TGATHER, 0x06fu, SFU)                                                      \
  X(TSCATTER, 0x070u, SFU)                                                     \
  X(TPERMUTE, 0x075u, SFU)                                                     \
  X(TSHUF, 0x076u, SFU)                                                        \
  X(TPACK, 0x077u, SFU)                                                        \
  X(TUNPACK, 0x078u, SFU)                                                      \
  X(TGPR2T, 0x07eu, SFU)

inline bool isCanonicalTileOperationV058(unsigned Selector) {
  switch (Selector) {
#define X(Name, Value, Engine) case Value:
    LINXISA_V058_TILE_OPERATION_LIST(X)
#undef X
    return true;
  default:
    return false;
  }
}

inline std::optional<unsigned> parseTileOperationV058(StringRef UpperName) {
  return StringSwitch<std::optional<unsigned>>(UpperName)
#define X(Name, Value, Engine) .Case(#Name, Value)
      LINXISA_V058_TILE_OPERATION_LIST(X)
#undef X
          .Default(std::nullopt);
}

inline StringRef canonicalTileOperationNameV058(unsigned Selector) {
  switch (Selector) {
#define X(Name, Value, Engine)                                                 \
  case Value:                                                                  \
    return #Name;
    LINXISA_V058_TILE_OPERATION_LIST(X)
#undef X
  default:
    return StringRef();
  }
}

inline std::optional<TileEngineV058> tileEngineV058(unsigned Selector) {
  switch (Selector) {
#define X(Name, Value, Engine)                                                 \
  case Value:                                                                  \
    return TileEngineV058::Engine;
    LINXISA_V058_TILE_OPERATION_LIST(X)
#undef X
  default:
    return std::nullopt;
  }
}

inline StringRef tileEngineNameV058(TileEngineV058 Engine) {
  switch (Engine) {
  case TileEngineV058::VEC:
    return "VEC";
  case TileEngineV058::SFU:
    return "SFU";
  case TileEngineV058::TLSU:
    return "TLSU";
  case TileEngineV058::CUBE:
    return "CUBE";
  }
  llvm_unreachable("unknown PTO 0.58 tile engine");
}

inline StringRef tileOperationAssemblyAliasV058(TileEngineV058 Engine) {
  switch (Engine) {
  case TileEngineV058::VEC:
    return "BSTART.VEC";
  case TileEngineV058::SFU:
    return "BSTART.SFU";
  case TileEngineV058::TLSU:
  case TileEngineV058::CUBE:
    llvm_unreachable("TLSU/CUBE use operation-specific block headers");
  }
  llvm_unreachable("unknown PTO 0.58 tile engine");
}

#undef LINXISA_V058_TILE_OPERATION_LIST

} // namespace LinxISA
} // namespace llvm

#endif
