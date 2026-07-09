// RUN: %clang++ --target=linx64v5 -O2 -mlxbc -c %s -o %t.o

typedef double tile512 tile_size(512);

template <typename T> struct type_traits;
template <> struct type_traits<double> { static constexpr int TypeCode = 0; };

template <typename T> struct tile_type_traits;
template <> struct tile_type_traits<tile512> {
  static constexpr int TilesizeCode = 9;
};

enum class TmaPadValue : int {
  Zero = 0,
  Max = 1,
  Min = 2,
  Null = 3,
};

template <typename Elem, int ValidColV, int ValidRowV, int ColsV, int RowsV>
struct tile_block {
  using DType = Elem;
  using TileDType = tile512;
  static constexpr int ValidCol = ValidColV;
  static constexpr int ValidRow = ValidRowV;
  static constexpr int Cols = ColsV;
  static constexpr int Rows = RowsV;

  tile512 storage;

  tile512 &data() { return storage; }
  const tile512 &data() const { return storage; }
};

template <typename Elem>
struct global_mem_ref {
  using DType = Elem;

  Elem *ptr;

  Elem *data() const { return ptr; }
};

template <typename tile_shape_out, typename tile_shape_offset, typename gm_shape,
          TmaPadValue Pad = TmaPadValue::Null>
inline void MGATHER(tile_shape_out &dst, const gm_shape &src,
                    const tile_shape_offset &offset) {
  static_assert(tile_shape_offset::ValidCol <= tile_shape_offset::Cols, "");
  asm volatile(
      "BSTART.TLSU MGATHER, %c[DataType]\n"
      "B.DATR %c[PadValue]\n"
      "B.DIM zero, %c[ValidCol], ->LB0\n"
      "B.DIM zero, %c[ValidRow], ->LB1\n"
      "B.DIM zero, %c[Col], ->LB2\n"
      "B.IOT [%[off]], last, ->%[dst]<%c[TileSize]>\n"
      "B.IOR [%[base]], []\n"
      : [dst] "=Tr"(dst.data())
      : [base] "r"(src.data()), [off] "Tr"(offset.data()),
        [DataType] "i"(type_traits<typename tile_shape_out::DType>::TypeCode),
        [PadValue] "i"(static_cast<int>(Pad)),
        [TileSize] "i"(
            tile_type_traits<typename tile_shape_out::TileDType>::TilesizeCode),
        [ValidCol] "i"(tile_shape_offset::ValidCol),
        [ValidRow] "i"(tile_shape_offset::ValidRow),
        [Col] "i"(tile_shape_offset::Cols)
      : "memory");
}

template <typename tile_shape_in, typename tile_shape_offset, typename gm_shape>
inline void MSCATTER(gm_shape &dst, const tile_shape_in &src,
                     const tile_shape_offset &offset) {
  static_assert(tile_shape_offset::ValidCol <= tile_shape_offset::Cols, "");
  asm volatile(
      "BSTART.TLSU MSCATTER, %c[DataType]\n"
      "B.DIM zero, %c[ValidCol], ->LB0\n"
      "B.DIM zero, %c[ValidRow], ->LB1\n"
      "B.DIM zero, %c[Col], ->LB2\n"
      "B.IOT [%[src], %[off]], last\n"
      "B.IOR [%[base]], []\n"
      :
      : [base] "r"(dst.data()), [src] "Tr"(src.data()),
        [off] "Tr"(offset.data()),
        [DataType] "i"(type_traits<typename tile_shape_in::DType>::TypeCode),
        [ValidCol] "i"(tile_shape_offset::ValidCol),
        [ValidRow] "i"(tile_shape_offset::ValidRow),
        [Col] "i"(tile_shape_offset::Cols)
      : "memory");
}

template <typename tile_shape_out, typename tile_shape_offset,
          typename tile_shape_mask, typename gm_shape,
          TmaPadValue Pad = TmaPadValue::Null>
inline void MGATHER_MASK(tile_shape_out &dst, const gm_shape &src,
                         const tile_shape_offset &offset,
                         const tile_shape_mask &mask) {
  asm volatile(
      "BSTART.TLSU MGATHER.MASK, %c[DataType]\n"
      "B.DATR %c[PadValue]\n"
      "B.DIM zero, %c[Col], ->LB0\n"
      "B.DIM zero, %c[Row], ->LB1\n"
      "B.IOT [%[off]], ->%[dst]<%c[TileSize]>\n"
      "B.IOT [%[mask]], last\n"
      "B.IOR [%[base]], []\n"
      : [dst] "=Tr"(dst.data())
      : [base] "r"(src.data()), [off] "Tr"(offset.data()),
        [mask] "Tr"(mask.data()),
        [DataType] "i"(type_traits<typename tile_shape_out::DType>::TypeCode),
        [PadValue] "i"(static_cast<int>(Pad)),
        [TileSize] "i"(
            tile_type_traits<typename tile_shape_out::TileDType>::TilesizeCode),
        [Col] "i"(tile_shape_offset::Cols), [Row] "i"(tile_shape_offset::Rows)
      : "memory");
}

template <typename tile_shape_in, typename tile_shape_offset,
          typename tile_shape_mask, typename gm_shape>
inline void MSCATTER_MASK(gm_shape &dst, const tile_shape_in &src,
                          const tile_shape_offset &offset,
                          const tile_shape_mask &mask) {
  asm volatile(
      "BSTART.TLSU MSCATTER.MASK, %c[DataType]\n"
      "B.DIM zero, %c[Col], ->LB0\n"
      "B.DIM zero, %c[Row], ->LB1\n"
      "B.IOT [%[src], %[off]]\n"
      "B.IOT [%[mask]], last\n"
      "B.IOR [%[base]], []\n"
      :
      : [base] "r"(dst.data()), [src] "Tr"(src.data()),
        [off] "Tr"(offset.data()), [mask] "Tr"(mask.data()),
        [DataType] "i"(type_traits<typename tile_shape_in::DType>::TypeCode),
        [Col] "i"(tile_shape_offset::Cols), [Row] "i"(tile_shape_offset::Rows)
      : "memory");
}

using data_tile = tile_block<double, 16, 8, 16, 8>;
using padded_offset_tile = tile_block<double, 12, 6, 16, 8>;
using full_offset_tile = tile_block<double, 16, 8, 16, 8>;
using mask_tile = tile_block<double, 16, 8, 16, 8>;

void test_tma_gather_scatter_mask(double *base_ptr) {
  data_tile dst;
  data_tile src;
  padded_offset_tile padded_offset;
  full_offset_tile full_offset;
  mask_tile mask;
  global_mem_ref<double> base{base_ptr};

  MGATHER<data_tile, padded_offset_tile, global_mem_ref<double>,
          TmaPadValue::Zero>(dst, base, padded_offset);
  MSCATTER(base, src, padded_offset);
  MGATHER_MASK<data_tile, full_offset_tile, mask_tile, global_mem_ref<double>,
               TmaPadValue::Zero>(dst, base, full_offset, mask);
  MSCATTER_MASK(base, src, full_offset, mask);
}
