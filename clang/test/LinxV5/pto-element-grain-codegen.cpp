// RUN: %python %S/Inputs/check-element-grain.py --clang %clang --source %s --output %t

#ifndef GRAIN
#define GRAIN 256
#endif
#ifndef TYPE
#define TYPE unsigned
#endif
#ifndef COUNT
#define COUNT 129
#endif

extern "C" void typed(const TYPE *__restrict input,
                      const TYPE *__restrict other, TYPE *__restrict output) {
#pragma pto element for grain(GRAIN)
  for (unsigned element = 0; element < COUNT; ++element) {
    TYPE left = input[element];
    TYPE right = other[element];
    if ((left > static_cast<TYPE>(2)) | (right < static_cast<TYPE>(1)))
      output[element] = left + right;
    else
      output[element] = left - right;
  }
}

extern "C" void integer_ops(const unsigned *__restrict input,
                            unsigned *__restrict output) {
#pragma pto element for grain(GRAIN)
  for (unsigned element = 0; element < COUNT; ++element) {
    unsigned value = input[element];
    if ((value > 3) & (value < 100))
      output[element] = ((value << 1) | element) ^ 7;
    else
      output[element] = (value & 15) + element;
  }
}

extern "C" void indexed(const unsigned *__restrict input,
                        const unsigned *__restrict indices,
                        unsigned *__restrict output) {
#pragma pto element for grain(GRAIN)
  for (unsigned element = 0; element < COUNT; ++element) {
    unsigned index = indices[element];
    if (index < COUNT)
      output[element] = input[index] + element;
  }
}

#ifndef HOST_TEST
extern "C" void atomic_sum(const unsigned *__restrict input,
                           unsigned *__restrict output) {
#pragma pto element for grain(GRAIN)
  for (unsigned element = 0; element < COUNT; ++element) {
    unsigned value = input[element];
    if (value > 1)
      __atomic_fetch_add(output + (element & 7), value, __ATOMIC_RELAXED);
  }
}
#endif
