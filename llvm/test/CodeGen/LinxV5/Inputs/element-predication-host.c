#include <stdint.h>
#include <stdio.h>

extern void three_way_phi(const int32_t *, int32_t *);
extern void tail_33(const uint32_t *, uint32_t *, uint64_t, _Bool, uint32_t);
extern void tail_129(const uint32_t *, uint32_t *, uint64_t, _Bool, uint32_t);
extern void two_regions(int32_t *);
extern void conditional(uint32_t *, const uint32_t *);
extern void nested_break_continue(uint32_t *, uint32_t, uint32_t, uint32_t);
extern void nested_cpp(uint32_t *, uint32_t, uint32_t, uint32_t, uint32_t);
extern void lane_bound_cpp(uint32_t *, uint32_t, uint32_t, uint32_t);
extern void nested_effect_cpp(uint32_t *);
extern void dynamic_loop(uint32_t *, uint32_t, uint32_t);
extern void dynamic_diamond(uint32_t *, uint32_t);
extern void dynamic_cpp(uint32_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                        uint32_t);
extern void dynamic_nested_cpp(uint32_t *, uint32_t, uint32_t, uint32_t);
extern void dynamic_effect_cpp(uint32_t *, uint32_t, uint32_t);

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
  for (uint32_t limit = 0; limit < 6; ++limit)
    for (uint32_t stop = 0; stop < 6; ++stop)
      for (uint32_t skip = 0; skip < 6; ++skip)
        for (uint32_t divisor = 1; divisor < 5; ++divisor) {
          uint32_t nested[32];
          uint32_t cpp_nested[32];
          nested_break_continue(nested, stop, skip, divisor);
          nested_cpp(cpp_nested, limit, stop, skip, divisor);
          for (uint32_t element = 0; element < 32; ++element) {
            uint32_t expected = 0;
            uint32_t cpp_expected = 0;
            for (uint32_t iteration = 0; iteration < 4; ++iteration) {
              if (iteration == ((element + stop) & 3))
                break;
              if (iteration == skip)
                continue;
              expected += (element + iteration) / divisor;
            }
            for (uint32_t iteration = 0; iteration < (limit & 3); ++iteration) {
              if (iteration == ((element + stop) & 3))
                break;
              if (iteration == skip)
                continue;
              for (uint32_t inner = 0; inner < 2; ++inner)
                cpp_expected += (element + iteration + inner) / divisor;
            }
            if (nested[element] != expected ||
                cpp_nested[element] != cpp_expected)
              return 7;
          }
        }
  nested_cpp(output, 0, 0, 0, 0);
  for (unsigned element = 0; element < 32; ++element)
    if (output[element] != 0)
      return 8;
  nested_cpp(output, 1, 0, 0, 0);
  for (unsigned element = 0; element < 32; ++element)
    if (output[element] != 0)
      return 9;
  for (uint32_t valid = 0; valid <= 33; ++valid)
    for (uint32_t seed = 0; seed < 6; ++seed) {
      for (unsigned element = 0; element < 32; ++element)
        output[element] = 0xdeadbeef;
      lane_bound_cpp(output, valid, seed, 3);
      for (unsigned element = 0; element < 32; ++element) {
        uint32_t expected = 0xdeadbeef;
        if (element < valid) {
          expected = element;
          for (uint32_t iteration = 0; iteration < ((element + seed) & 3);
               ++iteration)
            expected += (element + iteration) / 3;
        }
        if (output[element] != expected)
          return 10;
      }
    }
  lane_bound_cpp(0, 0, 0, 0);
  for (unsigned element = 0; element < 32; ++element)
    output[element] = element + 1;
  nested_effect_cpp(output);
  for (unsigned element = 0; element < 32; ++element) {
    uint32_t expected = element + 1;
    for (unsigned iteration = 0; iteration < 3; ++iteration) {
      if (iteration == (element & 3))
        break;
      if (iteration == 1)
        continue;
      expected = expected * 3 + iteration;
    }
    if (output[element] != expected)
      return 11;
  }
  for (uint32_t limit = 0; limit < 130; ++limit) {
    dynamic_diamond(output, limit);
    for (uint32_t element = 0; element < 32; ++element) {
      uint32_t expected = 0;
      for (uint32_t iteration = 0; iteration < limit; ++iteration)
        expected += element + iteration + (element < 16 ? 1 : 2);
      if (output[element] != expected)
        return 16;
    }
  }
  for (uint32_t limit = 0; limit < 130; ++limit)
    for (uint32_t skip = 0; skip < 5; ++skip) {
      dynamic_loop(output, limit, skip);
      for (uint32_t element = 0; element < 32; ++element) {
        uint32_t expected = 0;
        for (uint32_t iteration = 0; iteration < limit; ++iteration)
          if (iteration != skip)
            expected += element + iteration;
        if (output[element] != expected)
          return 12;
      }
      dynamic_cpp(output, limit, 3, skip, 3, 29);
      for (uint32_t element = 0; element < 32; ++element) {
        uint32_t expected = 0;
        for (uint32_t iteration = 0; iteration < limit; ++iteration)
          if (iteration != skip)
            expected += element + iteration;
        if (element < 29) {
          expected = element;
          for (uint32_t iteration = 0; iteration < element % 7 + limit;
               ++iteration) {
            if (iteration == element % 5 + 3)
              break;
            if (iteration == skip)
              continue;
            expected += (element + iteration) / 3;
          }
        }
        if (output[element] != expected)
          return 13;
      }
    }
  for (uint32_t limit = 0; limit < 12; ++limit)
    for (uint32_t inner_limit = 0; inner_limit < 8; ++inner_limit) {
      dynamic_nested_cpp(output, limit, inner_limit, 2);
      for (uint32_t element = 0; element < 32; ++element) {
        uint32_t expected = 0;
        for (uint32_t outer = 0; outer < limit; ++outer) {
          if (outer == 2)
            continue;
          for (uint32_t inner = 0; inner < inner_limit; ++inner) {
            if (inner == element % 5)
              break;
            expected += element + outer + inner;
          }
        }
        if (output[element] != expected)
          return 14;
      }
    }
  dynamic_cpp(0, 7, 0, 0, 0, 0);
  for (uint32_t limit = 0; limit < 12; ++limit)
    for (uint32_t skip = 0; skip < 5; ++skip) {
      for (uint32_t element = 0; element < 32; ++element)
        output[element] = element + 1;
      dynamic_effect_cpp(output, limit, skip);
      for (uint32_t element = 0; element < 32; ++element) {
        uint32_t expected = element + 1;
        for (uint32_t iteration = 0; iteration < limit; ++iteration) {
          if (iteration == element % 5)
            break;
          if (iteration == skip)
            continue;
          expected = expected * 3 + iteration;
        }
        if (output[element] != expected)
          return 15;
      }
    }
  puts("element predication host semantics: PASS");
  return 0;
}
