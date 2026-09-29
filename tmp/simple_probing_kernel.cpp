#include <stdint.h>
extern "C" unsigned short blkv_get_index_x();
extern "C" unsigned short blkv_get_index_y();

constexpr uint32_t kSlotsPerKey = 8;

template <int32_t thread_num, int32_t num_batches>
void __mtc__ lookup(uint8_t *__restrict__ slot,
                    int64_t *__restrict__ keys,
                    int32_t *__restrict__ values_output,
                    uint32_t *__restrict__ hashes_output,
                    uint32_t capacity,
                    int32_t num,
                    int32_t entry_size,
                    int32_t max_probe) {
  int32_t tid = blkv_get_index_x();
  int32_t batch = blkv_get_index_y();
  int32_t idx = batch * thread_num + tid;
  if (idx < num) {
    int64_t key = keys[idx];
    uint32_t h = (uint32_t)((uint64_t)key * kSlotsPerKey);
    hashes_output[idx] = h;

    uint32_t curr_slot = h % capacity;

    for (int probe_cnt = 0; probe_cnt < max_probe; probe_cnt++) {
      int64_t *key_addr = (int64_t *)(slot + entry_size * curr_slot);
      int64_t curr_key = *key_addr;
      int32_t *value_addr =
          (int32_t *)(slot + entry_size * curr_slot + sizeof(int64_t));
      int32_t curr_val = value_addr[0];

      if (curr_key == key) {
        values_output[idx] = curr_val;
        probe_cnt = max_probe;
      }
      curr_slot = (curr_slot + 1u) % capacity;
    }
  }
}

template void lookup<256, 1>(uint8_t *__restrict__, int64_t *__restrict__,
                             int32_t *__restrict__, uint32_t *__restrict__,
                             uint32_t, int32_t, int32_t, int32_t);
