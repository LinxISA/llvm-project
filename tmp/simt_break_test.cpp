extern "C" unsigned short blkv_get_index_x();

extern "C" void __mtc__ simt_break_test(const volatile int *in, int *out,
                                        int max_probe) {
  int tid = blkv_get_index_x();
  int acc = 0;

  for (int i = 0; i < max_probe; ++i) {
    int v = in[tid + i];
    if ((tid & 1) && i == 3) {
      acc = v;
      break;
    }
    acc += v;
  }

  out[tid] = acc;
}
