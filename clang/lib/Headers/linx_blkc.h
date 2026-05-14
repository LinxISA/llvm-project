#ifndef __LINX_BLKC
#define __LINX_BLKC

#include <stdint.h>

#define tile_size(n) __attribute__((ext_vector_type(n)))

#define __vbuf__ __attribute__((address_space(6)))

#define __bf16 __blkc_bf16
typedef _Float16 __half;
typedef float __fp32;

struct __fp8_base {
  char data;
};

struct __bf16_base {
  short data;
};

struct __fp16_base {
  short data;
};

struct __fp32_base {
  float data;
};

struct __fp64_base {
  double data;
};

struct __int4x2_base {
  char data;
};

struct __uint4x2_base {
  unsigned char data;
};

struct __int32_base {
  int data;
};

struct __uint32_base {
  unsigned int data;
};


#define DEFINE_OPERATOR(CLS, T, DSTTYPE, DSTWIDTH, STORAGE)                    \
  operator T() {                                                               \
    T res;                                                                     \
    asm volatile("v.cvt." CLS##_TYPE "2" DSTTYPE " %1." CLS##_MAJOR_TYPE       \
                 ", -> %0." DSTWIDTH "\n"                                      \
                 : "=vr"(STORAGE(res))                                         \
                 : "vr"(CLS##_STORAGE(*this)));                                \
    return res;                                                                \
  }

#define DEFINE_CONSTRUCT(CLS, T, SRCTYPE, MAJORTYPE, STORAGE)                  \
  CLS(T from) {                                                                \
    asm volatile("v.cvt." SRCTYPE "2" CLS##_TYPE " %1." MAJORTYPE              \
                                                 ", -> %0." CLS##_WIDTH "\n"   \
                 : "=vr"(CLS##_STORAGE(*this))                                 \
                 : "vr"(STORAGE(from)));                                       \
  }

#define DEFINE_CAST(CLS, T, DSTTYPE, DSTWIDTH, SRCTYPE, MAJORTYPE, STORAGE)    \
  DEFINE_CONSTRUCT(CLS, T, SRCTYPE, MAJORTYPE, STORAGE)                        \
  DEFINE_OPERATOR(CLS, T, DSTTYPE, DSTWIDTH, STORAGE)

#define SIMPLE_STORAGE(d) (d)

#define DEFINE_SIMPLE_CASTS(CLS)                                               \
  DEFINE_CAST(CLS, double, "fp64", "d", "fp64", "fd", SIMPLE_STORAGE)          \
  DEFINE_CAST(CLS, float, "fp32", "w", "fp32", "fs", SIMPLE_STORAGE)           \
  DEFINE_CAST(CLS, __half, "fp16", "h", "fp16", "fh", SIMPLE_STORAGE)          \
  DEFINE_CAST(CLS, long, "s64", "d", "s64", "sd", SIMPLE_STORAGE)              \
  DEFINE_CAST(CLS, int, "s32", "w", "s32", "sw", SIMPLE_STORAGE)               \
  DEFINE_CAST(CLS, short, "s16", "h", "s16", "sh", SIMPLE_STORAGE)             \
  DEFINE_CAST(CLS, char, "s8", "b", "s8", "sb", SIMPLE_STORAGE)                \
  DEFINE_CAST(CLS, unsigned long, "u64", "d", "u64", "ud", SIMPLE_STORAGE)     \
  DEFINE_CAST(CLS, unsigned int, "u32", "w", "u32", "uw", SIMPLE_STORAGE)      \
  DEFINE_CAST(CLS, unsigned short, "u16", "h", "u16", "uh", SIMPLE_STORAGE)    \
  DEFINE_CAST(CLS, unsigned char, "u8", "b", "u8", "ub", SIMPLE_STORAGE)

#define DEFINE_SPECIAL_CAST(ME, OTHER)                                         \
  DEFINE_CONSTRUCT(ME, OTHER, OTHER##_TYPE, OTHER##_MAJOR_TYPE,                \
                   OTHER##_STORAGE)                                            \
  DEFINE_OPERATOR(ME, OTHER, OTHER##_TYPE, OTHER##_WIDTH, OTHER##_STORAGE)

struct __fp8_e4m3 : public __fp8_base {
public:
  __fp8_e4m3() = default;

#define __fp8_e4m3_TYPE "e4m3"
#define __fp8_e4m3_MAJOR_TYPE "fb"
#define __fp8_e4m3_WIDTH "b"
#define __fp8_e4m3_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__fp8_e4m3)
};

struct __fp8_e5m2 : public __fp8_base {
  __fp8_e5m2() = default;

#define __fp8_e5m2_TYPE "e5m2"
#define __fp8_e5m2_MAJOR_TYPE "fb"
#define __fp8_e5m2_WIDTH "b"
#define __fp8_e5m2_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__fp8_e5m2)
  DEFINE_SPECIAL_CAST(__fp8_e5m2, __fp8_e4m3)
};

struct __blkc_bf16 : public __bf16_base {
  __blkc_bf16() = default;

#define __blkc_bf16_TYPE "bf16"
#define __blkc_bf16_MAJOR_TYPE "fh"
#define __blkc_bf16_WIDTH "h"
#define __blkc_bf16_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__blkc_bf16)
  DEFINE_SPECIAL_CAST(__blkc_bf16, __fp8_e4m3)
  DEFINE_SPECIAL_CAST(__blkc_bf16, __fp8_e5m2)
};

struct __tf32 : public __fp32_base {
public:
  __tf32() = default;

#define __tf32_TYPE "tf32"
#define __tf32_MAJOR_TYPE "fs"
#define __tf32_WIDTH "w"
#define __tf32_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__tf32)
};

struct __hf32 : public __fp32_base {
public:
  __hf32() = default;

#define __hf32_TYPE "hf32"
#define __hf32_MAJOR_TYPE "fs"
#define __hf32_WIDTH "w"
#define __hf32_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__hf32)
};

struct __hif8 : public __fp8_base {
public:
  __hif8() = default;

#define __hif8_TYPE "hif8"
#define __hif8_MAJOR_TYPE "fb"
#define __hif8_WIDTH "b"
#define __hif8_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__hif8)
  DEFINE_SPECIAL_CAST(__hif8, __fp8_e4m3)
  DEFINE_SPECIAL_CAST(__hif8, __fp8_e5m2)
};

struct __fp8_e8m0 : public __fp8_base {
public:
  __fp8_e8m0() = default;

#define __fp8_e8m0_TYPE "e8m0"
#define __fp8_e8m0_MAJOR_TYPE "fb"
#define __fp8_e8m0_WIDTH "b"
#define __fp8_e8m0_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__fp8_e8m0)
  DEFINE_SPECIAL_CAST(__fp8_e8m0, __fp8_e4m3)
  DEFINE_SPECIAL_CAST(__fp8_e8m0, __fp8_e5m2)
};

struct __fp8_e6m2 : public __fp8_base {
public:
  __fp8_e6m2() = default;

#define __fp8_e6m2_TYPE "e6m2"
#define __fp8_e6m2_MAJOR_TYPE "fb"
#define __fp8_e6m2_WIDTH "b"
#define __fp8_e6m2_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__fp8_e6m2)
  DEFINE_SPECIAL_CAST(__fp8_e6m2, __fp8_e4m3)
  DEFINE_SPECIAL_CAST(__fp8_e6m2, __fp8_e5m2)
};

struct __fp4_e2m1x2 : public __fp8_base {
public:
  __fp4_e2m1x2() = default;

#define __fp4_e2m1x2_TYPE "e2m1x2"
#define __fp4_e2m1x2_MAJOR_TYPE "fb"
#define __fp4_e2m1x2_WIDTH "b"
#define __fp4_e2m1x2_STORAGE(d) ((d).data)

  // DEFINE_SIMPLE_CASTS(__fp4_e2m1x2)
};

struct __fp4_e1m2x2 : public __fp8_base {
public:
  __fp4_e1m2x2() = default;

#define __fp4_e1m2x2_TYPE "e1m2x2"
#define __fp4_e1m2x2_MAJOR_TYPE "fb"
#define __fp4_e1m2x2_WIDTH "b"
#define __fp4_e1m2x2_STORAGE(d) ((d).data)

  // DEFINE_SIMPLE_CASTS(__fp4_e1m2x2)
  DEFINE_SPECIAL_CAST(__fp4_e1m2x2, __fp4_e2m1x2)
};

struct __fp4_hif4x2 : public __fp8_base {
public:
  __fp4_hif4x2() = default;

#define __fp4_hif4x2_TYPE "hif4x2"
#define __fp4_hif4x2_MAJOR_TYPE "fb"
#define __fp4_hif4x2_WIDTH "b"
#define __fp4_hif4x2_STORAGE(d) ((d).data)

  // DEFINE_SIMPLE_CASTS(__fp4_hif4x2)
  DEFINE_SPECIAL_CAST(__fp4_hif4x2, __fp4_e2m1x2)
  DEFINE_SPECIAL_CAST(__fp4_hif4x2, __fp4_e1m2x2)
};

struct __int4x2 : public __int4x2_base {
public:
  __int4x2() = default;

#define __int4x2_TYPE "s4x2"
#define __int4x2_MAJOR_TYPE "sb"
#define __int4x2_WIDTH "b"
#define __int4x2_STORAGE(d) ((d).data)

  // DEFINE_SIMPLE_CASTS(__int4x2)
};

struct __uint4x2 : public __uint4x2_base {
public:
  __uint4x2() = default;

#define __uint4x2_TYPE "u4x2"
#define __uint4x2_MAJOR_TYPE "ub"
#define __uint4x2_WIDTH "b"
#define __uint4x2_STORAGE(d) ((d).data)

  // DEFINE_SIMPLE_CASTS(__uint4x2)
};

struct __fp6_e3m2 : public __fp8_base {
public:
  __fp6_e3m2() = default;
#define __fp6_e3m2_TYPE "e3m2"
#define __fp6_e3m2_MAJOR_TYPE "fb"
#define __fp6_e3m2_WIDTH "b"
#define __fp6_e3m2_STORAGE(d) ((d).data)
  DEFINE_SIMPLE_CASTS(__fp6_e3m2)
  DEFINE_SPECIAL_CAST(__fp6_e3m2, __fp8_e4m3)
  DEFINE_SPECIAL_CAST(__fp6_e3m2, __fp8_e5m2)
};

struct __fp6_e2m3 : public __fp8_base {
public:
  __fp6_e2m3() = default;
#define __fp6_e2m3_TYPE "e2m3"
#define __fp6_e2m3_MAJOR_TYPE "fb"
#define __fp6_e2m3_WIDTH "b"
#define __fp6_e2m3_STORAGE(d) ((d).data)
  DEFINE_SIMPLE_CASTS(__fp6_e2m3)
  DEFINE_SPECIAL_CAST(__fp6_e2m3, __fp8_e4m3)
  DEFINE_SPECIAL_CAST(__fp6_e2m3, __fp8_e5m2)
};

struct __fp16x2 : public __fp32_base {
public:
  __fp16x2() = default;

#define __fp16x2_TYPE "fp16x2"
#define __fp16x2_MAJOR_TYPE "fs"
#define __fp16x2_WIDTH "w"
#define __fp16x2_STORAGE(d) ((d).data)

  // DEFINE_SIMPLE_CASTS(__fp16x2)

friend __fp16x2 operator+(const __fp16x2 &lhs, const __fp16x2 &rhs) {
  __fp16x2 res;
  res.data = lhs.data + rhs.data;
  return res;
}

friend __fp16x2 operator-(const __fp16x2 &lhs, const __fp16x2 &rhs) {
  __fp16x2 res;
  res.data = lhs.data - rhs.data;
  return res;
}

friend __fp16x2 operator*(const __fp16x2 &lhs, const __fp16x2 &rhs) {
  __fp16x2 res;
  res.data = lhs.data * rhs.data;
  return res;
}

friend __fp16x2 operator/(const __fp16x2 &lhs, const __fp16x2 &rhs) {
  __fp16x2 res;
  res.data = lhs.data / rhs.data;
  return res;
}
};

struct __bf16x2 : public __fp32_base {
public:
  __bf16x2() = default;

#define __bf16x2_TYPE "bf16x2"
#define __bf16x2_MAJOR_TYPE "fs"
#define __bf16x2_WIDTH "w"
#define __bf16x2_STORAGE(d) ((d).data)
};

struct __uint16x2 : public __uint32_base {
public:
  __uint16x2() = default;

#define __uint16x2_TYPE "u16x2"
#define __uint16x2_MAJOR_TYPE "uh"
#define __uint16x2_WIDTH "w"
#define __uint16x2_STORAGE(d) ((d).data)
};

struct __int16x2 : public __int32_base {
public:
  __int16x2() = default;

#define __int16x2_TYPE "s16x2"
#define __int16x2_MAJOR_TYPE "sh"
#define __int16x2_WIDTH "w"
#define __int16x2_STORAGE(d) ((d).data)
};

struct __fp8_e4m3x4 : public __fp32_base {
public:
  __fp8_e4m3x4() = default;

#define __fp8_e4m3x4_TYPE "e4m3x4"
#define __fp8_e4m3x4_MAJOR_TYPE "fb"
#define __fp8_e4m3x4_WIDTH "w"
#define __fp8_e4m3x4_STORAGE(d) ((d).data)
};

struct __fp8_e5m2x4 : public __fp32_base {
public:
  __fp8_e5m2x4() = default;

#define __fp8_e5m2x4_TYPE "e5m2x4"
#define __fp8_e5m2x4_MAJOR_TYPE "fb"
#define __fp8_e5m2x4_WIDTH "w"
#define __fp8_e5m2x4_STORAGE(d) ((d).data)
};

struct __uint8x4 : public __uint32_base {
public:
  __uint8x4() = default;

#define __uint8x4_TYPE "u8x4"
#define __uint8x4_MAJOR_TYPE "ub"
#define __uint8x4_WIDTH "w"
#define __uint8x4_STORAGE(d) ((d).data)
};

struct __int8x4 : public __int32_base {
public:
  __int8x4() = default;

#define __int8x4_TYPE "s8x4"
#define __int8x4_MAJOR_TYPE "sb"
#define __int8x4_WIDTH "w"
#define __int8x4_STORAGE(d) ((d).data)
};

struct __fp8_e6m2x2 : public __fp16_base {
public:
  __fp8_e6m2x2() = default;

#define __fp8_e6m2x2_TYPE "e6m2x2"
#define __fp8_e6m2x2_MAJOR_TYPE "fh"
#define __fp8_e6m2x2_WIDTH "h"
#define __fp8_e6m2x2_STORAGE(d) ((d).data)
};

struct __fp8_e4m3x2 : public __fp16_base {
public:
  __fp8_e4m3x2() = default;

#define __fp8_e4m3x2_TYPE "e4m3x2"
#define __fp8_e4m3x2_MAJOR_TYPE "fh"
#define __fp8_e4m3x2_WIDTH "h"
#define __fp8_e4m3x2_STORAGE(d) ((d).data)
};

struct __fp8_e5m2x2 : public __fp16_base {
public:
  __fp8_e5m2x2() = default;

#define __fp8_e5m2x2_TYPE "e5m2x2"
#define __fp8_e5m2x2_MAJOR_TYPE "fh"
#define __fp8_e5m2x2_WIDTH "h"
#define __fp8_e5m2x2_STORAGE(d) ((d).data)
};

#endif
