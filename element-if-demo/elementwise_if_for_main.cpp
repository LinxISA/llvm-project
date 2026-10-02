using tile = float tile_size(128);

extern void elementwise_if_for(tile &, const tile &, const tile &);

alignas(32) static tile output_tile;
alignas(32) static tile lhs_tile;
alignas(32) static tile rhs_tile;

extern "C" int main() {
  elementwise_if_for(output_tile, lhs_tile, rhs_tile);
  return 0;
}
