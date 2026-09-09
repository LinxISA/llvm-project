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


// Scalar casts use the scalar FSU convert instructions (fcvt.<src>2<dst>)
// operating on GPRs, or pure bit manipulation where no scalar encoding
// exists. The retired v.cvt.* forms were SIMT/vector-domain instructions:
// in the active block (janus) mode every dot-prefixed v.*/l.* instruction is
// unusable, and their narrow inline-asm result carriers (i16/i8) cannot be
// legalized by the scalar backend. Do not reintroduce v.cvt.* here.
//
// All inline-asm casts below use 64-bit integer carriers ("r" constraint on
// long) so the asm operands keep legal scalar types end to end; narrowing to
// the struct storage happens in plain C++ afterwards.

// Float<->float through the scalar convert instruction. CLS is the source
// struct type macro prefix, DSTCODE the fcvt destination dtype token.
#define DEFINE_FCVT_OPERATOR(CLS, T, SRCCODE, DSTCODE)                          \
  operator T() {                                                                \
    long res, src;                                                              \
    src = (long)CLS##_STORAGE(*this);                                           \
    asm volatile("fcvt." SRCCODE "2" DSTCODE " %1, -> %0\n" : "=r"(res)         \
                 : "r"(src));                                                   \
    return (T)res;                                                              \
  }

#define DEFINE_FCVT_CONSTRUCT(CLS, T, SRCCODE, DSTCODE)                         \
  CLS(T from) {                                                                 \
    long res, src = (long)from;                                                 \
    asm volatile("fcvt." SRCCODE "2" DSTCODE " %1, -> %0\n" : "=r"(res)         \
                 : "r"(src));                                                   \
    CLS##_STORAGE(*this) = (decltype(CLS##_STORAGE(*this)))res;                 \
  }

// Integer<->float through the scalar convert instructions. The
// float-to-integer direction uses the round-to-nearest FCVTN family;
// integer-to-float uses SCVTF/UCVTF.
#define DEFINE_FCVT_INT_OPERATOR(CLS, T, SRCCODE, DSTCODE)                      \
  operator T() {                                                                \
    long res, src;                                                              \
    src = (long)CLS##_STORAGE(*this);                                           \
    asm volatile("fcvtn." SRCCODE "2" DSTCODE " %1, -> %0\n" : "=r"(res)        \
                 : "r"(src));                                                   \
    return (T)res;                                                              \
  }

#define DEFINE_SCVTF_CONSTRUCT(CLS, T, SRCCODE, DSTCODE)                        \
  CLS(T from) {                                                                 \
    long res, src = (long)from;                                                 \
    asm volatile("scvtf." SRCCODE "2" DSTCODE " %1, -> %0\n" : "=r"(res)        \
                 : "r"(src));                                                   \
    CLS##_STORAGE(*this) = (decltype(CLS##_STORAGE(*this)))res;                 \
  }

// One (source dtype -> struct dtype) conversion pair expressed with the
// scalar fcvt instruction family. Used both for C-style construction
// (T -> CLS) and the conversion operator (CLS -> T) when both sides have
// scalar float encodings.
#define DEFINE_SCALAR_CAST_PAIR(CLS, T, SRCCODE, DSTCODE)                       \
  DEFINE_SCVTF_CONSTRUCT(CLS, T, SRCCODE, DSTCODE)                              \
  DEFINE_FCVT_OPERATOR(CLS, T, SRCCODE, DSTCODE)

#define DEFINE_SIMPLE_CASTS(CLS)                                               \
  DEFINE_SCALAR_CAST_PAIR(CLS, double, "fd", CLS##_FCVT)                        \
  DEFINE_SCALAR_CAST_PAIR(CLS, float, "fs", CLS##_FCVT)                         \
  DEFINE_SCALAR_CAST_PAIR(CLS, __half, "fh", CLS##_FCVT)                        \
  DEFINE_FCVT_INT_OPERATOR(CLS, long, CLS##_FCVT, "sd")                         \
  DEFINE_FCVT_INT_OPERATOR(CLS, int, CLS##_FCVT, "sw")                          \
  DEFINE_FCVT_INT_OPERATOR(CLS, short, CLS##_FCVT, "sh")                        \
  DEFINE_FCVT_INT_OPERATOR(CLS, char, CLS##_FCVT, "sb")                         \
  DEFINE_FCVT_INT_OPERATOR(CLS, unsigned long, CLS##_FCVT, "ud")                \
  DEFINE_FCVT_INT_OPERATOR(CLS, unsigned int, CLS##_FCVT, "uw")                 \
  DEFINE_FCVT_INT_OPERATOR(CLS, unsigned short, CLS##_FCVT, "uh")               \
  DEFINE_FCVT_INT_OPERATOR(CLS, unsigned char, CLS##_FCVT, "ub")                \
  DEFINE_INT_CONSTRUCTS(CLS)

// Integer -> float struct: SCVTF/UCVTF per signedness; the from-int
// constructors share one implementation because the carrier is long.
#define DEFINE_INT_CONSTRUCT(CLS, T, SRCCODE)                                   \
  CLS(T from) {                                                                 \
    long res, src = (long)from;                                                 \
    asm volatile("scvtf." SRCCODE "2" CLS##_FCVT " %1, -> %0\n" : "=r"(res)     \
                 : "r"(src));                                                   \
    CLS##_STORAGE(*this) = (decltype(CLS##_STORAGE(*this)))res;                 \
  }

#define DEFINE_INT_CONSTRUCTS(CLS)                                              \
  DEFINE_INT_CONSTRUCT(CLS, long, "sd")                                         \
  DEFINE_INT_CONSTRUCT(CLS, int, "sw")                                          \
  DEFINE_INT_CONSTRUCT(CLS, short, "sh")                                        \
  DEFINE_INT_CONSTRUCT(CLS, char, "sb")                                         \
  DEFINE_INT_CONSTRUCT(CLS, unsigned long, "ud")                                \
  DEFINE_INT_CONSTRUCT(CLS, unsigned int, "uw")                                 \
  DEFINE_INT_CONSTRUCT(CLS, unsigned short, "uh")                               \
  DEFINE_INT_CONSTRUCT(CLS, unsigned char, "ub")

#define DEFINE_SPECIAL_CAST(ME, OTHER)                                         \
  ME(OTHER from) {                                                             \
    OTHER##_STORAGE(*this) = OTHER##_STORAGE(from);                            \
  }                                                                            \
  operator OTHER() {                                                           \
    OTHER res;                                                                 \
    OTHER##_STORAGE(res) = OTHER##_STORAGE(*this);                             \
    return res;                                                                \
  }

struct __fp8_e4m3 : public __fp8_base {
public:
  __fp8_e4m3() = default;

#define __fp8_e4m3_TYPE "e4m3"
#define __fp8_e4m3_MAJOR_TYPE "fb"
#define __fp8_e4m3_WIDTH "b"
#define __fp8_e4m3_FCVT "fb"
#define __fp8_e4m3_STORAGE(d) ((d).data)

  DEFINE_SIMPLE_CASTS(__fp8_e4m3)
};

struct __fp8_e5m2 : public __fp8_base {
  __fp8_e5m2() = default;

#define __fp8_e5m2_TYPE "e5m2"
#define __fp8_e5m2_MAJOR_TYPE "fb"
#define __fp8_e5m2_WIDTH "b"
#define __fp8_e5m2_STORAGE(d) ((d).data)

  // Scalar casts disabled for __fp8_e5m2: the scalar FCVT family has no encoding
  // for this dtype and the retired v.cvt.* forms must not be used in block
  // mode. Use the TileOP tile-level conversion for data movement.
  // DEFINE_SIMPLE_CASTS(__fp8_e5m2)
  // DEFINE_SPECIAL_CAST(__fp8_e5m2, __fp8_e4m3)  // no scalar encoding; use tile-level conversion
};

struct __blkc_bf16 : public __bf16_base {
  __blkc_bf16() = default;

#define __blkc_bf16_TYPE "bf16"
#define __blkc_bf16_MAJOR_TYPE "fh"
#define __blkc_bf16_WIDTH "h"
#define __blkc_bf16_STORAGE(d) ((d).data)

  // Scalar BF16 has no FCVT encoding; convert in software through the
  // FP32 bit pattern (BF16 is the high half of FP32, round-to-nearest via
  // the lsb carry trick for fp32->bf16).
  __blkc_bf16(float from) {
    unsigned u;
    __builtin_memcpy(&u, &from, sizeof u);
    u += 0x7fffU + ((u >> 16) & 1U);      // round to nearest even
    data = (short)(u >> 16);
  }
  __blkc_bf16(double from) : __blkc_bf16((float)from) {}
  __blkc_bf16(__half from) : __blkc_bf16((float)from) {}
  operator float() {
    unsigned u = (unsigned short)data;
    u <<= 16;
    float f;
    __builtin_memcpy(&f, &u, sizeof f);
    return f;
  }
  operator double() { return (double)(float)*this; }
  operator __half() { return (__half)(float)*this; }
  explicit operator long() { return (long)(float)*this; }
  explicit operator int() { return (int)(float)*this; }
  explicit operator short() { return (short)(float)*this; }
  explicit operator char() { return (char)(float)*this; }
  explicit operator unsigned long() { return (unsigned long)(float)*this; }
  explicit operator unsigned int() { return (unsigned int)(float)*this; }
  explicit operator unsigned short() { return (unsigned short)(float)*this; }
  explicit operator unsigned char() { return (unsigned char)(float)*this; }
};

struct __tf32 : public __fp32_base {
public:
  __tf32() = default;

#define __tf32_TYPE "tf32"
#define __tf32_MAJOR_TYPE "fs"
#define __tf32_WIDTH "w"
#define __tf32_FCVT "fs"
#define __tf32_STORAGE(d) ((d).data)

  // TF32 shares the FP32 storage; scalar casts pass the value through
  // unchanged (no instruction needed; the dtype matters only for tile ops).
  __tf32(float from) { data = from; }
  __tf32(double from) : __tf32((float)from) {}
  __tf32(__half from) : __tf32((float)from) {}
  operator float() { return data; }
  operator double() { return (double)data; }
  operator __half() { return (__half)data; }
  explicit operator long() { return (long)data; }
  explicit operator int() { return (int)data; }
  explicit operator short() { return (short)data; }
  explicit operator char() { return (char)data; }
  explicit operator unsigned long() { return (unsigned long)data; }
  explicit operator unsigned int() { return (unsigned int)data; }
  explicit operator unsigned short() { return (unsigned short)data; }
  explicit operator unsigned char() { return (unsigned char)data; }
};

struct __hf32 : public __fp32_base {
public:
  __hf32() = default;

#define __hf32_TYPE "hf32"
#define __hf32_MAJOR_TYPE "fs"
#define __hf32_WIDTH "w"
#define __hf32_FCVT "fs"
#define __hf32_STORAGE(d) ((d).data)

  // HF32 shares the FP32 storage; scalar casts pass the value through
  // unchanged (no instruction needed; the dtype matters only for tile ops).
  __hf32(float from) { data = from; }
  __hf32(double from) : __hf32((float)from) {}
  __hf32(__half from) : __hf32((float)from) {}
  operator float() { return data; }
  operator double() { return (double)data; }
  operator __half() { return (__half)data; }
  explicit operator long() { return (long)data; }
  explicit operator int() { return (int)data; }
  explicit operator short() { return (short)data; }
  explicit operator char() { return (char)data; }
  explicit operator unsigned long() { return (unsigned long)data; }
  explicit operator unsigned int() { return (unsigned int)data; }
  explicit operator unsigned short() { return (unsigned short)data; }
  explicit operator unsigned char() { return (unsigned char)data; }
};

struct __hif8 : public __fp8_base {
public:
  __hif8() = default;

#define __hif8_TYPE "hif8"
#define __hif8_MAJOR_TYPE "fb"
#define __hif8_WIDTH "b"
#define __hif8_STORAGE(d) ((d).data)

  // Scalar casts disabled for __hif8: the scalar FCVT family has no encoding
  // for this dtype and the retired v.cvt.* forms must not be used in block
  // mode. Use the TileOP tile-level conversion for data movement.
  // DEFINE_SIMPLE_CASTS(__hif8)
  // DEFINE_SPECIAL_CAST(__hif8, __fp8_e4m3)  // no scalar encoding; use tile-level conversion
  // DEFINE_SPECIAL_CAST(__hif8, __fp8_e5m2)  // no scalar encoding; use tile-level conversion
};

struct __fp8_e8m0 : public __fp8_base {
public:
  __fp8_e8m0() = default;

#define __fp8_e8m0_TYPE "e8m0"
#define __fp8_e8m0_MAJOR_TYPE "fb"
#define __fp8_e8m0_WIDTH "b"
#define __fp8_e8m0_STORAGE(d) ((d).data)

  // Scalar casts disabled for __fp8_e8m0: the scalar FCVT family has no encoding
  // for this dtype and the retired v.cvt.* forms must not be used in block
  // mode. Use the TileOP tile-level conversion for data movement.
  // DEFINE_SIMPLE_CASTS(__fp8_e8m0)
  // DEFINE_SPECIAL_CAST(__fp8_e8m0, __fp8_e4m3)  // no scalar encoding; use tile-level conversion
  // DEFINE_SPECIAL_CAST(__fp8_e8m0, __fp8_e5m2)  // no scalar encoding; use tile-level conversion
};

struct __fp8_e6m2 : public __fp8_base {
public:
  __fp8_e6m2() = default;

#define __fp8_e6m2_TYPE "e6m2"
#define __fp8_e6m2_MAJOR_TYPE "fb"
#define __fp8_e6m2_WIDTH "b"
#define __fp8_e6m2_STORAGE(d) ((d).data)

  // Scalar casts disabled for __fp8_e6m2: the scalar FCVT family has no encoding
  // for this dtype and the retired v.cvt.* forms must not be used in block
  // mode. Use the TileOP tile-level conversion for data movement.
  // DEFINE_SIMPLE_CASTS(__fp8_e6m2)
  // DEFINE_SPECIAL_CAST(__fp8_e6m2, __fp8_e4m3)  // no scalar encoding; use tile-level conversion
  // DEFINE_SPECIAL_CAST(__fp8_e6m2, __fp8_e5m2)  // no scalar encoding; use tile-level conversion
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
  // DEFINE_SPECIAL_CAST(__fp4_e1m2x2, __fp4_e2m1x2)  // no scalar encoding; use tile-level conversion
};

struct __fp4_hif4x2 : public __fp8_base {
public:
  __fp4_hif4x2() = default;

#define __fp4_hif4x2_TYPE "hif4x2"
#define __fp4_hif4x2_MAJOR_TYPE "fb"
#define __fp4_hif4x2_WIDTH "b"
#define __fp4_hif4x2_STORAGE(d) ((d).data)

  // DEFINE_SIMPLE_CASTS(__fp4_hif4x2)
  // DEFINE_SPECIAL_CAST(__fp4_hif4x2, __fp4_e2m1x2)  // no scalar encoding; use tile-level conversion
  // DEFINE_SPECIAL_CAST(__fp4_hif4x2, __fp4_e1m2x2)  // no scalar encoding; use tile-level conversion
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
  // Scalar casts disabled for __fp6_e3m2: the scalar FCVT family has no encoding
  // for this dtype and the retired v.cvt.* forms must not be used in block
  // mode. Use the TileOP tile-level conversion for data movement.
  // DEFINE_SIMPLE_CASTS(__fp6_e3m2)
  // DEFINE_SPECIAL_CAST(__fp6_e3m2, __fp8_e4m3)  // no scalar encoding; use tile-level conversion
  // DEFINE_SPECIAL_CAST(__fp6_e3m2, __fp8_e5m2)  // no scalar encoding; use tile-level conversion
};

struct __fp6_e2m3 : public __fp8_base {
public:
  __fp6_e2m3() = default;
#define __fp6_e2m3_TYPE "e2m3"
#define __fp6_e2m3_MAJOR_TYPE "fb"
#define __fp6_e2m3_WIDTH "b"
#define __fp6_e2m3_STORAGE(d) ((d).data)
  // Scalar casts disabled for __fp6_e2m3: the scalar FCVT family has no encoding
  // for this dtype and the retired v.cvt.* forms must not be used in block
  // mode. Use the TileOP tile-level conversion for data movement.
  // DEFINE_SIMPLE_CASTS(__fp6_e2m3)
  // DEFINE_SPECIAL_CAST(__fp6_e2m3, __fp8_e4m3)  // no scalar encoding; use tile-level conversion
  // DEFINE_SPECIAL_CAST(__fp6_e2m3, __fp8_e5m2)  // no scalar encoding; use tile-level conversion
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
