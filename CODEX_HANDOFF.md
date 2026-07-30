# Codex 工作交接记录

> 记录日期：2026-07-30
> LLVM 仓库：`/home/zhuwei/linx-llvm`
> TileOP API 仓库：`/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API`
> LLVM 远端分支：`linxisa` → `git@github.com:LinxISA/llvm-project.git`，分支 `dev-llvm15-v5-encoding`（规定远端分支 `dev-llvm15_56`）
> TileOP API 远端：`origin` → `git@github.com:LinxISA/Linx-TileOP-API.git`，分支 `linx`

## 重启后如何继续

重新启动 Codex 后输入：

```text
请读取 /home/zhuwei/linx-llvm/CODEX_HANDOFF.md，并继续当前工作。
```

## 关键上下文

### 权威来源

- **网站开发人员手册**（唯一权威）：`https://pto-isa.github.io/SuperNPUBench/`
- **ISA intrinsic 文档**：`hengliao1972/DavinciOO` 分支 `codex/update-intrinsic-docs`（HEAD `3b4fe5e`），本地 `/tmp/DavinciOO-intrinsic`
- 后续实现全部对齐网站手册，命名、签名以网站为准

### 编译配置

```bash
# linx-llvm build（本地开发）
cmake -S llvm -B build -G Ninja \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD=LinxV5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=On

# 交叉编译测试
build/bin/clang-15 \
  --target=linx64v5-unknown-linux-musl -mlxbc -fenable-matrix -O2 -std=c++20 \
  -D__linx -DENABLE_TENSOR_INSTR \
  --sysroot=/home/zhuwei/linx-BLK-build/output/linx_blockisa_llvm_musl/sysroot \
  -c -I<tileop-api>/test/common -I<tileop-api>/include \
  -I<SuperNPUBench>/test/common -I<SuperNPUBench>/test/common/src \
  test.cpp -o test.o

# 回归测试
cd /home/zhuwei/linx-llvm && build/bin/llvm-lit -v \
  clang/test/LinxV5/blk_builtin_call.cpp \
  llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll

# 同步 tileop-api 头到 build resource-dir
cp -a /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/include/. \
  /home/zhuwei/linx-llvm/build/lib/clang/15.0.4/include/tileop-api/

# 推送
git push linxisa dev-llvm15-v5-encoding  # linx-llvm
git push origin linx                       # tileop-api
```

### 已完成的全部工作

#### v5 Encoding 落地（12 commit）

| commit | 内容 |
|---|---|
| `d832931` | B.IOT v5 bit layout（PE_MASK/TSize/Func/DstTile） |
| `52f7452` | 删 reuse（S0R/S1R/KillFlag/_RU + RegReuseMarker pass） |
| `5d8c883` | C.B.IOS 指令定义 |
| `32e0b5a` | C.B.IOS operand parse + SharedTID |
| `92568b0` | 删 ACCCVT（v5 removed） |
| `22a519f` | TLSU Function 8-13 enum + parse |
| `1ccfedf` | TileOpMap TLSU Function 8-13 符号名 |
| `3a4e115` | B.IOT operand parse（PE_MASK/TSize/->dst） |
| `aa57b0d` | get_thread_idx rename + IntrNoMem |
| `327efe5` | TCopyOut blk_tstore + parseTileDstWithSize v5 + TileOpExpand operand order |
| `74f3c0f` | Relax ClockhandsColoring + TRegToOffset |
| `23de7d8` | 4 个基础 Local FIXP 变体（BIAS/MX/MX_BIAS/MX_ACC） |

#### v5 Encoding 覆盖

- B.IOT: PE_MASK[18:15], TSize[11:9], Func[14:12], DstTile[8:7], Last[19]
- C.B.IOS: 16-bit Shared operand binder（替换 C.B.DIM RegSrc）
- TLSU Function 8-13: TMOV 4 mode + TSTORE.SPART + GMOV
- TileOpExpand: 全部 B.IOT 构造适配 v5 operand 顺序
- template_asm.hpp: 143 处 B.IOT inline-asm 从 v4→v5 转换
- parseTileDstWithSize: 兼容 v5 `->$N`（无 `<size>`）
- TLOAD static_assert: Shared tile (Right/ScaleRight) 放宽至 32KB

#### FIXP 实现（全部 6 个变体）

| 接口 | Function | 状态 |
|---|---|---|
| `TMATMUL_FIXP` | 9 | ✅ 完整编译链 |
| `TMATMUL_BIAS_FIXP` | 10 | ✅ 完整编译链 |
| `TMATMUL_ACC_FIXP` | 11 | ✅ 完整编译链 |
| `TMATMUL_MX_FIXP` | 12 | ✅ 完整编译链 |
| `TMATMUL_MX_BIAS_FIXP` | 13 | ✅ 完整编译链 |
| `TMATMUL_MX_ACC_FIXP` | 14 | ✅ 完整编译链 |

每个 FIXP 变体覆盖：Clang builtin → Sema → LLVM intrinsic → ISD node → lowering → ISel → pseudo → MC expansion → API wrapper → 测试。

ACC 变体使用 `ACC_TILE_SRC` 机器依赖（ACC 不编为 B.IOT source）。

#### get_thread_idx

- `__builtin_linx_get_thread_idx`（clang builtin）
- `int_linx_get_thread_idx`（IR intrinsic, IntrNoMem, 无参数）
- SSR_GET Imm12=0xFFF（后端 lowering, INTRINSIC_WO_CHAIN, 无 chain output）
- tileop-api: `get_thread_idx()` + backward-compat `get_thread_id()`
- PEID SSR `0x0802`（handoff 文档里提到的版本）

### 测例验证状态

| 测例 | 参数 | 结果 |
|---|---|---|
| vec/tadd.cpp | 默认 16×16 | ✅ 编译通过 |
| matmul.cpp | tM=16 tK=32 tN=64, 删 ACCCVT | ✅ 编译通过 |
| fa_2d_unroll_gmma.cpp | Tm=16 Tk=64, 修 TMATMUL 顺序+删 ACCCVT | ❌ boxed layout tile register name (tile_n2) 不认 |

编译通过的测例文件在 `/home/zhuwei/docs/test_cases/`。

fa 的阻塞是既有问题：boxed layout tile（SFractal != NoneBox）的 register 名 `tile_t2`/`tile_n2` 等不在 `MatchLinxV5TileRegisteName` 的 StringSwitch 里。

### 已知遗留问题

1. **fa boxed layout tile register name**：`MatchLinxV5TileRegisteName` 不认 `tile_t2`/`tile_u2`/`tile_m2`/`tile_n2` 等 boxed layout register 名。B.IOT inline-asm 用 `%5`（`Tr` 约束）展开后产生 `tile_n2`，AsmParser 不认 → Match Instruction Error。
2. **ACCCVT removed**：v5 删除 ACCCVT（status: removed）。matmul/fa 测试代码需要去掉 `ACCCVT` 调用。替代方案未定（可能用 TCVT 或 TSTORE from Acc）。
3. **`__mtc__` kernel ISel crash**：TCopyOut_Vec_RowMajor 等用 `<<<>>>` launch 的 `__mtc__` kernel 在 ISel 里 `RegClass must be allocatable` crash。通过 `-DENABLE_TENSOR_INSTR` 走 blk_tstore builtin 绕过。

### 仍缺失（按工作包）

- **工作包 B**：统一 FIXP descriptor（PreQuantMode/ReLU/Quant/RowMax/GroupMax/多 destination）
- **工作包 C**：SharedTile SSA/version 管理 + TMATMUL/TGEMV Shared Right + MX 双 binder
- **工作包 D**：Shared TLOAD/TSTORE + `TSTORE<pe_scope>` SPART
- **工作包 E**：TGEMV Function 16–22
- **工作包 F**：RecordEvent/WaitEvents/pe_scope/core_scope + Core4 convergence

### 关键设计约束（必须保留）

- ACC 是隐式状态，使用 `ACC_TILE_SRC`，不占普通 Tile source slot，不生成 `B.IOT ACC`
- 每个 FIXP opcode 只生成一个全零 `B.FPATR 0,0,0,0,0,0,0`（基础模式）
- PE_MASK 初始化为全 1（`0b1111`）
- 不要用 TileOP API inline asm 绕过 builtin/intrinsic
- 不要把 ACC 当普通 Tile 输入或输出
- 不要修改或删除用户已有未跟踪文件（`MultiThreadAdd.cpp` 等 patch 文件）

## 修改的文件清单

### linx-llvm

```
clang/include/clang/Basic/BuiltinsLinxV5.def
clang/lib/CodeGen/CGBuiltin.cpp
clang/lib/Sema/SemaChecking.cpp
clang/test/LinxV5/blk_builtin_call.cpp
llvm/include/llvm/IR/IntrinsicsLinx.td
llvm/lib/Target/LinxV5/LinxV5ISelLowering.h
llvm/lib/Target/LinxV5/LinxV5ISelLowering.cpp
llvm/lib/Target/LinxV5/LinxV5ISelDAGToDAG.cpp
llvm/lib/Target/LinxV5/LinxV5InstrInfo.td
llvm/lib/Target/LinxV5/LinxV5RegisterInfo.td
llvm/lib/Target/LinxV5/LinxV5RegisterInfoSIMT.td
llvm/lib/Target/LinxV5/LinxV5RegisterCanonicalization.cpp
llvm/lib/Target/LinxV5/LinxV5InstrInfo.cpp
llvm/lib/Target/LinxV5/LinxV5ClockhandsColoring.cpp
llvm/lib/Target/LinxV5/LinxV5TRegToOffset.cpp
llvm/lib/Target/LinxV5/LinxV5.h
llvm/lib/Target/LinxV5/CMakeLists.txt
llvm/lib/Target/LinxV5/AsmParser/LinxV5AsmParser.cpp
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5BaseInfo.h
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5InstPrinter.cpp
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5InstPrinter.h
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5MCCodeEmitter.cpp
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.cpp
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.h
llvm/lib/Target/LinxV5/Disassembler/LinxV5Disassembler.cpp
llvm/lib/Target/LinxV5/LinxV5TargetMachine.cpp
llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll
llvm/test/CodeGen/LinxV5/v5-shared-gmov.ll
llvm/test/CodeGen/LinxV5/peid.ll
llvm/test/MC/LinxV5/v5-shared-cube-encoding.s
```

### TileOP API

```
include/jcore/template_asm.hpp
include/jcore/type.hpp
include/common/pto_tileop.hpp
test/common/Makefile.common
docs/tileop-usage/README.md
docs/tileop-usage/constraints.md
docs/tileop-usage/cube.md
docs/tileop-usage/davincioo-alignment-status.md
```

### 其他文档

```
/home/zhuwei/docs/davincioo_4pe_programming_interface.md
/home/zhuwei/docs/davincioo_4pe_frontend_feasibility.md
/home/zhuwei/docs/davincioo_v5_encoding_implementation.md
/home/zhuwei/docs/get_thread_idx_implementation.md
/home/zhuwei/docs/test_cases/README.md
/home/zhuwei/docs/test_cases/vec_tadd_compiles.cpp
/home/zhuwei/docs/test_cases/matmul_compiles.cpp
```

## 终端乱码事故

不要在 Codex 工具中执行 `reset` 或向外层终端输出字符集控制序列。如果乱码，退出 Codex、关终端、重开。
