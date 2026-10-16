// RUN: %clang++ --target=linx64v5 -mlxbc -std=c++17 -fsyntax-only -Xclang -verify %s

void outer_break(int *out) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (out[element])
      break; // expected-error {{'break' exits a '#pragma pto element for' region; only break from a nested loop or switch is supported}}
  }
}

int function_return(const int *in) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (in[element])
      return 1; // expected-error {{'return' exits a '#pragma pto element for' region; function return is not supported}}
  }
  return 0;
}

void goto_out(int *out) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (out[element])
      goto done; // expected-error {{'goto' crosses into or out of a '#pragma pto element for' region; goto is not supported}}
  }
done:
  return;
}

void label_entry(int *out) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
entry: // expected-error {{a label creates an entry into a '#pragma pto element for' region; labels are not supported}}
    out[element] = 0;
  }
}

void supported_control(int *out, const int *in) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (!in[element])
      continue;
    for (unsigned probe = 0; probe < 4; ++probe) {
      if (in[element] == static_cast<int>(probe))
        break;
      if (probe & 1)
        continue;
      out[element] += in[element];
    }
    switch (in[element]) {
    case 1:
      out[element] = 1;
      break;
    default:
      break;
    }
    auto helper = [](int value) {
      if (value)
        return value;
      return 0;
    };
    out[element] += helper(in[element]);
  }
}
