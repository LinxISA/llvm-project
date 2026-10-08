#include <stdint.h>
#include <stdio.h>

extern void three_way_phi(const int32_t *, int32_t *);
extern void tail_33(const uint32_t *, uint32_t *, uint64_t, _Bool, uint32_t);
extern void tail_129(const uint32_t *, uint32_t *, uint64_t, _Bool, uint32_t);
extern void two_regions(int32_t *);
extern void conditional(uint32_t *, const uint32_t *);

int main(void) {
  int32_t values[4] = {-2, 0, 4, 25};
  int32_t actual[4] = {0};
  three_way_phi(values, actual);
  for (unsigned i = 0; i < 4; ++i) {
    int32_t expected;
    if (values[i] < 0)
      expected = -values[i];
    else if (values[i] == 0)
      expected = 7;
    else
      expected = 100 / values[i];
    if (actual[i] != expected)
      return 1;
  }

  uint32_t input[129], output[129];
  for (unsigned i = 0; i < 129; ++i) {
    input[i] = 37 * i + 11;
    output[i] = 0xdeadbeef;
  }
  tail_33(input, output, 17, 1, 3);
  for (unsigned i = 0; i < 129; ++i)
    if (output[i] != (i < 17 ? input[i] / 3 : 0xdeadbeef))
      return 2;
  // Disabled memory must not touch a null pointer or divide by zero.
  tail_33(0, output, 0, 1, 0);
  tail_129(0, output, 129, 0, 0);
  for (unsigned i = 0; i < 129; ++i)
    if (output[i] != (i < 17 ? input[i] / 3 : 0xdeadbeef))
      return 3;
  tail_129(input, output, 127, 1, 5);
  for (unsigned i = 0; i < 129; ++i)
    if (output[i] != (i < 127 ? input[i] / 5 : 0xdeadbeef))
      return 4;

  two_regions(values);
  const int32_t expected_final[4] = {5, 15, 35, 140};
  for (unsigned i = 0; i < 4; ++i)
    if (values[i] != expected_final[i])
      return 5;
  for (unsigned i = 0; i < 129; ++i) {
    input[i] = i % 7 == 0 ? 0 : i + 1;
    output[i] = 0xdeadbeef;
  }
  // This function comes from Clang compiling the checked-in C++ pragma test,
  // not the handcrafted IR fixtures above.
  conditional(output, input);
  for (unsigned i = 0; i < 129; ++i) {
    uint32_t expected = 0xdeadbeef;
    if (i < 33) {
      uint32_t value = input[i];
      if (value & 1)
        expected = value + 3;
      else if (value == 0)
        expected = 7;
      else
        expected = 100 / value;
    }
    if (output[i] != expected)
      return 6;
  }
  puts("element predication host semantics: PASS");
  return 0;
}
