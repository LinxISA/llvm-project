using FT = float __attribute__((matrix_type(16, 16)));
__vec__ void test_branch(FT __in__ TA, FT __in__ TB, FT __out__ TC, int num) {
    float* ptrA = blkv_get_tile_ptr(TA);
    float* ptrB = blkv_get_tile_ptr(TB);
    float* ptrC = blkv_get_tile_ptr(TC);
    int x = blkv_get_index_x();

    for (int i = 0; i < x; i++) {
        ptrC[i] = ptrA[i] * ptrB[i];
    }
}