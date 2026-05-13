//===- ValueTypes.td - ValueType definitions ---------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Value types - These values correspond to the register types defined in the
// MachineValueTypes.h file.  If you update anything here, you must update it
// there as well!
//
//===----------------------------------------------------------------------===//

class ValueType<int size, int value> {
  string Namespace = "MVT";
  int Size = size;
  int Value = value;
}

def OtherVT : ValueType<0,   1>;  // "Other" value
def i1      : ValueType<1,   2>;  // One bit boolean value
def i2      : ValueType<2,   3>;  // 2-bit integer value
def i4      : ValueType<4,   4>;  // 4-bit integer value
def i8      : ValueType<8,   5>;  // 8-bit integer value
def i16     : ValueType<16,  6>;  // 16-bit integer value
def i32     : ValueType<32,  7>;  // 32-bit integer value
def i64     : ValueType<64,  8>;  // 64-bit integer value
def i128    : ValueType<128, 9>;  // 128-bit integer value

def bf16    : ValueType<16,  10>;  // 16-bit brain floating point value
def f16     : ValueType<16,  11>;  // 16-bit floating point value
def f32     : ValueType<32,  12>;  // 32-bit floating point value
def f64     : ValueType<64,  13>;  // 64-bit floating point value
def f80     : ValueType<80,  14>;  // 80-bit floating point value
def f128    : ValueType<128, 15>;  // 128-bit floating point value
def ppcf128 : ValueType<128, 16>;  // PPC 128-bit floating point value

def v1i1    : ValueType<1,    17>;  //    1 x i1 vector value
def v2i1    : ValueType<2,    18>;  //    2 x i1 vector value
def v4i1    : ValueType<4,    19>;  //    4 x i1 vector value
def v8i1    : ValueType<8,    20>;  //    8 x i1 vector value
def v16i1   : ValueType<16,   21>;  //   16 x i1 vector value
def v32i1   : ValueType<32,   22>;  //   32 x i1 vector value
def v64i1   : ValueType<64,   23>;  //   64 x i1 vector value
def v128i1  : ValueType<128,  24>;  //  128 x i1 vector value
def v256i1  : ValueType<256,  25>;  //  256 x i1 vector value
def v512i1  : ValueType<512,  26>;  //  512 x i1 vector value
def v1024i1 : ValueType<1024, 27>;  // 1024 x i1 vector value

def v128i2  : ValueType<256,  28>;   //  128 x i2 vector value

def v64i4   : ValueType<256,  29>;   //   64 x i4 vector value

def v1i8    : ValueType<8,    30>;  //    1 x i8 vector value
def v2i8    : ValueType<16,   31>;  //    2 x i8 vector value
def v4i8    : ValueType<32,   32>;  //    4 x i8 vector value
def v8i8    : ValueType<64,   33>;  //    8 x i8 vector value
def v16i8   : ValueType<128,  34>;  //   16 x i8 vector value
def v32i8   : ValueType<256,  35>;  //   32 x i8 vector value
def v64i8   : ValueType<512,  36>;  //   64 x i8 vector value
def v128i8  : ValueType<1024, 37>;  //  128 x i8 vector value
def v256i8  : ValueType<2048, 38>;  //  256 x i8 vector value
def v512i8  : ValueType<4096, 39>;  //  512 x i8 vector value
def v1024i8 : ValueType<8192, 40>;  // 1024 x i8 vector value
def v2048i8 : ValueType<16384, 41>;  // 2048 x i8 vector value
def v4096i8  : ValueType<32768, 42>;  // 4096 x i8 vector value
def v8192i8  : ValueType<65536, 43>;  // 8192 x i8 vector value
def v16384i8 : ValueType<131072, 44>; // 16384 x i8 vector value
def v32768i8 : ValueType<262144, 45>; // 32768 x i8 vector value
def v65536i8 : ValueType<524288, 46>; // 65536 x i8 vector value

def v1i16   : ValueType<16,   47>;  //   1 x i16 vector value
def v2i16   : ValueType<32,   48>;  //   2 x i16 vector value
def v3i16   : ValueType<48,   49>;  //   3 x i16 vector value
def v4i16   : ValueType<64,   50>;  //   4 x i16 vector value
def v8i16   : ValueType<128,  51>;  //   8 x i16 vector value
def v16i16  : ValueType<256,  52>;  //  16 x i16 vector value
def v32i16  : ValueType<512,  53>;  //  32 x i16 vector value
def v64i16  : ValueType<1024, 54>;  //  64 x i16 vector value
def v128i16 : ValueType<2048, 55>;  // 128 x i16 vector value
def v256i16 : ValueType<4096, 56>;  // 256 x i16 vector value
def v512i16 : ValueType<8192, 57>;  // 512 x i16 vector value
def v1024i16   : ValueType<16384, 58>;   // 1024 x i16 vector value
def v2048i16   : ValueType<32768, 59>;   // 2048 x i16 vector value
def v4096i16   : ValueType<65536, 60>;   // 4096 x i16 vector value
def v8192i16   : ValueType<131072, 61>;  // 8192 x i16 vector value
def v16384i16  : ValueType<262144, 62>;  // 16384 x i16 vector value
def v32768i16  : ValueType<524288, 63>;  // 32768 x i16 vector value
def v65536i16  : ValueType<1048576, 64>; // 65536 x i16 vector value

def v1i32    : ValueType<32,    65>;  //    1 x i32 vector value
def v2i32    : ValueType<64,    66>;  //    2 x i32 vector value
def v3i32    : ValueType<96,    67>;  //    3 x i32 vector value
def v4i32    : ValueType<128,   68>;  //    4 x i32 vector value
def v5i32    : ValueType<160,   69>;  //    5 x i32 vector value
def v6i32    : ValueType<192,   70>;  //    6 x f32 vector value
def v7i32    : ValueType<224,   71>;  //    7 x f32 vector value
def v8i32    : ValueType<256,   72>;  //    8 x i32 vector value
def v16i32   : ValueType<512,   73>;  //   16 x i32 vector value
def v32i32   : ValueType<1024,  74>;  //   32 x i32 vector value
def v64i32   : ValueType<2048,  75>;  //   64 x i32 vector value
def v128i32  : ValueType<4096,  76>;  //  128 x i32 vector value
def v256i32  : ValueType<8192,  77>;  //  256 x i32 vector value
def v512i32  : ValueType<16384, 78>;  //  512 x i32 vector value
def v1024i32 : ValueType<32768, 79>;  // 1024 x i32 vector value
def v2048i32 : ValueType<65536, 80>;  // 2048 x i32 vector value
def v4096i32   : ValueType<131072, 81>;   // 4096 x i32 vector value
def v8192i32   : ValueType<262144, 82>;   // 8192 x i32 vector value
def v16384i32  : ValueType<524288, 83>;   // 16384 x i32 vector value
def v32768i32  : ValueType<1048576, 84>;  // 32768 x i32 vector value
def v65536i32  : ValueType<2097152, 85>;  // 65536 x i32 vector value

def v1i64   : ValueType<64,    86>;  //   1 x i64 vector value
def v2i64   : ValueType<128,   87>;  //   2 x i64 vector value
def v3i64   : ValueType<192,   88>;  //   3 x i64 vector value
def v4i64   : ValueType<256,   89>;  //   4 x i64 vector value
def v8i64   : ValueType<512,   90>;  //   8 x i64 vector value
def v16i64  : ValueType<1024,  91>;  //  16 x i64 vector value
def v32i64  : ValueType<2048,  92>;  //  32 x i64 vector value
def v64i64  : ValueType<4096,  93>;  //  64 x i64 vector value
def v128i64 : ValueType<8192,  94>;  // 128 x i64 vector value
def v256i64 : ValueType<16384, 95>;  // 256 x i64 vector value
def v512i64    : ValueType<32768, 96>;     // 512 x i64 vector value
def v1024i64   : ValueType<65536, 97>;     // 1024 x i64 vector value
def v2048i64   : ValueType<131072, 98>;    // 2048 x i64 vector value
def v4096i64   : ValueType<262144, 99>;    // 4096 x i64 vector value
def v8192i64   : ValueType<524288, 100>;   // 8192 x i64 vector value
def v16384i64  : ValueType<1048576, 101>;  // 16384 x i64 vector value
def v32768i64  : ValueType<2097152, 102>;  // 32768 x i64 vector value
def v65536i64  : ValueType<4194304, 103>;  // 65536 x i64 vector value

def v1i128 : ValueType<128, 104>;  //  1 x i128 vector value

def v1f16    : ValueType<16,     105>;  //    1 x f16 vector value
def v2f16    : ValueType<32,     106>;  //    2 x f16 vector value
def v3f16    : ValueType<48,     107>;  //    3 x f16 vector value
def v4f16    : ValueType<64,     108>;  //    4 x f16 vector value
def v8f16    : ValueType<128,    109>;  //    8 x f16 vector value
def v16f16   : ValueType<256,    110>;  //   16 x f16 vector value
def v32f16   : ValueType<512,    111>;  //   32 x f16 vector value
def v64f16   : ValueType<1024,   112>;  //   64 x f16 vector value
def v128f16  : ValueType<2048,   113>;  //  128 x f16 vector value
def v256f16  : ValueType<4096,   114>;  //  256 x f16 vector value
def v512f16  : ValueType<8192,   115>;  //  512 x f16 vector value
def v1024f16   : ValueType<16384, 116>;    // 1024 x f16 vector value
def v2048f16   : ValueType<32768, 117>;    // 2048 x f16 vector value
def v4096f16   : ValueType<65536, 118>;    // 4096 x f16 vector value
def v8192f16   : ValueType<131072, 119>;   // 8192 x f16 vector value
def v16384f16  : ValueType<262144, 120>;   // 16384 x f16 vector value
def v32768f16  : ValueType<524288, 121>;   // 32768 x f16 vector value
def v65536f16  : ValueType<1048576, 122>;  // 65536 x f16 vector value

def v2bf16   : ValueType<32,     123>;  //    2 x bf16 vector value
def v3bf16   : ValueType<48,     124>;  //    3 x bf16 vector value
def v4bf16   : ValueType<64,     125>;  //    4 x bf16 vector value
def v8bf16   : ValueType<128,    126>;  //    8 x bf16 vector value
def v16bf16  : ValueType<256,    127>;  //   16 x bf16 vector value
def v32bf16  : ValueType<512,    128>;  //   32 x bf16 vector value
def v64bf16  : ValueType<1024,   129>;  //   64 x bf16 vector value
def v128bf16 : ValueType<2048,   130>;  //  128 x bf16 vector value
def v256bf16    : ValueType<4096, 131>;      // 256 x bf16 vector value
def v512bf16    : ValueType<8192, 132>;      // 512 x bf16 vector value
def v1024bf16   : ValueType<16384, 133>;     // 1024 x bf16 vector value
def v2048bf16   : ValueType<32768, 134>;     // 2048 x bf16 vector value
def v4096bf16   : ValueType<65536, 135>;     // 4096 x bf16 vector value
def v8192bf16   : ValueType<131072, 136>;    // 8192 x bf16 vector value
def v16384bf16  : ValueType<262144, 137>;    // 16384 x bf16 vector value
def v32768bf16  : ValueType<524288, 138>;    // 32768 x bf16 vector value
def v65536bf16  : ValueType<1048576, 139>;   // 65536 x bf16 vector value

def v1f32    : ValueType<32,    140>;  //    1 x f32 vector value
def v2f32    : ValueType<64,    141>;  //    2 x f32 vector value
def v3f32    : ValueType<96,    142>;  //    3 x f32 vector value
def v4f32    : ValueType<128,   143>;  //    4 x f32 vector value
def v5f32    : ValueType<160,   144>;  //    5 x f32 vector value
def v6f32    : ValueType<192,   145>;  //    6 x f32 vector value
def v7f32    : ValueType<224,   146>;  //    7 x f32 vector value
def v8f32    : ValueType<256,   147>;  //    8 x f32 vector value
def v16f32   : ValueType<512,   148>;  //   16 x f32 vector value
def v32f32   : ValueType<1024,  149>;  //   32 x f32 vector value
def v64f32   : ValueType<2048,  150>;  //   64 x f32 vector value
def v128f32  : ValueType<4096,  151>;  //  128 x f32 vector value
def v256f32  : ValueType<8192,  152>;  //  256 x f32 vector value
def v512f32  : ValueType<16384, 153>;  //  512 x f32 vector value
def v1024f32 : ValueType<32768, 154>;  // 1024 x f32 vector value
def v2048f32 : ValueType<65536, 155>;  // 2048 x f32 vector value
def v4096f32   : ValueType<131072, 156>;   // 4096 x f32 vector value
def v8192f32   : ValueType<262144, 157>;   // 8192 x f32 vector value
def v16384f32  : ValueType<524288, 158>;   // 16384 x f32 vector value
def v32768f32  : ValueType<1048576, 159>;  // 32768 x f32 vector value
def v65536f32  : ValueType<2097152, 160>;  // 65536 x f32 vector value

def v1f64    : ValueType<64,    161>;  //    1 x f64 vector value
def v2f64    : ValueType<128,   162>;  //    2 x f64 vector value
def v3f64    : ValueType<192,   163>;  //    3 x f64 vector value
def v4f64    : ValueType<256,   164>;  //    4 x f64 vector value
def v8f64    : ValueType<512,   165>;  //    8 x f64 vector value
def v16f64   : ValueType<1024,  166>;  //   16 x f64 vector value
def v32f64   : ValueType<2048,  167>;  //   32 x f64 vector value
def v64f64   : ValueType<4096,  168>;  //   64 x f64 vector value
def v128f64  : ValueType<8192,  169>;  //  128 x f64 vector value
def v256f64  : ValueType<16384, 170>;  //  256 x f64 vector value
def v512f64  : ValueType<32768, 171>;  // 512 x f64 vector value
def v1024f64 : ValueType<65536, 172>;  // 1024 x f64 vector value
def v2048f64 : ValueType<131072, 173>;  // 2048 x f64 vector value
def v4096f64 : ValueType<262144, 174>;  // 4096 x f64 vector value
def v8192f64 : ValueType<524288, 175>;  // 8192 x f64 vector value
def v16384f64: ValueType<1048576, 176>;  // 16384 x f64 vector value
def v32768f64: ValueType<2097152, 177>;  // 32768 x f64 vector value
def v65536f64: ValueType<4194304, 178>;  // 65536 x f64 vector value

def nxv1i1  : ValueType<1,  179>;  // n x  1 x i1  vector value
def nxv2i1  : ValueType<2,  180>;  // n x  2 x i1  vector value
def nxv4i1  : ValueType<4,  181>;  // n x  4 x i1  vector value
def nxv8i1  : ValueType<8,  182>;  // n x  8 x i1  vector value
def nxv16i1 : ValueType<16, 183>;  // n x 16 x i1  vector value
def nxv32i1 : ValueType<32, 184>;  // n x 32 x i1  vector value
def nxv64i1 : ValueType<64, 185>;  // n x 64 x i1  vector value

def nxv1i8  : ValueType<8,   186>;  // n x  1 x i8  vector value
def nxv2i8  : ValueType<16,  187>;  // n x  2 x i8  vector value
def nxv4i8  : ValueType<32,  188>;  // n x  4 x i8  vector value
def nxv8i8  : ValueType<64,  189>;  // n x  8 x i8  vector value
def nxv16i8 : ValueType<128, 190>;  // n x 16 x i8  vector value
def nxv32i8 : ValueType<256, 191>;  // n x 32 x i8  vector value
def nxv64i8 : ValueType<512, 192>;  // n x 64 x i8  vector value

def nxv1i16  : ValueType<16,  193>;  // n x  1 x i16 vector value
def nxv2i16  : ValueType<32,  194>;  // n x  2 x i16 vector value
def nxv4i16  : ValueType<64,  195>;  // n x  4 x i16 vector value
def nxv8i16  : ValueType<128, 196>;  // n x  8 x i16 vector value
def nxv16i16 : ValueType<256, 197>;  // n x 16 x i16 vector value
def nxv32i16 : ValueType<512, 198>;  // n x 32 x i16 vector value

def nxv1i32  : ValueType<32,   199>;  // n x  1 x i32 vector value
def nxv2i32  : ValueType<64,   200>;  // n x  2 x i32 vector value
def nxv4i32  : ValueType<128,  201>;  // n x  4 x i32 vector value
def nxv8i32  : ValueType<256,  202>;  // n x  8 x i32 vector value
def nxv16i32 : ValueType<512,  203>;  // n x 16 x i32 vector value
def nxv32i32 : ValueType<1024, 204>;  // n x 32 x i32 vector value

def nxv1i64  : ValueType<64,   205>;  // n x  1 x i64 vector value
def nxv2i64  : ValueType<128,  206>;  // n x  2 x i64 vector value
def nxv4i64  : ValueType<256,  207>;  // n x  4 x i64 vector value
def nxv8i64  : ValueType<512,  208>;  // n x  8 x i64 vector value
def nxv16i64 : ValueType<1024, 209>;  // n x 16 x i64 vector value
def nxv32i64 : ValueType<2048, 210>;  // n x 32 x i64 vector value

def nxv1f16  : ValueType<16,  211>;  // n x  1 x  f16 vector value
def nxv2f16  : ValueType<32,  212>;  // n x  2 x  f16 vector value
def nxv4f16  : ValueType<64,  213>;  // n x  4 x  f16 vector value
def nxv8f16  : ValueType<128, 214>;  // n x  8 x  f16 vector value
def nxv16f16 : ValueType<256, 215>;  // n x 16 x  f16 vector value
def nxv32f16 : ValueType<512, 216>;  // n x 32 x  f16 vector value

def nxv1bf16 : ValueType<16,  217>;  // n x  1 x bf16 vector value
def nxv2bf16 : ValueType<32,  218>;  // n x  2 x bf16 vector value
def nxv4bf16 : ValueType<64,  219>;  // n x  4 x bf16 vector value
def nxv8bf16 : ValueType<128, 220>;  // n x  8 x bf16 vector value
def nxv16bf16 : ValueType<256, 221>;  // n x 16 x bf16 vector value
def nxv32bf16 : ValueType<512, 222>;  // n x 32 x bf16 vector value

def nxv1f32  : ValueType<32,  223>;  // n x  1 x  f32 vector value
def nxv2f32  : ValueType<64,  224>;  // n x  2 x  f32 vector value
def nxv4f32  : ValueType<128, 225>;  // n x  4 x  f32 vector value
def nxv8f32  : ValueType<256, 226>;  // n x  8 x  f32 vector value
def nxv16f32 : ValueType<512, 227>;  // n x 16 x  f32 vector value

def nxv1f64  : ValueType<64,  228>;  // n x  1 x  f64 vector value
def nxv2f64  : ValueType<128, 229>;  // n x  2 x  f64 vector value
def nxv4f64  : ValueType<256, 230>;  // n x  4 x  f64 vector value
def nxv8f64  : ValueType<512, 231>;  // n x  8 x  f64 vector value

def x86mmx    : ValueType<64,   232>;  // X86 MMX value
def FlagVT    : ValueType<0,    233>;  // Pre-RA sched glue
def isVoid    : ValueType<0,    234>;  // Produces no value
def untyped   : ValueType<8,    235>;  // Produces an untyped value
def funcref   : ValueType<0,    236>;  // WebAssembly's funcref type
def externref : ValueType<0,    237>;  // WebAssembly's externref type
def x86amx    : ValueType<8192, 238>;  // X86 AMX value
def i64x8     : ValueType<512,  239>;  // 8 Consecutive GPRs (AArch64)

def token      : ValueType<0, 248>;  // TokenTy
def MetadataVT : ValueType<0, 249>;  // Metadata

// Pseudo valuetype mapped to the current pointer size to any address space.
// Should only be used in TableGen.
def iPTRAny    : ValueType<0, 250>;

// Pseudo valuetype to represent "vector of any size"
def vAny       : ValueType<0, 251>;

// Pseudo valuetype to represent "float of any format"
def fAny       : ValueType<0, 252>;

// Pseudo valuetype to represent "integer of any bit width"
def iAny       : ValueType<0, 253>;

// Pseudo valuetype mapped to the current pointer size.
def iPTR       : ValueType<0, 254>;

// Pseudo valuetype to represent "any type of any size".
def Any        : ValueType<0, 255>;

/// This class is for targets that want to use pointer types in patterns
/// with the GlobalISelEmitter.  Targets must define their own pointer
/// derived from this class.  The scalar argument should be an
/// integer type with the same bit size as the pointer.
/// e.g. def p0 : PtrValueType <i64, 0>;

class PtrValueType <ValueType scalar, int addrspace> :
    ValueType<scalar.Size, scalar.Value> {
  int AddrSpace = addrspace;
}
