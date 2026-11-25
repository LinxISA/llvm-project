// RUN: %clang -target linx64v5 -mlxbc -O2 -mllvm -enable-all-vector-as-tilereg=true -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang -target linx64v5 -mlxbc -O1 -DMETADATA_ONLY -Xclang -disable-llvm-passes -S -emit-llvm %s -o - | FileCheck %s --check-prefix=META

namespace pto {
enum class Location { Vec, Mat, Left, Right, Acc, Bias, Scaling, Shared };
enum class BLayout { RowMajor, ColMajor, CubeM16, CubeM32, CubeN8 };
enum class SLayout { NoneBox, RowMajor, ColMajor };
enum class CompactMode { Null, Normal, RowPlusOne };
template <Location Loc, typename DType, int Rows, int Cols, BLayout Layout,
          int ValidRow = Rows, int ValidCol = Cols,
          SLayout Box = SLayout::NoneBox, int BoxBytes = 512, int Pad = 3,
          CompactMode Compact = CompactMode::Null, bool Element = false>
struct Tile {
  int RowMaskInternal, ColMaskInternal, reserved;
  using Carrier = DType __attribute__((ext_vector_type(Rows * Cols)));
  Carrier data_;
  Carrier &data() { return data_; }
};
}
using namespace pto;
using U = Tile<Location::Vec, unsigned, 2, 1024, BLayout::RowMajor>;
using S = Tile<Location::Vec, int, 1, 2048, BLayout::RowMajor>;
using D = Tile<Location::Vec, double, 1, 1024, BLayout::RowMajor>;
using Small = Tile<Location::Vec, unsigned, 1, 16, BLayout::RowMajor>;
using Tail = Tile<Location::Vec, unsigned, 2, 1024, BLayout::RowMajor, 2, 1000>;
using Dynamic = Tile<Location::Vec, unsigned, 2, 1024, BLayout::RowMajor, -1, -1>;
using Predicate = Tile<Location::Vec, bool, 1, 16384, BLayout::RowMajor>;
using Shared = Tile<Location::Shared, unsigned, 2, 1024, BLayout::RowMajor>;
using Box = Tile<Location::Vec, unsigned, 2, 1024, BLayout::RowMajor, 2, 1024, SLayout::RowMajor>;
using Compact = Tile<Location::Vec, unsigned, 2, 1024, BLayout::RowMajor, 2, 1024, SLayout::NoneBox, 512, 3, CompactMode::Normal>;

// IR-LABEL: define {{.*}} @unsigned_carrier(
// IR: call <2048 x i32> @llvm.linx.blk.tload{{.*}}(i64 1024, i64 2, i64 1024, i64 25, i64 0, i64 0, ptr {{.*}}, i64 4096)
// IR: call void @llvm.linx.blk.tstore{{.*}}(i64 1024, i64 2, i64 1024, i64 25, i64 0, ptr {{.*}}, i64 4096,
extern "C" void unsigned_carrier(U &dst, U &src) { dst.data() = src.data(); }
// IR-LABEL: define {{.*}} @signed_carrier(
// IR: call <2048 x i32> @llvm.linx.blk.tload{{.*}}(i64 2048, i64 1, i64 2048, i64 17,
extern "C" void signed_carrier(S &dst, S &src) { dst.data_ = src.data_; }
// IR-LABEL: define {{.*}} @double_carrier(
// IR: call <1024 x double> @llvm.linx.blk.tload{{.*}}(i64 1024, i64 1, i64 1024, i64 0,
extern "C" void double_carrier(D &dst, D &src) { dst.data() = src.data(); }

// A partial/dynamic valid rectangle cannot replace a whole-vector access:
// bytes outside it are still observable. Keep the load and store intact.
// IR-LABEL: define {{.*}} @small_carrier(
// IR-NOT: call {{.*}}@llvm.linx.blk.
// IR: load <16 x i32>
// IR: store <16 x i32>
extern "C" void small_carrier(Small &dst, Small &src) { dst.data() = src.data(); }
// IR-LABEL: define {{.*}} @partial_carrier(
// IR-NOT: call {{.*}}@llvm.linx.blk.
// IR: load <2048 x i32>
// IR: store <2048 x i32>
extern "C" void partial_carrier(Tail &dst, Tail &src) { dst.data() = src.data(); }
// IR-LABEL: define {{.*}} @dynamic_carrier(
// IR-NOT: call {{.*}}@llvm.linx.blk.
// IR: load <2048 x i32>
// IR: store <2048 x i32>
extern "C" void dynamic_carrier(Dynamic &dst, Dynamic &src) { dst.data() = src.data(); }
// IR-LABEL: define {{.*}} @volatile_carrier(
// IR: load volatile <2048 x i32>
// IR: store volatile <2048 x i32>
extern "C" void volatile_carrier(volatile U &dst, volatile U &src) { dst.data_ = src.data_; }

#ifdef METADATA_ONLY
// META-LABEL: define {{.*}} @predicate_carrier(
// META-NOT: !linx.tile.transport
// META: ret void
extern "C" void predicate_carrier(Predicate &dst, Predicate &src) { dst.data_ = src.data_; }
#endif
// META-LABEL: define {{.*}} @shared_carrier(
// META-NOT: !linx.tile.transport
// META: ret void
extern "C" void shared_carrier(Shared &dst, Shared &src) { dst.data_ = src.data_; }
// META-LABEL: define {{.*}} @boxed_carrier(
// META-NOT: !linx.tile.transport
// META: ret void
extern "C" void boxed_carrier(Box &dst, Box &src) { dst.data_ = src.data_; }
// META-LABEL: define {{.*}} @compact_carrier(
// META-NOT: !linx.tile.transport
// META: ret void
extern "C" void compact_carrier(Compact &dst, Compact &src) { dst.data_ = src.data_; }
// META-DAG: !{!"linx.tile.carrier:v1;layout=0;dtype=25;rows=2;cols=1024;vrow=2;vcol=1024"}
// META-DAG: !{!"linx.tile.carrier:v1;layout=0;dtype=17;rows=1;cols=2048;vrow=1;vcol=2048"}
// META-DAG: !{!"linx.tile.carrier:v1;layout=0;dtype=0;rows=1;cols=1024;vrow=1;vcol=1024"}

// This loop lets GVN/PRE hoist a boundary carrier load. Resolve the typed
// transport before that optimization can discard unknown load metadata.
extern "C" void barrier() __attribute__((linx_scalar_shareability("intra")));
extern "C" unsigned typed_loop(U &value, int n) {
  unsigned result = 0;
  for (int i = 0; i < n; ++i) {
    result += value.data()[0];
    barrier();
    auto v = value.data();
    asm volatile("" : "+Tr"(v));
    value.data() = v;
  }
  return result;
}
// IR-LABEL: define {{.*}} @typed_loop(
// IR-NOT: load <2048 x i32>
// IR: call <2048 x i32> @llvm.linx.blk.tload{{.*}}i64 25,
// IR-NOT: load <2048 x i32>
// IR: ret i32
