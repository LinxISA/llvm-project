// RUN: %clang_cc1 -triple linx64v5 -fsyntax-only -verify %s
void bad(unsigned long address, unsigned long word) {
  (void)__builtin_linx_msgb_read(address); // expected-error {{must be a constant integer}}
  __builtin_linx_msgb_write(address, word); // expected-error {{must be a constant integer}}
  (void)__builtin_linx_msgb_read(0x82f); // expected-error {{outside the valid range}}
  __builtin_linx_msgb_write(0x84c, word); // expected-error {{outside the valid range}}
}
