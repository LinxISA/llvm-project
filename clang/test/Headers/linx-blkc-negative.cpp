// RUN: not %clang -target x86_64-unknown-linux-gnu -std=c++20 -fsyntax-only \
// RUN:   -include %S/../../lib/Headers/linx_blkc.h %s 2>&1 | \
// RUN:   FileCheck %s --check-prefix=HOST
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -S -o /dev/null \
// RUN:   -DBAD_TR %s 2>&1 | FileCheck %s --check-prefix=TR
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -fsyntax-only \
// RUN:   -DBAD_VECTOR %s 2>&1 | FileCheck %s --check-prefix=VECTOR

// HOST: error: "linx_blkc.h is only supported for the Linx target"

#if defined(BAD_TR)
void bad_tr(unsigned &Value) {
  asm volatile("" : "+Tr"(Value));
}
// TR: error: couldn't allocate output register for constraint 'Tr'
#endif

#if defined(BAD_VECTOR)
enum class UserEnum : unsigned char {};
using BadVector = UserEnum tile_size(16);
// VECTOR: error: invalid vector element type 'UserEnum'
#endif
