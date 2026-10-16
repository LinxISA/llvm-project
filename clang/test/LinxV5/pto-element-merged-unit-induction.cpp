// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

using u32 = unsigned;
using row128 = u32 tile_size(128);
using row2048 = u32 tile_size(2048);

__attribute__((always_inline, annotate("pto.element.view:v2;dtype=u32;layout=norm;physical_rows=1;physical_cols=128;valid_rows=1;valid_cols=32")))
inline row128 &tail32(row128 &x) { return x; }
__attribute__((always_inline, annotate("pto.element.view:v2;dtype=u32;layout=norm;physical_rows=1;physical_cols=2048;valid_rows=1;valid_cols=2048")))
inline row2048 &full2048(row2048 &x) { return x; }

extern "C" void merged_unit_induction(u32 *input, u32 *count_output,
                                        u32 *candidate_output, u32 valid,
                                        u32 selected) {
  row2048 keys, counts, candidates;
  asm volatile("BSTART.TLSU TLOAD, U32\n"
               "B.DIM zero, 2048, ->lb0\n"
               "B.DIM zero, 1, ->lb1\n"
               "B.DIM zero, 2048, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\n"
               "B.IOR [%1], []\n"
               : "=Tr"(keys) : "r"(input), "i"(7) : "memory");
  auto &key = full2048(keys);
  auto &count = full2048(counts);
  auto &candidate = full2048(candidates);
#pragma pto element for
  for (u32 element = 0; element < 2048; ++element) {
    const bool take = element < valid && key[element] > selected;
    count[element] = take ? 1u : 0u;
    candidate[element] = take ? element + 1u : 0u;
  }
  asm volatile("BSTART.TLSU TSTORE, U32\n"
               "B.DIM zero, 2048, ->lb0\n"
               "B.DIM zero, 1, ->lb1\n"
               "B.DIM zero, 2048, ->lb2\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1], []\n"
               : : "Tr"(counts), "r"(count_output) : "memory");
  asm volatile("BSTART.TLSU TSTORE, U32\n"
               "B.DIM zero, 2048, ->lb0\n"
               "B.DIM zero, 1, ->lb1\n"
               "B.DIM zero, 2048, ->lb2\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1], []\n"
               : : "Tr"(candidates), "r"(candidate_output) : "memory");
}

extern "C" void tail_u32_add(u32 *input, u32 *count_output,
                              u32 *candidate_output, u32 valid,
                              u32 selected) {
  row128 keys, counts, candidates;
  asm volatile("BSTART.TLSU TLOAD, U32\n"
               "B.DIM zero, 32, ->lb0\n"
               "B.DIM zero, 1, ->lb1\n"
               "B.DIM zero, 128, ->lb2\n"
               "B.IOT mask=1111, last, ->%0<%Z2>\n"
               "B.IOR [%1], []\n"
               : "=Tr"(keys) : "r"(input), "i"(3) : "memory");
  auto &key = tail32(keys);
  auto &count = tail32(counts);
  auto &candidate = tail32(candidates);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    const bool take = element < valid && key[element] > selected;
    count[element] = take ? 1u : 0u;
    candidate[element] = take ? element + 1u : 0u;
  }
  asm volatile("BSTART.TLSU TSTORE, U32\n"
               "B.DIM zero, 32, ->lb0\n"
               "B.DIM zero, 1, ->lb1\n"
               "B.DIM zero, 128, ->lb2\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1], []\n"
               : : "Tr"(counts), "r"(count_output) : "memory");
  asm volatile("BSTART.TLSU TSTORE, U32\n"
               "B.DIM zero, 32, ->lb0\n"
               "B.DIM zero, 1, ->lb1\n"
               "B.DIM zero, 128, ->lb2\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1], []\n"
               : : "Tr"(candidates), "r"(candidate_output) : "memory");
}

// IR-LABEL: define{{.*}} @merged_unit_induction
// IR-NOT: @llvm.linx.experimental.element.region
// IR-NOT: extractelement
// IR-NOT: insertelement
// IR: @llvm.linx.experimental.ew.tcmp.row.v512i32.v2048i32.v2048i32
// IR: %[[FULL_ONE32:.*]] = freeze i32 1
// IR: %[[FULL_ONE64:.*]] = zext i32 %[[FULL_ONE32]] to i64
// IR: @llvm.linx.experimental.ew.tci.row.v2048i32(i64 1, i64 2048, i64 25, i64 0, i64 %[[FULL_ONE64]], i64 0, i64 2048)
// IR: @llvm.linx.experimental.ew.tsel.row.v2048i32.v512i32.v2048i32.v2048i32
// IR: ret void
// IR-LABEL: define{{.*}} @tail_u32_add
// IR: %[[TAIL_ONE32:.*]] = freeze i32 1
// IR: %[[TAIL_ONE64:.*]] = zext i32 %[[TAIL_ONE32]] to i64
// IR: @llvm.linx.experimental.ew.tci.row.v128i32(i64 1, i64 32, i64 25, i64 0, i64 %[[TAIL_ONE64]], i64 0, i64 128)
// OBJ-LABEL: <tail_u32_add>:
// OBJ: BSTART.TEPL{{[ \t]+}}TCI, U32
// OBJ: B.DIM{{.*}}32,{{.*}}->lb0
// OBJ: B.DIM{{.*}}1,{{.*}}->lb1
// OBJ: B.DIM{{.*}}128,{{.*}}->lb2
// OBJ-NOT: BSTART.TEPL{{[ \t]+}}TADD, U32
