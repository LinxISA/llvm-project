// RUN: not %clang -target x86_64-unknown-linux-gnu -std=c++20 -fsyntax-only \
// RUN:   -include %S/../../lib/Headers/linx_blkc.h %s 2>&1 | \
// RUN:   FileCheck %s --check-prefix=HOST
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -S -o /dev/null \
// RUN:   -DBAD_TR %s 2>&1 | FileCheck %s --check-prefix=TR
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -S -o /dev/null \
// RUN:   -DBAD_TR_SIZE %s 2>&1 | FileCheck %s --check-prefix=TR-SIZE
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -c -O2 -o /dev/null \
// RUN:   -DBAD_TR_TIED %s 2>&1 | FileCheck %s --check-prefix=TR-TIED
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -c -O2 -o /dev/null \
// RUN:   -DBAD_SIZE_CODE %s 2>&1 | FileCheck %s --check-prefix=SIZE-CODE
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -S -O2 -o /dev/null \
// RUN:   -DBAD_SR_ESCAPE %s 2>&1 | FileCheck %s --check-prefix=SR-ESCAPE
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -S -o /dev/null \
// RUN:   -DBAD_SR_TYPE %s 2>&1 | FileCheck %s --check-prefix=SR-TYPE
// RUN: not %clang -target linx64-unknown-linux-musl -std=c++20 -fsyntax-only \
// RUN:   -DBAD_VECTOR %s 2>&1 | FileCheck %s --check-prefix=VECTOR

// HOST: error: "linx_blkc.h is only supported for the Linx target"

#if defined(BAD_TR)
void bad_tr(unsigned &Value) {
  asm volatile("" : "+Tr"(Value));
}
// TR: error: invalid type 'unsigned int' in asm input for constraint '^Tr'
#endif

#if defined(BAD_TR_SIZE)
using SmallTile = float tile_size(32);
void bad_tr_size(SmallTile &Value) {
  asm volatile("" : "+Tr"(Value));
}
// TR-SIZE: error: invalid type 'SmallTile' (vector of 32 'float' values) in asm input for constraint '^Tr'
#endif

#if defined(BAD_TR_TIED)
using TiedTile = float tile_size(1024);
void bad_tr_tied(TiedTile &Value) {
  asm volatile("B.IOT %0, mask=1111, last, ->%0<4KB>" : "+Tr"(Value));
}
// TR-TIED: error: invalid operand in inline asm
#endif

#if defined(BAD_SIZE_CODE)
using SizeTile = float tile_size(1024);
void bad_size_code(SizeTile &Value) {
  asm volatile("B.IOT mask=1111, last, ->%0<%Z1>"
               : "=Tr"(Value)
               : "i"(0));
}
// SIZE-CODE: error: invalid operand in inline asm
#endif

#if defined(BAD_SR_ESCAPE)
void bad_sr_escape(unsigned long &Dst, unsigned long Src) {
  asm volatile("" : "=Sr"(Dst) : "Sr"(Src));
}
// SR-ESCAPE: error: {{.*}}Sr values are compiler-local Shared handles and cannot be copied to or from ordinary registers
#endif

#if defined(BAD_SR_TYPE)
void bad_sr_type(float &Value) {
  asm volatile("" : "=Sr"(Value));
}
// SR-TYPE: error: invalid type 'float' in asm input for constraint '^Sr'
#endif

#if defined(BAD_VECTOR)
enum class UserEnum : unsigned char {};
using BadVector = UserEnum tile_size(16);
// VECTOR: error: invalid vector element type 'UserEnum'
#endif
