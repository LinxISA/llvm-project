// RUN: %clang -target linx64v5-unknown-linux-musl -mlxbc -fenable-matrix -O2 \
// RUN:     -mllvm -enable-all-vector-as-tilereg=true -std=c++20 -D__linx \
// RUN:     -isystem %S/Inputs -S %s -o - | FileCheck %s

// Issue #123: CUBE tile values live across a real call are caller-save; the
// regalloc spill path (PseudoTSpill/PseudoTReload) is hard-coded S64/NORM
// and loses the CUBE layout, so a NORM reload feeding a CUBE_M32 consumer
// violates PTO #291. The compiler now de-promotes such values at IR level:
// a layout-correct transport store to a private slot right after the tile
// definition and a transport reload at every use below the call, so no tile
// register crosses the call and no spill is generated. The asm below
// replicates the TileOP TCVT template shape (clang rewrites the %-style
// positional operands to ${N:mod} in the IR, which is what the provenance
// parser sees).

using TileV = float __attribute__((ext_vector_type(8192)));

static inline TileV fake_cvt(const TileV &src) {
  TileV out;
  asm volatile(
      "BSTART.TEPL 27, %D1\n"
      ".if %c[RMode] == 0\nB.DATR %D2, RNONE\n.endif\n"
      "B.DIM %5, 0, ->lb0\n"
      "B.DIM %6, 0, ->lb1\n"
      "B.IOT %3, mask=1111, last, ->%0<%Z4>\n"
      : "=Tr"(out)
      : "i"(4 /* FP16 source dtype */), "i"(1 /* FP32 dest dtype */),
        "Tr"(src), "i"(9 /* size code */), "r"(256 /* ValidCol */),
        "r"(32 /* ValidRow */), "i"(29 /* CubeM32 layout code */),
        [RMode] "i"(0));
  return out;
}

extern "C" void opaque(void);

extern "C" void kernel(void) {
  TileV src;
  TileV t0 = fake_cvt(src);
  opaque(); // t0 is live across the call
  TileV t1 = fake_cvt(t0);
  fake_cvt(t1); // side-effect chain keeps every step observable
}

// The slot round-trip uses the M322ND store / ND2M32 load transports with
// the static shape (256 columns, 32 rows, 4-byte elements => 1024-byte
// stride) and a width-matched integer carrier for the private slot.
// CHECK: TSTORE.M322ND <LB0: 256, LB1: 32, LB2: 256, S32>
// CHECK: TLOAD.ND2M32 <LB0: 256, LB1: 32, LB2: 256, S32, Zero>
// No raw S64/NORM spill remains anywhere.
// CHECK-NOT: S64
