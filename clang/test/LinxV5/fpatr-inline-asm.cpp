// RUN: %clang++ --target=linx64v5 -mcpu=janus -O2 -c %s -o %t.numeric.o
// RUN: %clang++ --target=linx64v5 -mcpu=janus -O2 -DNAMED -c %s -o %t.named.o
// RUN: cmp %t.numeric.o %t.named.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.numeric.o | FileCheck %s

template <unsigned Quant, unsigned Activation, unsigned GroupCode,
          unsigned RowMax, unsigned GroupMax>
__attribute__((always_inline)) inline void attributes() {
#ifdef NAMED
  asm volatile("B.FPATR PreQuantMode=%c[Quant], ReluMode=%c[Activation], "
               "GroupNCode=%c[GroupCode], RowMaxEn=%c[RowMax], "
               "GroupMaxEn=%c[GroupMax], RowMaxInit=0, MaxAbsEn=0, "
               "TransA=0, TransB=0, CScaleEn=0"
#else
  asm volatile("B.FPATR %c[Quant], %c[Activation], %c[GroupCode], "
               "%c[RowMax], %c[GroupMax], 0, 0, 0, 0, 0"
#endif
               :
               : [Quant] "i"(Quant), [Activation] "i"(Activation),
                 [GroupCode] "i"(GroupCode), [RowMax] "i"(RowMax),
                 [GroupMax] "i"(GroupMax));
}

void neutral() { attributes<0, 0, 0, 0, 0>(); }
// CHECK-LABEL: <_Z7neutralv>:
// CHECK: B.FPATR PreQuantMode=NoQuant, ReluMode=None, GroupN=0, RowMaxEn=0, GroupMaxEn=0, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0

void postprocess() { attributes<16, 1, 3, 1, 1>(); }
// CHECK-LABEL: <_Z11postprocessv>:
// CHECK: B.FPATR PreQuantMode=F322BF16, ReluMode=ReLU, GroupN=32, RowMaxEn=1, GroupMaxEn=1, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0
