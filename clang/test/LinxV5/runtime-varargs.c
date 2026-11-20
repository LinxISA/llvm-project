// RUN: %clang --target=linx64v5 -mcpu=janus -mlinx-runtime-intra-abi -ffreestanding -O2 -c %s -o %t.o
// RUN: %clang --target=linx64v5 -mcpu=janus -mlinx-runtime-intra-abi -ffreestanding -O0 -c %s -o %t.o0.o
#include <stdarg.h>
unsigned optional(unsigned flags, ...) {
  unsigned result = 0;
  if (flags) {
    va_list arguments;
    va_start(arguments, flags);
    result = va_arg(arguments, unsigned);
    va_end(arguments);
  }
  return result;
}
