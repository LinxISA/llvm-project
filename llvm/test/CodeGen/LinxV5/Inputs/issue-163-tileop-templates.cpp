// Backend fixture: ../issue-163-tileop-templates.ll.
// Regenerate with TileOP 79d429f; see the IR header for the command.
// Generated IR regression provenance: Linx-TileOP-API 79d429f.
#include <common/pto_tileop.hpp>
#include <type_traits>
using namespace pto;
template<typename T, int R, int C>
using GM = global_tensor<T, RowMajor<R, C>>;

template<int M, bool Scale>
__attribute__((always_inline)) inline void mx_loop(
    float *out, __fp8_e4m3 *ap, __fp8_e8m0 *sap,
    __fp8_e4m3 *bp, __fp8_e8m0 *sbp, uint8_t *sp, unsigned count) {
  constexpr int Rows = M / 4;
  using D = std::conditional_t<M == 64, CubeAccumulatorM16<float, Rows, 64>,
                                     CubeAccumulatorM32<float, Rows, 64>>;
  using AT = SharedMatrixLeft<__fp8_e4m3, M, 64>;
  using BT = SharedMatrixRight<__fp8_e4m3, 64, 64>;
  using SAT = SharedMatrixLeft<__fp8_e8m0, M, 8, M, 2>;
  using SBT = SharedMatrixRight<__fp8_e8m0, 64, 8, 64, 2>;
  using CS = Tile<Location::Vec, uint8_t, 32, 8, BLayout::CubeM32, Rows, 1>;
  GM<__fp8_e4m3, M, 64> ga(ap);
  GM<__fp8_e4m3, 64, 64> gb(bp);
  GM<__fp8_e8m0, M, 8> gsa(sap);
  GM<__fp8_e8m0, 64, 8> gsb(sbp);
  auto a = TLOAD<AT>(ga); auto sa = TLOAD<SAT>(gsa);
  auto b = TLOAD<BT>(gb); auto sb = TLOAD<SBT>(gsb);
  D d;
  TMATMUL_MX<3>(d, a, sa, b, sb, fixp::keep_acc());
  if constexpr (Scale) {
    CS cs; GM<uint8_t, 32, 8> gs(sp); TLOAD_CUBE(cs, gs);
    for (unsigned i = 0; i < count; ++i)
      TMATMUL_MX_ACC<3>(d, d, a, sa, b, sb, fixp::keep_acc().cscale(cs));
  } else {
    for (unsigned i = 0; i < count; ++i)
      TMATMUL_MX_ACC<3>(d, d, a, sa, b, sb, fixp::keep_acc());
  }
  GM<float, Rows, 64> gd(out); TSTORE_CUBE(gd, d);
}
#define WRAPPER(M, S) \
extern "C" void mx_m##M##_s##S(float *o, __fp8_e4m3 *a, __fp8_e8m0 *sa, \
                             __fp8_e4m3 *b, __fp8_e8m0 *sb, uint8_t *s, unsigned n) { \
  mx_loop<M, S>(o, a, sa, b, sb, s, n); \
}
WRAPPER(64, 0)
WRAPPER(64, 1)
WRAPPER(128, 0)
WRAPPER(128, 1)

template<typename T>
__attribute__((always_inline)) inline void reductions(T *out, T *rp, T *gp,
                                                     float *ap, float *bp) {
  using A = CubeTileM32<float, 32, 64>;
  using B = CubeTileN8<float, 64, 256>;
  using D = CubeAccumulatorM32<T, 32, 256>;
  using R = CubeAccumulatorM32<T, 32, 2, 32, 1>;
  using G = CubeAccumulatorM32<T, 32, 8>;
  A a; B b; D d; R r; G g;
  GM<float, 32, 64> ga(ap); GM<float, 64, 256> gb(bp);
  TLOAD_CUBE(a, ga); TLOAD_CUBE(b, gb);
  if constexpr (std::is_same_v<T, __bf16>)
    TMATMUL(d, a, b, fixp::bf16().row_max(r).template group_max<32>(g));
  else
    TMATMUL(d, a, b, fixp::f16().row_max(r).template group_max<32>(g));
  GM<T, 32, 256> gd(out); GM<T, 32, 2> gr(rp); GM<T, 32, 8> gg(gp);
  TSTORE_CUBE(gd, d); TSTORE_CUBE(gr, r); TSTORE_CUBE(gg, g);
}
extern "C" void bf16_reductions(__bf16 *o, __bf16 *r, __bf16 *g, float *a, float *b) {
  reductions(o, r, g, a, b);
}
extern "C" void fp16_reductions(__half *o, __half *r, __half *g, float *a, float *b) {
  reductions(o, r, g, a, b);
}
