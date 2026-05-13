//===- Support/MachineValueType.h - Machine-Level types ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the set of machine-level target independent types which
// legal values in the code generator use.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_MACHINEVALUETYPE_H
#define LLVM_SUPPORT_MACHINEVALUETYPE_H

#include "llvm/ADT/Sequence.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/TypeSize.h"
#include <cassert>

namespace llvm {

  class Type;

  /// Machine Value Type. Every type that is supported natively by some
  /// processor targeted by LLVM occurs here. This means that any legal value
  /// type can be represented by an MVT.
  class MVT {
  public:
    enum SimpleValueType : uint16_t {
      // clang-format off

      // Simple value types that aren't explicitly part of this enumeration
      // are considered extended value types.
      INVALID_SIMPLE_VALUE_TYPE = 0,

      // If you change this numbering, you must change the values in
      // ValueTypes.td as well!
      Other          =   1,   // This is a non-standard value
      i1             =   2,   // This is a 1 bit integer value
      i2             =   3,   // This is a 2 bit integer value
      i4             =   4,   // This is a 4 bit integer value
      i8             =   5,   // This is an 8 bit integer value
      i16            =   6,   // This is a 16 bit integer value
      i32            =   7,   // This is a 32 bit integer value
      i64            =   8,   // This is a 64 bit integer value
      i128           =   9,   // This is a 128 bit integer value

      FIRST_INTEGER_VALUETYPE = i1,
      LAST_INTEGER_VALUETYPE  = i128,

      bf16           =  10,   // This is a 16 bit brain floating point value
      f16            =  11,   // This is a 16 bit floating point value
      f32            =  12,   // This is a 32 bit floating point value
      f64            =  13,   // This is a 64 bit floating point value
      f80            =  14,   // This is a 80 bit floating point value
      f128           =  15,   // This is a 128 bit floating point value
      ppcf128        =  16,   // This is a PPC 128-bit floating point value

      FIRST_FP_VALUETYPE = bf16,
      LAST_FP_VALUETYPE  = ppcf128,

      v1i1           =  17,   //    1 x i1
      v2i1           =  18,   //    2 x i1
      v4i1           =  19,   //    4 x i1
      v8i1           =  20,   //    8 x i1
      v16i1          =  21,   //   16 x i1
      v32i1          =  22,   //   32 x i1
      v64i1          =  23,   //   64 x i1
      v128i1         =  24,   //  128 x i1
      v256i1         =  25,   //  256 x i1
      v512i1         =  26,   //  512 x i1
      v1024i1        =  27,   // 1024 x i1

      v128i2         =  28,   //  128 x i2

      v64i4          =  29,   //   64 x i4

      v1i8           =  30,   //    1 x i8
      v2i8           =  31,   //    2 x i8
      v4i8           =  32,   //    4 x i8
      v8i8           =  33,   //    8 x i8
      v16i8          =  34,   //   16 x i8
      v32i8          =  35,   //   32 x i8
      v64i8          =  36,   //   64 x i8
      v128i8         =  37,   //  128 x i8
      v256i8         =  38,   //  256 x i8
      v512i8         =  39,   //  512 x i8
      v1024i8        =  40,   // 1024 x i8
      v2048i8        =  41,   // 2048 x i8
      v4096i8        =  42,   // 4096 x i8
      v8192i8        =  43,   // 8192 x i8
      v16384i8       =  44,   // 16384 x i8
      v32768i8       =  45,   // 32768 x i8
      v65536i8       =  46,   // 65536 x i8

      v1i16          =  47,   //   1 x i16
      v2i16          =  48,   //   2 x i16
      v3i16          =  49,   //   3 x i16
      v4i16          =  50,   //   4 x i16
      v8i16          =  51,   //   8 x i16
      v16i16         =  52,   //  16 x i16
      v32i16         =  53,   //  32 x i16
      v64i16         =  54,   //  64 x i16
      v128i16        =  55,   // 128 x i16
      v256i16        =  56,   // 256 x i16
      v512i16        =  57,   // 512 x i16
      v1024i16       =  58,   // 1024 x i16
      v2048i16       =  59,   // 2048 x i16
      v4096i16       =  60,   // 4096 x i16
      v8192i16       =  61,   // 8192 x i16
      v16384i16      =  62,   // 16384 x i16
      v32768i16      =  63,   // 32768 x i16
      v65536i16      =  64,   // 65536 x i16

      v1i32          =  65,   //    1 x i32
      v2i32          =  66,   //    2 x i32
      v3i32          =  67,   //    3 x i32
      v4i32          =  68,   //    4 x i32
      v5i32          =  69,   //    5 x i32
      v6i32          =  70,   //    6 x i32
      v7i32          =  71,   //    7 x i32
      v8i32          =  72,   //    8 x i32
      v16i32         =  73,   //   16 x i32
      v32i32         =  74,   //   32 x i32
      v64i32         =  75,   //   64 x i32
      v128i32        =  76,   //  128 x i32
      v256i32        =  77,   //  256 x i32
      v512i32        =  78,   //  512 x i32
      v1024i32       =  79,   // 1024 x i32
      v2048i32       =  80,   // 2048 x i32
      v4096i32       =  81,   // 4096 x i32
      v8192i32       =  82,   // 8192 x i32
      v16384i32      =  83,   // 16384 x i32
      v32768i32      =  84,   // 32768 x i32
      v65536i32      =  85,   // 65536 x i32

      v1i64          =  86,   //   1 x i64
      v2i64          =  87,   //   2 x i64
      v3i64          =  88,   //   3 x i64
      v4i64          =  89,   //   4 x i64
      v8i64          =  90,   //   8 x i64
      v16i64         =  91,   //  16 x i64
      v32i64         =  92,   //  32 x i64
      v64i64         =  93,   //  64 x i64
      v128i64        =  94,   // 128 x i64
      v256i64        =  95,   // 256 x i64
      v512i64        =  96,   // 512 x i64
      v1024i64       =  97,   // 1024 x i64
      v2048i64       =  98,   // 2048 x i64
      v4096i64       =  99,   // 4096 x i64
      v8192i64       =  100,  // 8192 x i64
      v16384i64      =  101,  // 16384 x i64
      v32768i64      =  102,  // 32768 x i64
      v65536i64      =  103,  // 65536 x i64

      v1i128         =  104,   //  1 x i128

      FIRST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE = v1i1,
      LAST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE = v1i128,

      v1f16          =  105,   //    1 x f16
      v2f16          =  106,   //    2 x f16
      v3f16          =  107,   //    3 x f16
      v4f16          =  108,   //    4 x f16
      v8f16          =  109,   //    8 x f16
      v16f16         =  110,   //   16 x f16
      v32f16         =  111,   //   32 x f16
      v64f16         =  112,   //   64 x f16
      v128f16        =  113,   //  128 x f16
      v256f16        =  114,   //  256 x f16
      v512f16        =  115,   //  256 x f16
      v1024f16       =  116,   //  1024 x f16
      v2048f16       =  117,   //  2048 x f16
      v4096f16       =  118,   //  4096 x f16
      v8192f16       =  119,   //  8192 x f16
      v16384f16      =  120,   //  16384 x f16
      v32768f16      =  121,   //  32768 x f16
      v65536f16      =  122,   //  65536 x f16

      v2bf16         =  123,   //    2 x bf16
      v3bf16         =  124,   //    3 x bf16
      v4bf16         =  125,   //    4 x bf16
      v8bf16         =  126,   //    8 x bf16
      v16bf16        =  127,   //   16 x bf16
      v32bf16        =  128,   //   32 x bf16
      v64bf16        =  129,   //   64 x bf16
      v128bf16       =  130,   //  128 x bf16
      v256bf16       =  131,   //  256 x bf16
      v512bf16       =  132,   //  512 x bf16
      v1024bf16      =  133,   //  1024 x bf16
      v2048bf16      =  134,   //  2048 x bf16
      v4096bf16      =  135,   //  4096 x bf16
      v8192bf16      =  136,   //  8192 x bf16
      v16384bf16     =  137,   //  16384 x bf16
      v32768bf16     =  138,   //  32768 x bf16
      v65536bf16     =  139,   //  65536 x bf16

      v1f32          = 140,   //    1 x f32
      v2f32          = 141,   //    2 x f32
      v3f32          = 142,   //    3 x f32
      v4f32          = 143,   //    4 x f32
      v5f32          = 144,   //    5 x f32
      v6f32          = 145,   //    6 x f32
      v7f32          = 146,   //    7 x f32
      v8f32          = 147,   //    8 x f32
      v16f32         = 148,   //   16 x f32
      v32f32         = 149,   //   32 x f32
      v64f32         = 150,   //   64 x f32
      v128f32        = 151,   //  128 x f32
      v256f32        = 152,   //  256 x f32
      v512f32        = 153,   //  512 x f32
      v1024f32       = 154,   // 1024 x f32
      v2048f32       = 155,   // 2048 x f32
      v4096f32       = 156,   // 4096 x f32
      v8192f32       = 157,   // 8192 x f32
      v16384f32      = 158,   // 16384 x f32
      v32768f32      = 159,   // 32768 x f32
      v65536f32      = 160,   // 65536 x f32

      v1f64          = 161,   //    1 x f64
      v2f64          = 162,   //    2 x f64
      v3f64          = 163,   //    3 x f64
      v4f64          = 164,   //    4 x f64
      v8f64          = 165,   //    8 x f64
      v16f64         = 166,   //   16 x f64
      v32f64         = 167,   //   32 x f64
      v64f64         = 168,   //   64 x f64
      v128f64        = 169,   //  128 x f64
      v256f64        = 170,   //  256 x f64
      v512f64        = 171,   //  512 x f64
      v1024f64       = 172,   //  1024 x f64
      v2048f64       = 173,   //  2048 x f64
      v4096f64       = 174,   //  4096 x f64
      v8192f64       = 175,   //  8192 x f64
      v16384f64      = 176,   //  16384 x f64
      v32768f64      = 177,   //  32768 x f64
      v65536f64      = 178,   //  65536 x f64

      FIRST_FP_FIXEDLEN_VECTOR_VALUETYPE = v1f16,
      LAST_FP_FIXEDLEN_VECTOR_VALUETYPE = v65536f64,

      FIRST_FIXEDLEN_VECTOR_VALUETYPE = v1i1,
      LAST_FIXEDLEN_VECTOR_VALUETYPE = v65536f64,

      nxv1i1         = 179,   // n x  1 x i1
      nxv2i1         = 180,   // n x  2 x i1
      nxv4i1         = 181,   // n x  4 x i1
      nxv8i1         = 182,   // n x  8 x i1
      nxv16i1        = 183,   // n x 16 x i1
      nxv32i1        = 184,   // n x 32 x i1
      nxv64i1        = 185,   // n x 64 x i1

      nxv1i8         = 186,   // n x  1 x i8
      nxv2i8         = 187,   // n x  2 x i8
      nxv4i8         = 188,   // n x  4 x i8
      nxv8i8         = 189,   // n x  8 x i8
      nxv16i8        = 190,   // n x 16 x i8
      nxv32i8        = 191,   // n x 32 x i8
      nxv64i8        = 192,   // n x 64 x i8

      nxv1i16        = 193,  // n x  1 x i16
      nxv2i16        = 194,  // n x  2 x i16
      nxv4i16        = 195,  // n x  4 x i16
      nxv8i16        = 196,  // n x  8 x i16
      nxv16i16       = 197,  // n x 16 x i16
      nxv32i16       = 198,  // n x 32 x i16

      nxv1i32        = 199,  // n x  1 x i32
      nxv2i32        = 200,  // n x  2 x i32
      nxv4i32        = 201,  // n x  4 x i32
      nxv8i32        = 202,  // n x  8 x i32
      nxv16i32       = 203,  // n x 16 x i32
      nxv32i32       = 204,  // n x 32 x i32

      nxv1i64        = 205,  // n x  1 x i64
      nxv2i64        = 206,  // n x  2 x i64
      nxv4i64        = 207,  // n x  4 x i64
      nxv8i64        = 208,  // n x  8 x i64
      nxv16i64       = 209,  // n x 16 x i64
      nxv32i64       = 210,  // n x 32 x i64

      FIRST_INTEGER_SCALABLE_VECTOR_VALUETYPE = nxv1i1,
      LAST_INTEGER_SCALABLE_VECTOR_VALUETYPE = nxv32i64,

      nxv1f16        = 211,  // n x  1 x f16
      nxv2f16        = 212,  // n x  2 x f16
      nxv4f16        = 213,  // n x  4 x f16
      nxv8f16        = 214,  // n x  8 x f16
      nxv16f16       = 215,  // n x 16 x f16
      nxv32f16       = 216,  // n x 32 x f16

      nxv1bf16       = 217,  // n x  1 x bf16
      nxv2bf16       = 218,  // n x  2 x bf16
      nxv4bf16       = 219,  // n x  4 x bf16
      nxv8bf16       = 220,  // n x  8 x bf16
      nxv16bf16      = 221,  // n x 16 x bf16
      nxv32bf16      = 222,  // n x 32 x bf16

      nxv1f32        = 223,  // n x  1 x f32
      nxv2f32        = 224,  // n x  2 x f32
      nxv4f32        = 225,  // n x  4 x f32
      nxv8f32        = 226,  // n x  8 x f32
      nxv16f32       = 227,  // n x 16 x f32

      nxv1f64        = 228,  // n x  1 x f64
      nxv2f64        = 229,  // n x  2 x f64
      nxv4f64        = 230,  // n x  4 x f64
      nxv8f64        = 231,  // n x  8 x f64

      FIRST_FP_SCALABLE_VECTOR_VALUETYPE = nxv1f16,
      LAST_FP_SCALABLE_VECTOR_VALUETYPE = nxv8f64,

      FIRST_SCALABLE_VECTOR_VALUETYPE = nxv1i1,
      LAST_SCALABLE_VECTOR_VALUETYPE = nxv8f64,

      FIRST_VECTOR_VALUETYPE = v1i1,
      LAST_VECTOR_VALUETYPE  = nxv8f64,

      x86mmx         = 232,    // This is an X86 MMX value

      Glue           = 233,    // This glues nodes together during pre-RA sched

      isVoid         = 234,    // This has no value

      Untyped        = 235,    // This value takes a register, but has
                               // unspecified type.  The register class
                               // will be determined by the opcode.

      funcref        = 236,    // WebAssembly's funcref type
      externref      = 237,    // WebAssembly's externref type
      x86amx         = 238,    // This is an X86 AMX value
      i64x8          = 239,    // 8 Consecutive GPRs (AArch64)

      FIRST_VALUETYPE =  1,    // This is always the beginning of the list.
      LAST_VALUETYPE = i64x8,  // This always remains at the end of the list.
      VALUETYPE_SIZE = LAST_VALUETYPE + 1,

      // This is the current maximum for LAST_VALUETYPE.
      // MVT::MAX_ALLOWED_VALUETYPE is used for asserts and to size bit vectors
      // This value must be a multiple of 32.
      MAX_ALLOWED_VALUETYPE = 256,

      // A value of type llvm::TokenTy
      token          = 248,

      // This is MDNode or MDString.
      Metadata       = 249,

      // An int value the size of the pointer of the current
      // target to any address space. This must only be used internal to
      // tblgen. Other than for overloading, we treat iPTRAny the same as iPTR.
      iPTRAny        = 250,

      // A vector with any length and element size. This is used
      // for intrinsics that have overloadings based on vector types.
      // This is only for tblgen's consumption!
      vAny           = 251,

      // Any floating-point or vector floating-point value. This is used
      // for intrinsics that have overloadings based on floating-point types.
      // This is only for tblgen's consumption!
      fAny           = 252,

      // An integer or vector integer value of any bit width. This is
      // used for intrinsics that have overloadings based on integer bit widths.
      // This is only for tblgen's consumption!
      iAny           = 253,

      // An int value the size of the pointer of the current
      // target.  This should only be used internal to tblgen!
      iPTR           = 254,

      // Any type. This is used for intrinsics that have overloadings.
      // This is only for tblgen's consumption!
      Any            = 255

      // clang-format on
    };

    SimpleValueType SimpleTy = INVALID_SIMPLE_VALUE_TYPE;

    constexpr MVT() = default;
    constexpr MVT(SimpleValueType SVT) : SimpleTy(SVT) {}

    bool operator>(const MVT& S)  const { return SimpleTy >  S.SimpleTy; }
    bool operator<(const MVT& S)  const { return SimpleTy <  S.SimpleTy; }
    bool operator==(const MVT& S) const { return SimpleTy == S.SimpleTy; }
    bool operator!=(const MVT& S) const { return SimpleTy != S.SimpleTy; }
    bool operator>=(const MVT& S) const { return SimpleTy >= S.SimpleTy; }
    bool operator<=(const MVT& S) const { return SimpleTy <= S.SimpleTy; }

    /// Return true if this is a valid simple valuetype.
    bool isValid() const {
      return (SimpleTy >= MVT::FIRST_VALUETYPE &&
              SimpleTy <= MVT::LAST_VALUETYPE);
    }

    /// Return true if this is a FP or a vector FP type.
    bool isFloatingPoint() const {
      return ((SimpleTy >= MVT::FIRST_FP_VALUETYPE &&
               SimpleTy <= MVT::LAST_FP_VALUETYPE) ||
              (SimpleTy >= MVT::FIRST_FP_FIXEDLEN_VECTOR_VALUETYPE &&
               SimpleTy <= MVT::LAST_FP_FIXEDLEN_VECTOR_VALUETYPE) ||
              (SimpleTy >= MVT::FIRST_FP_SCALABLE_VECTOR_VALUETYPE &&
               SimpleTy <= MVT::LAST_FP_SCALABLE_VECTOR_VALUETYPE));
    }

    /// Return true if this is an integer or a vector integer type.
    bool isInteger() const {
      return ((SimpleTy >= MVT::FIRST_INTEGER_VALUETYPE &&
               SimpleTy <= MVT::LAST_INTEGER_VALUETYPE) ||
              (SimpleTy >= MVT::FIRST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE &&
               SimpleTy <= MVT::LAST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE) ||
              (SimpleTy >= MVT::FIRST_INTEGER_SCALABLE_VECTOR_VALUETYPE &&
               SimpleTy <= MVT::LAST_INTEGER_SCALABLE_VECTOR_VALUETYPE));
    }

    /// Return true if this is an integer, not including vectors.
    bool isScalarInteger() const {
      return (SimpleTy >= MVT::FIRST_INTEGER_VALUETYPE &&
              SimpleTy <= MVT::LAST_INTEGER_VALUETYPE);
    }

    /// Return true if this is a vector value type.
    bool isVector() const {
      return (SimpleTy >= MVT::FIRST_VECTOR_VALUETYPE &&
              SimpleTy <= MVT::LAST_VECTOR_VALUETYPE);
    }

    /// Return true if this is a vector value type where the
    /// runtime length is machine dependent
    bool isScalableVector() const {
      return (SimpleTy >= MVT::FIRST_SCALABLE_VECTOR_VALUETYPE &&
              SimpleTy <= MVT::LAST_SCALABLE_VECTOR_VALUETYPE);
    }

    bool isFixedLengthVector() const {
      return (SimpleTy >= MVT::FIRST_FIXEDLEN_VECTOR_VALUETYPE &&
              SimpleTy <= MVT::LAST_FIXEDLEN_VECTOR_VALUETYPE);
    }

    /// Return true if this is a 16-bit vector type.
    bool is16BitVector() const {
      return (SimpleTy == MVT::v2i8  || SimpleTy == MVT::v1i16 ||
              SimpleTy == MVT::v16i1 || SimpleTy == MVT::v1f16);
    }

    /// Return true if this is a 32-bit vector type.
    bool is32BitVector() const {
      return (SimpleTy == MVT::v32i1 || SimpleTy == MVT::v4i8   ||
              SimpleTy == MVT::v2i16 || SimpleTy == MVT::v1i32  ||
              SimpleTy == MVT::v2f16 || SimpleTy == MVT::v2bf16 ||
              SimpleTy == MVT::v1f32);
    }

    /// Return true if this is a 64-bit vector type.
    bool is64BitVector() const {
      return (SimpleTy == MVT::v64i1  || SimpleTy == MVT::v8i8  ||
              SimpleTy == MVT::v4i16  || SimpleTy == MVT::v2i32 ||
              SimpleTy == MVT::v1i64  || SimpleTy == MVT::v4f16 ||
              SimpleTy == MVT::v4bf16 ||SimpleTy == MVT::v2f32  ||
              SimpleTy == MVT::v1f64);
    }

    /// Return true if this is a 128-bit vector type.
    bool is128BitVector() const {
      return (SimpleTy == MVT::v128i1 || SimpleTy == MVT::v16i8  ||
              SimpleTy == MVT::v8i16  || SimpleTy == MVT::v4i32  ||
              SimpleTy == MVT::v2i64  || SimpleTy == MVT::v1i128 ||
              SimpleTy == MVT::v8f16  || SimpleTy == MVT::v8bf16 ||
              SimpleTy == MVT::v4f32  || SimpleTy == MVT::v2f64);
    }

    /// Return true if this is a 256-bit vector type.
    bool is256BitVector() const {
      return (SimpleTy == MVT::v16f16 || SimpleTy == MVT::v16bf16 ||
              SimpleTy == MVT::v8f32 || SimpleTy == MVT::v4f64 ||
              SimpleTy == MVT::v32i8 || SimpleTy == MVT::v16i16 ||
              SimpleTy == MVT::v8i32 || SimpleTy == MVT::v4i64 ||
              SimpleTy == MVT::v256i1 || SimpleTy == MVT::v128i2 ||
              SimpleTy == MVT::v64i4);
    }

    /// Return true if this is a 512-bit vector type.
    bool is512BitVector() const {
      return (SimpleTy == MVT::v32f16 || SimpleTy == MVT::v32bf16 ||
              SimpleTy == MVT::v16f32 || SimpleTy == MVT::v8f64   ||
              SimpleTy == MVT::v512i1 || SimpleTy == MVT::v64i8   ||
              SimpleTy == MVT::v32i16 || SimpleTy == MVT::v16i32  ||
              SimpleTy == MVT::v8i64);
    }

    /// Return true if this is a 1024-bit vector type.
    bool is1024BitVector() const {
      return (SimpleTy == MVT::v1024i1 || SimpleTy == MVT::v128i8 ||
              SimpleTy == MVT::v64i16  || SimpleTy == MVT::v32i32 ||
              SimpleTy == MVT::v16i64  || SimpleTy == MVT::v64f16 ||
              SimpleTy == MVT::v32f32  || SimpleTy == MVT::v16f64 ||
              SimpleTy == MVT::v64bf16);
    }

    /// Return true if this is a 2048-bit vector type.
    bool is2048BitVector() const {
      return (SimpleTy == MVT::v256i8  || SimpleTy == MVT::v128i16 ||
              SimpleTy == MVT::v64i32  || SimpleTy == MVT::v32i64  ||
              SimpleTy == MVT::v128f16 || SimpleTy == MVT::v64f32  ||
              SimpleTy == MVT::v32f64  || SimpleTy == MVT::v128bf16);
    }

    /// Return true if this is an overloaded type for TableGen.
    bool isOverloaded() const {
      return (SimpleTy == MVT::Any || SimpleTy == MVT::iAny ||
              SimpleTy == MVT::fAny || SimpleTy == MVT::vAny ||
              SimpleTy == MVT::iPTRAny);
    }

    /// Return a vector with the same number of elements as this vector, but
    /// with the element type converted to an integer type with the same
    /// bitwidth.
    MVT changeVectorElementTypeToInteger() const {
      MVT EltTy = getVectorElementType();
      MVT IntTy = MVT::getIntegerVT(EltTy.getSizeInBits());
      MVT VecTy = MVT::getVectorVT(IntTy, getVectorElementCount());
      assert(VecTy.SimpleTy != MVT::INVALID_SIMPLE_VALUE_TYPE &&
             "Simple vector VT not representable by simple integer vector VT!");
      return VecTy;
    }

    /// Return a VT for a vector type whose attributes match ourselves
    /// with the exception of the element type that is chosen by the caller.
    MVT changeVectorElementType(MVT EltVT) const {
      MVT VecTy = MVT::getVectorVT(EltVT, getVectorElementCount());
      assert(VecTy.SimpleTy != MVT::INVALID_SIMPLE_VALUE_TYPE &&
             "Simple vector VT not representable by simple integer vector VT!");
      return VecTy;
    }

    /// Return the type converted to an equivalently sized integer or vector
    /// with integer element type. Similar to changeVectorElementTypeToInteger,
    /// but also handles scalars.
    MVT changeTypeToInteger() {
      if (isVector())
        return changeVectorElementTypeToInteger();
      return MVT::getIntegerVT(getSizeInBits());
    }

    /// Return a VT for a vector type with the same element type but
    /// half the number of elements.
    MVT getHalfNumVectorElementsVT() const {
      MVT EltVT = getVectorElementType();
      auto EltCnt = getVectorElementCount();
      assert(EltCnt.isKnownEven() && "Splitting vector, but not in half!");
      return getVectorVT(EltVT, EltCnt.divideCoefficientBy(2));
    }

    /// Returns true if the given vector is a power of 2.
    bool isPow2VectorType() const {
      unsigned NElts = getVectorMinNumElements();
      return !(NElts & (NElts - 1));
    }

    /// Widens the length of the given vector MVT up to the nearest power of 2
    /// and returns that type.
    MVT getPow2VectorType() const {
      if (isPow2VectorType())
        return *this;

      ElementCount NElts = getVectorElementCount();
      unsigned NewMinCount = 1 << Log2_32_Ceil(NElts.getKnownMinValue());
      NElts = ElementCount::get(NewMinCount, NElts.isScalable());
      return MVT::getVectorVT(getVectorElementType(), NElts);
    }

    /// If this is a vector, return the element type, otherwise return this.
    MVT getScalarType() const {
      return isVector() ? getVectorElementType() : *this;
    }

    MVT getVectorElementType() const {
      // clang-format off
      switch (SimpleTy) {
      default:
        llvm_unreachable("Not a vector MVT!");
      case v1i1:
      case v2i1:
      case v4i1:
      case v8i1:
      case v16i1:
      case v32i1:
      case v64i1:
      case v128i1:
      case v256i1:
      case v512i1:
      case v1024i1:
      case nxv1i1:
      case nxv2i1:
      case nxv4i1:
      case nxv8i1:
      case nxv16i1:
      case nxv32i1:
      case nxv64i1: return i1;
      case v128i2: return i2;
      case v64i4: return i4;
      case v1i8:
      case v2i8:
      case v4i8:
      case v8i8:
      case v16i8:
      case v32i8:
      case v64i8:
      case v128i8:
      case v256i8:
      case v512i8:
      case v1024i8:
      case v2048i8:
      case v4096i8:
      case v8192i8:
      case v16384i8:
      case v32768i8:
      case v65536i8:
      case nxv1i8:
      case nxv2i8:
      case nxv4i8:
      case nxv8i8:
      case nxv16i8:
      case nxv32i8:
      case nxv64i8: return i8;
      case v1i16:
      case v2i16:
      case v3i16:
      case v4i16:
      case v8i16:
      case v16i16:
      case v32i16:
      case v64i16:
      case v128i16:
      case v256i16:
      case v512i16:
      case v1024i16:
      case v2048i16:
      case v4096i16:
      case v8192i16:
      case v16384i16:
      case v32768i16:
      case v65536i16:
      case nxv1i16:
      case nxv2i16:
      case nxv4i16:
      case nxv8i16:
      case nxv16i16:
      case nxv32i16: return i16;
      case v1i32:
      case v2i32:
      case v3i32:
      case v4i32:
      case v5i32:
      case v6i32:
      case v7i32:
      case v8i32:
      case v16i32:
      case v32i32:
      case v64i32:
      case v128i32:
      case v256i32:
      case v512i32:
      case v1024i32:
      case v2048i32:
      case v4096i32:
      case v8192i32:
      case v16384i32:
      case v32768i32:
      case v65536i32:
      case nxv1i32:
      case nxv2i32:
      case nxv4i32:
      case nxv8i32:
      case nxv16i32:
      case nxv32i32: return i32;
      case v1i64:
      case v2i64:
      case v3i64:
      case v4i64:
      case v8i64:
      case v16i64:
      case v32i64:
      case v64i64:
      case v128i64:
      case v256i64:
      case v512i64:
      case v1024i64:
      case v2048i64:
      case v4096i64:
      case v8192i64:
      case v16384i64:
      case v32768i64:
      case v65536i64:
      case nxv1i64:
      case nxv2i64:
      case nxv4i64:
      case nxv8i64:
      case nxv16i64:
      case nxv32i64: return i64;
      case v1i128: return i128;
      case v1f16:
      case v2f16:
      case v3f16:
      case v4f16:
      case v8f16:
      case v16f16:
      case v32f16:
      case v64f16:
      case v128f16:
      case v256f16:
      case v512f16:
      case v1024f16:
      case v2048f16:
      case v4096f16:
      case v8192f16:
      case v16384f16:
      case v32768f16:
      case v65536f16:
      case nxv1f16:
      case nxv2f16:
      case nxv4f16:
      case nxv8f16:
      case nxv16f16:
      case nxv32f16: return f16;
      case v2bf16:
      case v3bf16:
      case v4bf16:
      case v8bf16:
      case v16bf16:
      case v32bf16:
      case v64bf16:
      case v128bf16:
      case v256bf16:
      case v512bf16:
      case v1024bf16:
      case v2048bf16:
      case v4096bf16:
      case v8192bf16:
      case v16384bf16:
      case v32768bf16:
      case v65536bf16:
      case nxv1bf16:
      case nxv2bf16:
      case nxv4bf16:
      case nxv8bf16:
      case nxv16bf16:
      case nxv32bf16: return bf16;
      case v1f32:
      case v2f32:
      case v3f32:
      case v4f32:
      case v5f32:
      case v6f32:
      case v7f32:
      case v8f32:
      case v16f32:
      case v32f32:
      case v64f32:
      case v128f32:
      case v256f32:
      case v512f32:
      case v1024f32:
      case v2048f32:
      case v4096f32:
      case v8192f32:
      case v16384f32:
      case v32768f32:
      case v65536f32:
      case nxv1f32:
      case nxv2f32:
      case nxv4f32:
      case nxv8f32:
      case nxv16f32: return f32;
      case v1f64:
      case v2f64:
      case v3f64:
      case v4f64:
      case v8f64:
      case v16f64:
      case v32f64:
      case v64f64:
      case v128f64:
      case v256f64:
      case v512f64:
      case v1024f64:
      case v2048f64:
      case v4096f64:
      case v8192f64:
      case v16384f64:
      case v32768f64:
      case v65536f64:
      case nxv1f64:
      case nxv2f64:
      case nxv4f64:
      case nxv8f64: return f64;
      }
      // clang-format on
    }

    /// Given a vector type, return the minimum number of elements it contains.
    unsigned getVectorMinNumElements() const {
      switch (SimpleTy) {
      default:
        llvm_unreachable("Not a vector MVT!");
      case v65536i8:
      case v65536i16:
      case v65536i32:
      case v65536i64:
      case v65536f16:
      case v65536bf16:
      case v65536f32:
      case v65536f64:
        return 65536;
      case v32768i8:
      case v32768i16:
      case v32768i32:
      case v32768i64:
      case v32768f16:
      case v32768bf16:
      case v32768f32:
      case v32768f64:
        return 32768;
      case v16384i8:
      case v16384i16:
      case v16384i32:
      case v16384i64:
      case v16384f16:
      case v16384bf16:
      case v16384f32:
      case v16384f64:
        return 16384;
      case v8192i8:
      case v8192i16:
      case v8192i32:
      case v8192i64:
      case v8192f16:
      case v8192bf16:
      case v8192f32:
      case v8192f64:
        return 8192;
      case v4096i8:
      case v4096i16:
      case v4096i32:
      case v4096i64:
      case v4096f16:
      case v4096bf16:
      case v4096f32:
      case v4096f64:
        return 4096;
      case v2048i8:
      case v2048i16:
      case v2048i32:
      case v2048i64:
      case v2048f16:
      case v2048bf16:
      case v2048f32:
      case v2048f64:
        return 2048;
      case v1024i1:
      case v1024i8:
      case v1024i16:
      case v1024i32:
      case v1024i64:
      case v1024f16:
      case v1024bf16:
      case v1024f32:
      case v1024f64:
        return 1024;
      case v512i1:
      case v512i8:
      case v512i16:
      case v512i32:
      case v512i64:
      case v512f16:
      case v512bf16:
      case v512f32:
      case v512f64:
        return 512;
      case v256i1:
      case v256i8:
      case v256i16:
      case v256f16:
      case v256i32:
      case v256i64:
      case v256bf16:
      case v256f32:
      case v256f64: return 256;
      case v128i1:
      case v128i2:
      case v128i8:
      case v128i16:
      case v128i32:
      case v128i64:
      case v128f16:
      case v128bf16:
      case v128f32:
      case v128f64: return 128;
      case v64i1:
      case v64i4:
      case v64i8:
      case v64i16:
      case v64i32:
      case v64i64:
      case v64f16:
      case v64bf16:
      case v64f32:
      case v64f64:
      case nxv64i1:
      case nxv64i8: return 64;
      case v32i1:
      case v32i8:
      case v32i16:
      case v32i32:
      case v32i64:
      case v32f16:
      case v32bf16:
      case v32f32:
      case v32f64:
      case nxv32i1:
      case nxv32i8:
      case nxv32i16:
      case nxv32i32:
      case nxv32i64:
      case nxv32f16:
      case nxv32bf16: return 32;
      case v16i1:
      case v16i8:
      case v16i16:
      case v16i32:
      case v16i64:
      case v16f16:
      case v16bf16:
      case v16f32:
      case v16f64:
      case nxv16i1:
      case nxv16i8:
      case nxv16i16:
      case nxv16i32:
      case nxv16i64:
      case nxv16f16:
      case nxv16bf16:
      case nxv16f32: return 16;
      case v8i1:
      case v8i8:
      case v8i16:
      case v8i32:
      case v8i64:
      case v8f16:
      case v8bf16:
      case v8f32:
      case v8f64:
      case nxv8i1:
      case nxv8i8:
      case nxv8i16:
      case nxv8i32:
      case nxv8i64:
      case nxv8f16:
      case nxv8bf16:
      case nxv8f32:
      case nxv8f64: return 8;
      case v7i32:
      case v7f32: return 7;
      case v6i32:
      case v6f32: return 6;
      case v5i32:
      case v5f32: return 5;
      case v4i1:
      case v4i8:
      case v4i16:
      case v4i32:
      case v4i64:
      case v4f16:
      case v4bf16:
      case v4f32:
      case v4f64:
      case nxv4i1:
      case nxv4i8:
      case nxv4i16:
      case nxv4i32:
      case nxv4i64:
      case nxv4f16:
      case nxv4bf16:
      case nxv4f32:
      case nxv4f64: return 4;
      case v3i16:
      case v3i32:
      case v3i64:
      case v3f16:
      case v3bf16:
      case v3f32:
      case v3f64: return 3;
      case v2i1:
      case v2i8:
      case v2i16:
      case v2i32:
      case v2i64:
      case v2f16:
      case v2bf16:
      case v2f32:
      case v2f64:
      case nxv2i1:
      case nxv2i8:
      case nxv2i16:
      case nxv2i32:
      case nxv2i64:
      case nxv2f16:
      case nxv2bf16:
      case nxv2f32:
      case nxv2f64: return 2;
      case v1i1:
      case v1i8:
      case v1i16:
      case v1i32:
      case v1i64:
      case v1i128:
      case v1f16:
      case v1f32:
      case v1f64:
      case nxv1i1:
      case nxv1i8:
      case nxv1i16:
      case nxv1i32:
      case nxv1i64:
      case nxv1f16:
      case nxv1bf16:
      case nxv1f32:
      case nxv1f64: return 1;
      }
    }

    ElementCount getVectorElementCount() const {
      return ElementCount::get(getVectorMinNumElements(), isScalableVector());
    }

    unsigned getVectorNumElements() const {
      if (isScalableVector())
        llvm::reportInvalidSizeRequest(
            "Possible incorrect use of MVT::getVectorNumElements() for "
            "scalable vector. Scalable flag may be dropped, use "
            "MVT::getVectorElementCount() instead");
      return getVectorMinNumElements();
    }

    /// Returns the size of the specified MVT in bits.
    ///
    /// If the value type is a scalable vector type, the scalable property will
    /// be set and the runtime size will be a positive integer multiple of the
    /// base size.
    TypeSize getSizeInBits() const {
      switch (SimpleTy) {
      default:
        llvm_unreachable("getSizeInBits called on extended MVT.");
      case Other:
        llvm_unreachable("Value type is non-standard value, Other.");
      case iPTR:
        llvm_unreachable("Value type size is target-dependent. Ask TLI.");
      case iPTRAny:
      case iAny:
      case fAny:
      case vAny:
      case Any:
        llvm_unreachable("Value type is overloaded.");
      case token:
        llvm_unreachable("Token type is a sentinel that cannot be used "
                         "in codegen and has no size");
      case Metadata:
        llvm_unreachable("Value type is metadata.");
      case i1:
      case v1i1: return TypeSize::Fixed(1);
      case nxv1i1: return TypeSize::Scalable(1);
      case i2:
      case v2i1: return TypeSize::Fixed(2);
      case nxv2i1: return TypeSize::Scalable(2);
      case i4:
      case v4i1: return TypeSize::Fixed(4);
      case nxv4i1: return TypeSize::Scalable(4);
      case i8  :
      case v1i8:
      case v8i1: return TypeSize::Fixed(8);
      case nxv1i8:
      case nxv8i1: return TypeSize::Scalable(8);
      case i16 :
      case f16:
      case bf16:
      case v16i1:
      case v2i8:
      case v1i16:
      case v1f16: return TypeSize::Fixed(16);
      case nxv16i1:
      case nxv2i8:
      case nxv1i16:
      case nxv1bf16:
      case nxv1f16: return TypeSize::Scalable(16);
      case f32 :
      case i32 :
      case v32i1:
      case v4i8:
      case v2i16:
      case v2f16:
      case v2bf16:
      case v1f32:
      case v1i32: return TypeSize::Fixed(32);
      case nxv32i1:
      case nxv4i8:
      case nxv2i16:
      case nxv1i32:
      case nxv2f16:
      case nxv2bf16:
      case nxv1f32: return TypeSize::Scalable(32);
      case v3i16:
      case v3f16:
      case v3bf16: return TypeSize::Fixed(48);
      case x86mmx:
      case f64 :
      case i64 :
      case v64i1:
      case v8i8:
      case v4i16:
      case v2i32:
      case v1i64:
      case v4f16:
      case v4bf16:
      case v2f32:
      case v1f64: return TypeSize::Fixed(64);
      case nxv64i1:
      case nxv8i8:
      case nxv4i16:
      case nxv2i32:
      case nxv1i64:
      case nxv4f16:
      case nxv4bf16:
      case nxv2f32:
      case nxv1f64: return TypeSize::Scalable(64);
      case f80 :  return TypeSize::Fixed(80);
      case v3i32:
      case v3f32: return TypeSize::Fixed(96);
      case f128:
      case ppcf128:
      case i128:
      case v128i1:
      case v16i8:
      case v8i16:
      case v4i32:
      case v2i64:
      case v1i128:
      case v8f16:
      case v8bf16:
      case v4f32:
      case v2f64: return TypeSize::Fixed(128);
      case nxv16i8:
      case nxv8i16:
      case nxv4i32:
      case nxv2i64:
      case nxv8f16:
      case nxv8bf16:
      case nxv4f32:
      case nxv2f64: return TypeSize::Scalable(128);
      case v5i32:
      case v5f32: return TypeSize::Fixed(160);
      case v6i32:
      case v3i64:
      case v6f32:
      case v3f64: return TypeSize::Fixed(192);
      case v7i32:
      case v7f32: return TypeSize::Fixed(224);
      case v256i1:
      case v128i2:
      case v64i4:
      case v32i8:
      case v16i16:
      case v8i32:
      case v4i64:
      case v16f16:
      case v16bf16:
      case v8f32:
      case v4f64: return TypeSize::Fixed(256);
      case nxv32i8:
      case nxv16i16:
      case nxv8i32:
      case nxv4i64:
      case nxv16f16:
      case nxv16bf16:
      case nxv8f32:
      case nxv4f64: return TypeSize::Scalable(256);
      case i64x8:
      case v512i1:
      case v64i8:
      case v32i16:
      case v16i32:
      case v8i64:
      case v32f16:
      case v32bf16:
      case v16f32:
      case v8f64: return TypeSize::Fixed(512);
      case nxv64i8:
      case nxv32i16:
      case nxv16i32:
      case nxv8i64:
      case nxv32f16:
      case nxv32bf16:
      case nxv16f32:
      case nxv8f64: return TypeSize::Scalable(512);
      case v1024i1:
      case v128i8:
      case v64i16:
      case v32i32:
      case v16i64:
      case v64f16:
      case v64bf16:
      case v32f32:
      case v16f64: return TypeSize::Fixed(1024);
      case nxv32i32:
      case nxv16i64: return TypeSize::Scalable(1024);
      case v256i8:
      case v128i16:
      case v64i32:
      case v32i64:
      case v128f16:
      case v128bf16:
      case v64f32:
      case v32f64: return TypeSize::Fixed(2048);
      case nxv32i64: return TypeSize::Scalable(2048);
      case v512i8:
      case v256i16:
      case v128i32:
      case v64i64:
      case v256f16:
      case v256bf16:
      case v128f32:
      case v64f64:  return TypeSize::Fixed(4096);
      case v1024i8:
      case v512i16:
      case v256i32:
      case v128i64:
      case v512f16:
      case v512bf16:
      case v256f32:
      case x86amx:
      case v128f64:  return TypeSize::Fixed(8192);
      case v2048i8:
      case v1024f16:
      case v1024bf16:
      case v1024i16:
      case v512i32:
      case v256i64:
      case v512f32:
      case v256f64:  return TypeSize::Fixed(16384);
      case v4096i8:
      case v2048i16:
      case v2048f16:
      case v2048bf16:
      case v1024i32:
      case v1024f32:
      case v512i64:
      case v512f64:
        return TypeSize::Fixed(32768);
      case v1024f64:
      case v2048i32:
      case v8192i8:
      case v4096i16:
      case v4096f16:
      case v4096bf16:
      case v1024i64:
      case v2048f32:  return TypeSize::Fixed(65536);
      case v16384i8:
      case v8192i16:
      case v8192f16:
      case v8192bf16:
      case v4096i32:
      case v4096f32:
      case v2048f64:
      case v2048i64:
        return TypeSize::Fixed(131072);
      case v32768i8:
      case v16384i16:
      case v16384f16:
      case v16384bf16:
      case v8192i32:
      case v8192f32:
      case v4096i64:
      case v4096f64:
        return TypeSize::Fixed(262144);
      case v65536i8:
      case v32768i16:
      case v32768f16:
      case v32768bf16:
      case v16384i32:
      case v16384f32:
      case v8192i64:
      case v8192f64:
        return TypeSize::Fixed(524288);
      case v65536i16:
      case v65536f16:
      case v65536bf16:
      case v32768i32:
      case v32768f32:
      case v16384i64:
      case v16384f64:
        return TypeSize::Fixed(1048576);
      case v65536i32:
      case v65536f32:
      case v32768i64:
      case v32768f64:
        return TypeSize::Fixed(2097152);
      case v65536i64:
      case v65536f64:
        return TypeSize::Fixed(4194304);
      case funcref:
      case externref: return TypeSize::Fixed(0); // opaque type
      }
    }

    /// Return the size of the specified fixed width value type in bits. The
    /// function will assert if the type is scalable.
    uint64_t getFixedSizeInBits() const {
      return getSizeInBits().getFixedSize();
    }

    uint64_t getScalarSizeInBits() const {
      return getScalarType().getSizeInBits().getFixedSize();
    }

    /// Return the number of bytes overwritten by a store of the specified value
    /// type.
    ///
    /// If the value type is a scalable vector type, the scalable property will
    /// be set and the runtime size will be a positive integer multiple of the
    /// base size.
    TypeSize getStoreSize() const {
      TypeSize BaseSize = getSizeInBits();
      return {(BaseSize.getKnownMinSize() + 7) / 8, BaseSize.isScalable()};
    }

    // Return the number of bytes overwritten by a store of this value type or
    // this value type's element type in the case of a vector.
    uint64_t getScalarStoreSize() const {
      return getScalarType().getStoreSize().getFixedSize();
    }

    /// Return the number of bits overwritten by a store of the specified value
    /// type.
    ///
    /// If the value type is a scalable vector type, the scalable property will
    /// be set and the runtime size will be a positive integer multiple of the
    /// base size.
    TypeSize getStoreSizeInBits() const {
      return getStoreSize() * 8;
    }

    /// Returns true if the number of bits for the type is a multiple of an
    /// 8-bit byte.
    bool isByteSized() const { return getSizeInBits().isKnownMultipleOf(8); }

    /// Return true if we know at compile time this has more bits than VT.
    bool knownBitsGT(MVT VT) const {
      return TypeSize::isKnownGT(getSizeInBits(), VT.getSizeInBits());
    }

    /// Return true if we know at compile time this has more than or the same
    /// bits as VT.
    bool knownBitsGE(MVT VT) const {
      return TypeSize::isKnownGE(getSizeInBits(), VT.getSizeInBits());
    }

    /// Return true if we know at compile time this has fewer bits than VT.
    bool knownBitsLT(MVT VT) const {
      return TypeSize::isKnownLT(getSizeInBits(), VT.getSizeInBits());
    }

    /// Return true if we know at compile time this has fewer than or the same
    /// bits as VT.
    bool knownBitsLE(MVT VT) const {
      return TypeSize::isKnownLE(getSizeInBits(), VT.getSizeInBits());
    }

    /// Return true if this has more bits than VT.
    bool bitsGT(MVT VT) const {
      assert(isScalableVector() == VT.isScalableVector() &&
             "Comparison between scalable and fixed types");
      return knownBitsGT(VT);
    }

    /// Return true if this has no less bits than VT.
    bool bitsGE(MVT VT) const {
      assert(isScalableVector() == VT.isScalableVector() &&
             "Comparison between scalable and fixed types");
      return knownBitsGE(VT);
    }

    /// Return true if this has less bits than VT.
    bool bitsLT(MVT VT) const {
      assert(isScalableVector() == VT.isScalableVector() &&
             "Comparison between scalable and fixed types");
      return knownBitsLT(VT);
    }

    /// Return true if this has no more bits than VT.
    bool bitsLE(MVT VT) const {
      assert(isScalableVector() == VT.isScalableVector() &&
             "Comparison between scalable and fixed types");
      return knownBitsLE(VT);
    }

    static MVT getFloatingPointVT(unsigned BitWidth) {
      switch (BitWidth) {
      default:
        llvm_unreachable("Bad bit width!");
      case 16:
        return MVT::f16;
      case 32:
        return MVT::f32;
      case 64:
        return MVT::f64;
      case 80:
        return MVT::f80;
      case 128:
        return MVT::f128;
      }
    }

    static MVT getIntegerVT(unsigned BitWidth) {
      switch (BitWidth) {
      default:
        return (MVT::SimpleValueType)(MVT::INVALID_SIMPLE_VALUE_TYPE);
      case 1:
        return MVT::i1;
      case 2:
        return MVT::i2;
      case 4:
        return MVT::i4;
      case 8:
        return MVT::i8;
      case 16:
        return MVT::i16;
      case 32:
        return MVT::i32;
      case 64:
        return MVT::i64;
      case 128:
        return MVT::i128;
      }
    }

    static MVT getVectorVT(MVT VT, unsigned NumElements) {
      // clang-format off
      switch (VT.SimpleTy) {
      default:
        break;
      case MVT::i1:
        if (NumElements == 1)    return MVT::v1i1;
        if (NumElements == 2)    return MVT::v2i1;
        if (NumElements == 4)    return MVT::v4i1;
        if (NumElements == 8)    return MVT::v8i1;
        if (NumElements == 16)   return MVT::v16i1;
        if (NumElements == 32)   return MVT::v32i1;
        if (NumElements == 64)   return MVT::v64i1;
        if (NumElements == 128)  return MVT::v128i1;
        if (NumElements == 256)  return MVT::v256i1;
        if (NumElements == 512)  return MVT::v512i1;
        if (NumElements == 1024) return MVT::v1024i1;
        break;
      case MVT::i2:
        if (NumElements == 128) return MVT::v128i2;
        break;
      case MVT::i4:
        if (NumElements == 64) return MVT::v64i4;
        break;
      case MVT::i8:
        if (NumElements == 1)   return MVT::v1i8;
        if (NumElements == 2)   return MVT::v2i8;
        if (NumElements == 4)   return MVT::v4i8;
        if (NumElements == 8)   return MVT::v8i8;
        if (NumElements == 16)  return MVT::v16i8;
        if (NumElements == 32)  return MVT::v32i8;
        if (NumElements == 64)  return MVT::v64i8;
        if (NumElements == 128) return MVT::v128i8;
        if (NumElements == 256) return MVT::v256i8;
        if (NumElements == 512) return MVT::v512i8;
        if (NumElements == 1024) return MVT::v1024i8;
        if (NumElements == 2048)  return MVT::v2048i8;
        if (NumElements == 4096)  return MVT::v4096i8;
        if (NumElements == 8192)  return MVT::v8192i8;
        if (NumElements == 16384) return MVT::v16384i8;
        if (NumElements == 32768) return MVT::v32768i8;
        if (NumElements == 65536) return MVT::v65536i8;
        break;
      case MVT::i16:
        if (NumElements == 1)   return MVT::v1i16;
        if (NumElements == 2)   return MVT::v2i16;
        if (NumElements == 3)   return MVT::v3i16;
        if (NumElements == 4)   return MVT::v4i16;
        if (NumElements == 8)   return MVT::v8i16;
        if (NumElements == 16)  return MVT::v16i16;
        if (NumElements == 32)  return MVT::v32i16;
        if (NumElements == 64)  return MVT::v64i16;
        if (NumElements == 128) return MVT::v128i16;
        if (NumElements == 256) return MVT::v256i16;
        if (NumElements == 512) return MVT::v512i16;
        if (NumElements == 1024)  return MVT::v1024i16;
        if (NumElements == 2048)  return MVT::v2048i16;
        if (NumElements == 4096)  return MVT::v4096i16;
        if (NumElements == 8192)  return MVT::v8192i16;
        if (NumElements == 16384) return MVT::v16384i16;
        if (NumElements == 32768) return MVT::v32768i16;
        if (NumElements == 65536) return MVT::v65536i16;
        break;
      case MVT::i32:
        if (NumElements == 1)    return MVT::v1i32;
        if (NumElements == 2)    return MVT::v2i32;
        if (NumElements == 3)    return MVT::v3i32;
        if (NumElements == 4)    return MVT::v4i32;
        if (NumElements == 5)    return MVT::v5i32;
        if (NumElements == 6)    return MVT::v6i32;
        if (NumElements == 7)    return MVT::v7i32;
        if (NumElements == 8)    return MVT::v8i32;
        if (NumElements == 16)   return MVT::v16i32;
        if (NumElements == 32)   return MVT::v32i32;
        if (NumElements == 64)   return MVT::v64i32;
        if (NumElements == 128)  return MVT::v128i32;
        if (NumElements == 256)  return MVT::v256i32;
        if (NumElements == 512)  return MVT::v512i32;
        if (NumElements == 1024) return MVT::v1024i32;
        if (NumElements == 2048) return MVT::v2048i32;
        if (NumElements == 4096)  return MVT::v4096i32;
        if (NumElements == 8192)  return MVT::v8192i32;
        if (NumElements == 16384) return MVT::v16384i32;
        if (NumElements == 32768) return MVT::v32768i32;
        if (NumElements == 65536) return MVT::v65536i32;
        break;
      case MVT::i64:
        if (NumElements == 1)  return MVT::v1i64;
        if (NumElements == 2)  return MVT::v2i64;
        if (NumElements == 3)  return MVT::v3i64;
        if (NumElements == 4)  return MVT::v4i64;
        if (NumElements == 8)  return MVT::v8i64;
        if (NumElements == 16) return MVT::v16i64;
        if (NumElements == 32) return MVT::v32i64;
        if (NumElements == 64) return MVT::v64i64;
        if (NumElements == 128) return MVT::v128i64;
        if (NumElements == 256) return MVT::v256i64;
        if (NumElements == 512)   return MVT::v512i64;
        if (NumElements == 1024)  return MVT::v1024i64;
        if (NumElements == 2048)  return MVT::v2048i64;
        if (NumElements == 4096)  return MVT::v4096i64;
        if (NumElements == 8192)  return MVT::v8192i64;
        if (NumElements == 16384) return MVT::v16384i64;
        if (NumElements == 32768) return MVT::v32768i64;
        if (NumElements == 65536) return MVT::v65536i64;
        break;
      case MVT::i128:
        if (NumElements == 1)  return MVT::v1i128;
        break;
      case MVT::f16:
        if (NumElements == 1)   return MVT::v1f16;
        if (NumElements == 2)   return MVT::v2f16;
        if (NumElements == 3)   return MVT::v3f16;
        if (NumElements == 4)   return MVT::v4f16;
        if (NumElements == 8)   return MVT::v8f16;
        if (NumElements == 16)  return MVT::v16f16;
        if (NumElements == 32)  return MVT::v32f16;
        if (NumElements == 64)  return MVT::v64f16;
        if (NumElements == 128) return MVT::v128f16;
        if (NumElements == 256) return MVT::v256f16;
        if (NumElements == 512) return MVT::v512f16;
        if (NumElements == 1024)  return MVT::v1024f16;
        if (NumElements == 2048)  return MVT::v2048f16;
        if (NumElements == 4096)  return MVT::v4096f16;
        if (NumElements == 8192)  return MVT::v8192f16;
        if (NumElements == 16384) return MVT::v16384f16;
        if (NumElements == 32768) return MVT::v32768f16;
        if (NumElements == 65536) return MVT::v65536f16;
        break;
      case MVT::bf16:
        if (NumElements == 2)   return MVT::v2bf16;
        if (NumElements == 3)   return MVT::v3bf16;
        if (NumElements == 4)   return MVT::v4bf16;
        if (NumElements == 8)   return MVT::v8bf16;
        if (NumElements == 16)  return MVT::v16bf16;
        if (NumElements == 32)  return MVT::v32bf16;
        if (NumElements == 64)  return MVT::v64bf16;
        if (NumElements == 128) return MVT::v128bf16;
        if (NumElements == 256)   return MVT::v256bf16;
        if (NumElements == 512)   return MVT::v512bf16;
        if (NumElements == 1024)  return MVT::v1024bf16;
        if (NumElements == 2048)  return MVT::v2048bf16;
        if (NumElements == 4096)  return MVT::v4096bf16;
        if (NumElements == 8192)  return MVT::v8192bf16;
        if (NumElements == 16384) return MVT::v16384bf16;
        if (NumElements == 32768) return MVT::v32768bf16;
        if (NumElements == 65536) return MVT::v65536bf16;
        break;
      case MVT::f32:
        if (NumElements == 1)    return MVT::v1f32;
        if (NumElements == 2)    return MVT::v2f32;
        if (NumElements == 3)    return MVT::v3f32;
        if (NumElements == 4)    return MVT::v4f32;
        if (NumElements == 5)    return MVT::v5f32;
        if (NumElements == 6)    return MVT::v6f32;
        if (NumElements == 7)    return MVT::v7f32;
        if (NumElements == 8)    return MVT::v8f32;
        if (NumElements == 16)   return MVT::v16f32;
        if (NumElements == 32)   return MVT::v32f32;
        if (NumElements == 64)   return MVT::v64f32;
        if (NumElements == 128)  return MVT::v128f32;
        if (NumElements == 256)  return MVT::v256f32;
        if (NumElements == 512)  return MVT::v512f32;
        if (NumElements == 1024) return MVT::v1024f32;
        if (NumElements == 2048) return MVT::v2048f32;
        if (NumElements == 4096)  return MVT::v4096f32;
        if (NumElements == 8192)  return MVT::v8192f32;
        if (NumElements == 16384) return MVT::v16384f32;
        if (NumElements == 32768) return MVT::v32768f32;
        if (NumElements == 65536) return MVT::v65536f32;
        break;
      case MVT::f64:
        if (NumElements == 1)  return MVT::v1f64;
        if (NumElements == 2)  return MVT::v2f64;
        if (NumElements == 3)  return MVT::v3f64;
        if (NumElements == 4)  return MVT::v4f64;
        if (NumElements == 8)  return MVT::v8f64;
        if (NumElements == 16) return MVT::v16f64;
        if (NumElements == 32) return MVT::v32f64;
        if (NumElements == 64) return MVT::v64f64;
        if (NumElements == 128) return MVT::v128f64;
        if (NumElements == 256) return MVT::v256f64;
        if (NumElements == 512) return MVT::v512f64;
        if (NumElements == 1024) return MVT::v1024f64;
        if (NumElements == 2048) return MVT::v2048f64;
        if (NumElements == 4096) return MVT::v4096f64;
        if (NumElements == 8192) return MVT::v8192f64;
        if (NumElements == 16384) return MVT::v16384f64;
        if (NumElements == 32768) return MVT::v32768f64;
        if (NumElements == 65536) return MVT::v65536f64;
        break;
      }
      return (MVT::SimpleValueType)(MVT::INVALID_SIMPLE_VALUE_TYPE);
      // clang-format on
    }

    static MVT getScalableVectorVT(MVT VT, unsigned NumElements) {
      switch(VT.SimpleTy) {
        default:
          break;
        case MVT::i1:
          if (NumElements == 1)  return MVT::nxv1i1;
          if (NumElements == 2)  return MVT::nxv2i1;
          if (NumElements == 4)  return MVT::nxv4i1;
          if (NumElements == 8)  return MVT::nxv8i1;
          if (NumElements == 16) return MVT::nxv16i1;
          if (NumElements == 32) return MVT::nxv32i1;
          if (NumElements == 64) return MVT::nxv64i1;
          break;
        case MVT::i8:
          if (NumElements == 1)  return MVT::nxv1i8;
          if (NumElements == 2)  return MVT::nxv2i8;
          if (NumElements == 4)  return MVT::nxv4i8;
          if (NumElements == 8)  return MVT::nxv8i8;
          if (NumElements == 16) return MVT::nxv16i8;
          if (NumElements == 32) return MVT::nxv32i8;
          if (NumElements == 64) return MVT::nxv64i8;
          break;
        case MVT::i16:
          if (NumElements == 1)  return MVT::nxv1i16;
          if (NumElements == 2)  return MVT::nxv2i16;
          if (NumElements == 4)  return MVT::nxv4i16;
          if (NumElements == 8)  return MVT::nxv8i16;
          if (NumElements == 16) return MVT::nxv16i16;
          if (NumElements == 32) return MVT::nxv32i16;
          break;
        case MVT::i32:
          if (NumElements == 1)  return MVT::nxv1i32;
          if (NumElements == 2)  return MVT::nxv2i32;
          if (NumElements == 4)  return MVT::nxv4i32;
          if (NumElements == 8)  return MVT::nxv8i32;
          if (NumElements == 16) return MVT::nxv16i32;
          if (NumElements == 32) return MVT::nxv32i32;
          break;
        case MVT::i64:
          if (NumElements == 1)  return MVT::nxv1i64;
          if (NumElements == 2)  return MVT::nxv2i64;
          if (NumElements == 4)  return MVT::nxv4i64;
          if (NumElements == 8)  return MVT::nxv8i64;
          if (NumElements == 16) return MVT::nxv16i64;
          if (NumElements == 32) return MVT::nxv32i64;
          break;
        case MVT::f16:
          if (NumElements == 1)  return MVT::nxv1f16;
          if (NumElements == 2)  return MVT::nxv2f16;
          if (NumElements == 4)  return MVT::nxv4f16;
          if (NumElements == 8)  return MVT::nxv8f16;
          if (NumElements == 16)  return MVT::nxv16f16;
          if (NumElements == 32)  return MVT::nxv32f16;
          break;
        case MVT::bf16:
          if (NumElements == 1)  return MVT::nxv1bf16;
          if (NumElements == 2)  return MVT::nxv2bf16;
          if (NumElements == 4)  return MVT::nxv4bf16;
          if (NumElements == 8)  return MVT::nxv8bf16;
          if (NumElements == 16)  return MVT::nxv16bf16;
          if (NumElements == 32)  return MVT::nxv32bf16;
          break;
        case MVT::f32:
          if (NumElements == 1)  return MVT::nxv1f32;
          if (NumElements == 2)  return MVT::nxv2f32;
          if (NumElements == 4)  return MVT::nxv4f32;
          if (NumElements == 8)  return MVT::nxv8f32;
          if (NumElements == 16) return MVT::nxv16f32;
          break;
        case MVT::f64:
          if (NumElements == 1)  return MVT::nxv1f64;
          if (NumElements == 2)  return MVT::nxv2f64;
          if (NumElements == 4)  return MVT::nxv4f64;
          if (NumElements == 8)  return MVT::nxv8f64;
          break;
      }
      return (MVT::SimpleValueType)(MVT::INVALID_SIMPLE_VALUE_TYPE);
    }

    static MVT getVectorVT(MVT VT, unsigned NumElements, bool IsScalable) {
      if (IsScalable)
        return getScalableVectorVT(VT, NumElements);
      return getVectorVT(VT, NumElements);
    }

    static MVT getVectorVT(MVT VT, ElementCount EC) {
      if (EC.isScalable())
        return getScalableVectorVT(VT, EC.getKnownMinValue());
      return getVectorVT(VT, EC.getKnownMinValue());
    }

    /// Return the value type corresponding to the specified type.  This returns
    /// all pointers as iPTR.  If HandleUnknown is true, unknown types are
    /// returned as Other, otherwise they are invalid.
    static MVT getVT(Type *Ty, bool HandleUnknown = false);

  public:
    /// SimpleValueType Iteration
    /// @{
    static auto all_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_VALUETYPE, MVT::LAST_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto integer_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_INTEGER_VALUETYPE,
                                MVT::LAST_INTEGER_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto fp_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_FP_VALUETYPE, MVT::LAST_FP_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto vector_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_VECTOR_VALUETYPE,
                                MVT::LAST_VECTOR_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto fixedlen_vector_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_FIXEDLEN_VECTOR_VALUETYPE,
                                MVT::LAST_FIXEDLEN_VECTOR_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto scalable_vector_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_SCALABLE_VECTOR_VALUETYPE,
                                MVT::LAST_SCALABLE_VECTOR_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto integer_fixedlen_vector_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE,
                                MVT::LAST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto fp_fixedlen_vector_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_FP_FIXEDLEN_VECTOR_VALUETYPE,
                                MVT::LAST_FP_FIXEDLEN_VECTOR_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto integer_scalable_vector_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_INTEGER_SCALABLE_VECTOR_VALUETYPE,
                                MVT::LAST_INTEGER_SCALABLE_VECTOR_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }

    static auto fp_scalable_vector_valuetypes() {
      return enum_seq_inclusive(MVT::FIRST_FP_SCALABLE_VECTOR_VALUETYPE,
                                MVT::LAST_FP_SCALABLE_VECTOR_VALUETYPE,
                                force_iteration_on_noniterable_enum);
    }
    /// @}
  };

} // end namespace llvm

#endif // LLVM_SUPPORT_MACHINEVALUETYPE_H
