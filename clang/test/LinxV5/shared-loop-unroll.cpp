// RUN: %clang -target linx64v5 -mlxbc -O2 -DFULL -c %s -o %t.full.o
// RUN: %clang -target linx64v5 -mlxbc -O2 -DTWO -c %s -o %t.two.o
// RUN: %clang -target linx64v5 -mlxbc -O2 -c %s -o %t.loop.o

extern "C" void shared_loop_unroll(unsigned long count) {
  unsigned long handle;
  asm volatile("B.IOS mask=1111, ->%S0<512B>" : "=Sr"(handle));
#if defined(FULL)
#pragma clang loop unroll(full)
#elif defined(TWO)
#pragma clang loop unroll_count(2)
#else
#pragma clang loop unroll(disable)
#endif
  for (unsigned index = 0; index < 32; ++index) {
    asm volatile("B.IOS %S0, mask=1111" : : "Sr"(handle));
    unsigned long next;
    asm volatile("B.IOS mask=1111, ->%S0<512B>" : "=Sr"(next));
    handle = next;
  }
}
