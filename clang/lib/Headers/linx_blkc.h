//===-- linx_blkc.h - Linx block C++ frontend ABI ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef __LINX_BLKC_H
#define __LINX_BLKC_H

#if !defined(__linx) && !defined(__LINX__)
#error "linx_blkc.h is only supported for the Linx target"
#endif

#define PTO_LINX_COMPAT_TYPES_PROVIDED 1

using __fp32 = float;
using __half = _Float16;

#define __LINX_STORAGE_TYPE(NAME, STORAGE)                                 \
  enum class __attribute__((annotate("linx.storage_type"))) NAME : STORAGE {}

__LINX_STORAGE_TYPE(__tf32, unsigned int);
__LINX_STORAGE_TYPE(__hf32, unsigned int);
__LINX_STORAGE_TYPE(__blkc_bf16, unsigned short);
__LINX_STORAGE_TYPE(__hif8, unsigned char);
__LINX_STORAGE_TYPE(__fp8_e4m3, unsigned char);
__LINX_STORAGE_TYPE(__fp8_e5m2, unsigned char);
__LINX_STORAGE_TYPE(__fp8_e6m2, unsigned char);
__LINX_STORAGE_TYPE(__fp6_e3m2, unsigned char);
__LINX_STORAGE_TYPE(__fp6_e2m3, unsigned char);
__LINX_STORAGE_TYPE(__fp4_e2m1x2, unsigned char);
__LINX_STORAGE_TYPE(__fp4_e1m2x2, unsigned char);
__LINX_STORAGE_TYPE(__fp8_e8m0, unsigned char);
__LINX_STORAGE_TYPE(__fp4_hif4x2, unsigned char);
__LINX_STORAGE_TYPE(__int4x2, signed char);
__LINX_STORAGE_TYPE(__uint4x2, unsigned char);

#undef __LINX_STORAGE_TYPE

#define __LINX_PACKED_STORAGE_TYPE(NAME, STORAGE)                            \
  struct NAME {                                                              \
    STORAGE data;                                                            \
  }

__LINX_PACKED_STORAGE_TYPE(__fp16x2, unsigned int);
__LINX_PACKED_STORAGE_TYPE(__bf16x2, unsigned int);
__LINX_PACKED_STORAGE_TYPE(__uint16x2, unsigned int);
__LINX_PACKED_STORAGE_TYPE(__int16x2, unsigned int);
__LINX_PACKED_STORAGE_TYPE(__fp8_e4m3x4, unsigned int);
__LINX_PACKED_STORAGE_TYPE(__fp8_e5m2x4, unsigned int);
__LINX_PACKED_STORAGE_TYPE(__uint8x4, unsigned int);
__LINX_PACKED_STORAGE_TYPE(__int8x4, unsigned int);
__LINX_PACKED_STORAGE_TYPE(__fp8_e6m2x2, unsigned short);
__LINX_PACKED_STORAGE_TYPE(__fp8_e4m3x2, unsigned short);
__LINX_PACKED_STORAGE_TYPE(__fp8_e5m2x2, unsigned short);

#undef __LINX_PACKED_STORAGE_TYPE

template <typename Storage, typename Format>
static __inline__ Storage &__linx_storage_ref(Format &Value) {
  static_assert(sizeof(Storage) == sizeof(Format));
  return *reinterpret_cast<Storage *>(&Value);
}

template <typename Storage, typename Format>
static __inline__ const Storage &__linx_storage_ref(const Format &Value) {
  static_assert(sizeof(Storage) == sizeof(Format));
  return *reinterpret_cast<const Storage *>(&Value);
}

#define __tf32_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __hf32_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __blkc_bf16_STORAGE(value) __linx_storage_ref<unsigned short>(value)
#define __hif8_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp8_e4m3_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp8_e5m2_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp8_e6m2_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp6_e3m2_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp6_e2m3_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp4_e2m1x2_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp4_e1m2x2_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp8_e8m0_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp4_hif4x2_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __int4x2_STORAGE(value) __linx_storage_ref<signed char>(value)
#define __uint4x2_STORAGE(value) __linx_storage_ref<unsigned char>(value)
#define __fp16x2_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __bf16x2_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __blkc_bf16x2_STORAGE(value) __bf16x2_STORAGE(value)
#define __uint16x2_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __int16x2_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __fp8_e4m3x4_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __fp8_e5m2x4_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __uint8x4_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __int8x4_STORAGE(value) __linx_storage_ref<unsigned int>(value)
#define __fp8_e6m2x2_STORAGE(value) __linx_storage_ref<unsigned short>(value)
#define __fp8_e4m3x2_STORAGE(value) __linx_storage_ref<unsigned short>(value)
#define __fp8_e5m2x2_STORAGE(value) __linx_storage_ref<unsigned short>(value)

// TileOP uses this type modifier after the element type. The element count is
// physical storage count, so packed public formats still use byte carriers.
#define tile_size(elements) __attribute__((ext_vector_type(elements)))

#endif // __LINX_BLKC_H
