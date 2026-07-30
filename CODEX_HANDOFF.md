# Codex 工作交接记录

> 记录日期：2026-07-30
> LLVM 仓库：`/home/zhuwei/linx-llvm`
> TileOP API 仓库：`/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API`

## 重启后如何继续

重新启动 Codex 后输入：

```text
请读取 /home/zhuwei/linx-llvm/CODEX_HANDOFF.md，并继续当前工作。
```

## 当前目标与背景

用户要求将 LLVM/TileOP API 与以下 DavinciOO 参考目录对比并修复缺失：

- 仓库：`hengliao1972/DavinciOO`
- 分支：`codex/update-intrinsic-docs`
- 目录：`isa/intrinsic`
- 已确认远端最新提交：`3b4fe5e6f7f95d008fc05f6ecb5d1a2acef9fce1`
- 提交日期：2026-07-23
- 本地参考克隆：`/tmp/DavinciOO-intrinsic`

最初重点包括：

- SharedTReg 定义和指令
- core-scope TMATMUL 指令簇
- GMOV
- PEID/SSR
- 所有相关 encoding
- TileOP API `template_asm.hpp` 刷新和可用性

## 本轮已经完成的 FIXP 实现

用户指出 `TMATMUL_FIXP` 和 `TMATMUL_ACC_FIXP` 没有实现，现已补齐基础 Local form。

### Clang/LLVM 编译链

已增加：

- Clang builtin：
  - `blk_matmul_fixp`
  - `blk_matmul_acc_fixp`
- LLVM intrinsic：
  - `llvm.linx.blk.matmul.fixp`
  - `llvm.linx.blk.matmul.acc.fixp`
- LinxV5 ISD 节点
- DAG lowering 和 instruction selection
- CodeGen pseudo：
  - `PseudoMAMULB_FIXP_SizeI`
  - `PseudoMAMULB_ACC_FIXP_SizeI`
- MC expansion：
  - `BSTART.CUBE TMATMUL.FIXP` / Function 9
  - `BSTART.CUBE TMATMUL.ACC.FIXP` / Function 11
  - `B.DATR`
  - 固定基础模式 `B.FPATR 0, 0, 0, 0, 0, 0, 0`
  - A/B non-final `B.IOT`
  - 普通 Tile destination final `B.IOT`

关键设计：`TMATMUL_ACC_FIXP` 的 ACC 使用专用 `ACC_TILE_SRC` 机器依赖，不作为普通可分配 Tile，也不进入 `B.IOT`。这避免了之前 inline asm 方案触发的 register coalescing 崩溃。

### TileOP API

已在以下文件加入高层 wrapper：

- `/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/include/jcore/template_asm.hpp`

接口：

```cpp
TMATMUL_FIXP(d, a, b);
TMATMUL_ACC_FIXP(d, acc, a, b);
```

当前约束：

- 基础 Local form
- `d` 为普通 Local Tile，不是 ACC
- `a` 为 `Location::Left`
- `b` 为 `Location::Right`
- ACC form 的 `acc` 为 `Location::Acc`
- 形状必须匹配
- destination logical Tile size 为 512 B–32 KB
- 当前不支持 quant、ReLU、RowMax、GroupMax 等高级模式

## FIXP 验证结果

已成功构建：

```bash
cmake --build build --target llvm-mc llvm-objdump clang llc -j2
```

已通过回归测试：

```bash
build/bin/llvm-lit -v \
  clang/test/LinxV5/blk_builtin_call.cpp \
  llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll
```

结果：2/2 PASS。

真实 TileOP API 实例化也已经成功编译和反汇编。使用的正确交叉编译配置为：

```bash
build/bin/clang++ \
  --target=linx64v5-unknown-linux-musl \
  --sysroot=/home/zhuwei/linx-BLK-build/output/linx_blockisa_llvm_musl/sysroot \
  -std=c++20 -O2 -mlxbc -fenable-matrix \
  -mllvm -enable-all-vector-as-tilereg=true \
  -c test.cpp -o test.o
```

反汇编确认：

```asm
BSTART.CUBE TMATMUL.FIXP, FP32
B.DATR FP32, byte0, Null
B.FPATR 0, 0, 0, 0, 0, 0, 0
B.IOT A, B, mask=1111
B.IOT mask=1111, TSize=<有效值>, last, ->D

BSTART.CUBE TMATMUL.ACC.FIXP, FP32
B.DATR FP32, byte0, Null
B.FPATR 0, 0, 0, 0, 0, 0, 0
B.IOT A, B, mask=1111
B.IOT mask=1111, TSize=<有效值>, last, ->D
```

## DavinciOO 对齐结论

不能宣称与 DavinciOO `isa/intrinsic` 全量对齐。

准确状态：

- encoding 层大体对齐
- 基础 FIXP/GMOV/PEID 可用
- 完整公开 API、Shared 语义、高级 FIXP、TGEMV 仍有缺失

### 已对齐或基本对齐

- CUBE Function 0/1/2、4/5/6、9–14 的枚举、parser、printer
- `B.FPATR` 七字段 encoding
- `TMATMUL_FIXP` 基础 Local form
- `TMATMUL_ACC_FIXP` 基础 Local form及隐式 ACC dependency
- `TMATMUL_BIAS_FIXP` 基础 Local form（2026-07-30 补齐）
- `TMATMUL_MX_FIXP` 基础 Local form（2026-07-30 补齐）
- `TMATMUL_MX_BIAS_FIXP` 基础 Local form（2026-07-30 补齐）
- `TMATMUL_MX_ACC_FIXP` 基础 Local form（2026-07-30 补齐）
- 全部 6 个 FIXP 变体（Function 9–14）完整编译链：builtin/intrinsic/ISD/pseudo/MC expand/API
- v5 TSize：0 implicit，1–7 对应 512 B–32 KB
- GMOV Function 13 基础 encoding和编译链
- PEID SSR `0x0802`，`get_thread_id()` 和兼容 `get_thread_idx()`
- Shared TMOV Function 8–11 基础 encoding
- `C.B.IOS` 基础支持
- `TSTORE.SPART` Function 12 的 MC 名称/枚举

### 仍缺失

- 完整 FIXP 高级模式（工作包 B，descriptor 设计）：
  - PreQuantMode
  - PreQuantMode
  - PReLU/LReLU
  - Tile/GPR quant 参数
  - RowMax/GroupMax/MaxAbs
  - 多 destination 紧凑编码
  - rounding/saturation 和输出 dtype 完整推导
- 所有 TMATMUL/TGEMV 的 Shared Right overload
- MX 连续双 Shared binder（Right + ScaleRight）
- 编译器管理的 Shared ID/version/defined_mask/SSA 生命周期
- 当前 Shared TMOV API仍显式接受模板 `SharedId`，与参考最终公开语义不一致
- Shared TLOAD/TSTORE 和 `TSTORE<pe_scope>` / SPART 完整编译链及验证
- TGEMV Function 16–22 指令簇
- `RecordEvent`、`WaitEvents`、`pe_scope`、`core_scope`
- GMOV/Shared CUBE Core4 静态收敛性验证

## 对齐状态文档

已经生成完整状态文档：

- `/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/docs/tileop-usage/davincioo-alignment-status.md`

并在文档目录加入入口：

- `/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/docs/tileop-usage/README.md`

状态文档包含：

- 参考分支/提交/核对日期
- 已对齐项
- 未对齐项
- 验证情况
- 建议后续实现顺序

## Clang resource-dir 刷新

发现 `build/lib/clang/15.0.4/include/tileop-api` 是 TileOP API 的缓存副本，之前没有刷新。

本轮已使用以下方式将整个源码 include 树同步到 build resource-dir：

```bash
cp -a \
  /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/include/. \
  /home/zhuwei/linx-llvm/build/lib/clang/15.0.4/include/tileop-api/
```

注意：安装工具链的另一份 resource-dir 仍可能是旧版本：

```text
/home/zhuwei/linx-BLK-build/output/linx_blockisa_llvm_musl/lib/clang/15.0.4/include/tileop-api
```

不要直接用旧安装工具链验证新 builtin，除非重新安装/同步新 Clang 和 API。

## LLVM 当前相关修改文件

本轮及之前相关修改包括：

- `clang/include/clang/Basic/BuiltinsLinxV5.def`
- `clang/lib/CodeGen/CGBuiltin.cpp`
- `clang/lib/Sema/SemaChecking.cpp`
- `clang/test/LinxV5/blk_builtin_call.cpp`
- `llvm/include/llvm/IR/IntrinsicsLinx.td`
- `llvm/lib/Target/LinxV5/LinxV5ISelLowering.h`
- `llvm/lib/Target/LinxV5/LinxV5ISelLowering.cpp`
- `llvm/lib/Target/LinxV5/LinxV5ISelDAGToDAG.cpp`
- `llvm/lib/Target/LinxV5/LinxV5InstrInfo.td`
- `llvm/lib/Target/LinxV5/LinxV5VBXInstrInfo.td`
- `llvm/lib/Target/LinxV5/AsmParser/LinxV5AsmParser.cpp`
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5BaseInfo.h`
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5InstPrinter.cpp`
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5MCCodeEmitter.cpp`
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.cpp`
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.h`
- `llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll`
- `llvm/test/CodeGen/LinxV5/v5-shared-gmov.ll`
- `llvm/test/MC/LinxV5/v5-shared-cube-encoding.s`
- `clang/test/LinxV5/peid.cpp`
- `llvm/test/CodeGen/LinxV5/peid.ll`

仓内还有很多用户已有或其他工作的未跟踪文件，不要擅自删除或覆盖。

## TileOP API 当前相关修改文件

- `include/jcore/type.hpp`
- `include/jcore/template_asm.hpp`
- `include/jcore/TCvt.hpp`
- `include/common/pto_tileop.hpp`
- `docs/tileop-usage/README.md`
- `docs/tileop-usage/constraints.md`
- `docs/tileop-usage/cube.md`
- `docs/tileop-usage/davincioo-alignment-status.md`

## 必须保留的未跟踪文件

不要修改或删除：

```text
/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/test/tileop_api/src/MultiThreadAdd.cpp
```

该文件是用户已有的未跟踪文件。

## 建议下一步

如果用户要求继续补齐所有缺失，建议顺序：

1. 实现其余四个 `TMATMUL*.FIXP` 基础 Local builtin/intrinsic/pseudo。
2. 设计统一 FIXP descriptor，支持完整 `B.FPATR` mode 和多输入/多输出。
3. 引入编译器管理的 SharedTile SSA/version，而不是公开 `SharedId`。
4. 实现 TMATMUL Shared Right 和 MX 双 binder。
5. 实现 Shared TLOAD/TSTORE 和 `TSTORE.SPART`。
6. 实现 TGEMV Function 16–22。
7. 接入 RecordEvent、scope 和 Core4 convergence verifier。

## 终端乱码事故

本轮曾在 Codex 子 PTY 中执行：

```bash
printf '\033(B\033)B\017'
reset
```

控制序列污染了 Codex 外层界面的字符集，导致文字显示为 DEC 图形字符。文件内容没有损坏。

最可靠恢复方法：

1. 退出 Codex。
2. 关闭当前终端标签页/窗口。
3. 新开终端。
4. 重新启动 Codex。
5. 让 Codex读取本交接文件。

不要再次通过 Codex 工具执行 `reset` 或向外层终端输出字符集控制序列。

## 可直接实现部分：后续 Agent 设计与实施指南

### 结论与范围

以下缺失项并不是因为 DavinciOO 在线文档没有定义。参考提交
`3b4fe5e6f7f95d008fc05f6ecb5d1a2acef9fce1` 的 `isa/intrinsic` 已经给出
opcode、header、operand 顺序、Local/Shared form 和主要合法性约束，可以继续实现：

1. 其余四个基础 Local FIXP：
   - `TMATMUL_BIAS_FIXP`
   - `TMATMUL_MX_FIXP`
   - `TMATMUL_MX_BIAS_FIXP`
   - `TMATMUL_MX_ACC_FIXP`
2. 完整 FIXP descriptor 和高级 `B.FPATR` mode。
3. TMATMUL/TGEMV Shared Right，以及 MX 的 Shared Right + Shared ScaleRight。
4. Shared TLOAD/TSTORE 和 `TSTORE<pe_scope>` / `TSTORE.SPART`。
5. TGEMV Function 16–22。
6. `RecordEvent`、`WaitEvents`、`pe_scope`、`core_scope` 和 Core4 convergence 检查。

其中第 1 项最适合直接实现，且不依赖新的 Shared SSA 或 event ABI。第 2–6 项也有
参考定义，但应按阶段设计，不能只靠 TileOP API inline asm 拼接完成。

参考文档位于：

```text
/tmp/DavinciOO-intrinsic/isa/intrinsic
```

重点文件：

```text
TMATMUL_*_FIXP.md
TGEMV*.md
TLOAD.md
TSTORE.md
TMOV.md
GMOV.md
header/BSTART.CUBE.md
header/BSTART.TLSU.md
header/B.FPATR.md
header/B.DATR.md
header/B.IOT.md
header/B.IOR.md
header/C.B.IOS.md
```

### 实施总原则

- 不要把“MC 能编码 opcode”等同于“编译链/API 已实现”。每个公开操作至少要覆盖：
  Clang builtin、Sema、LLVM intrinsic、lowering、ISel、pseudo、MC expansion、API 和测试。
- 优先复用现有 `blk_matmul_fixp` / `blk_matmul_acc_fixp` 路径，不要另建一套
  inline asm-only FIXP 实现。
- FIXP ACC variant 的 ACC 必须继续使用 `ACC_TILE_SRC` 机器依赖。ACC 是隐式状态，
  不占普通 Tile source slot，也不生成 `B.IOT ACC`。
- 所有 FIXP opcode 必须生成且只生成一个 `B.FPATR`。基础模式固定为：

  ```asm
  B.FPATR 0, 0, 0, 0, 0, 0, 0
  ```

- 基础 Local form 暂时固定 `RMode.NONE`、saturation off，不实现 quant、ReLU、max
  或多 destination；这部分应通过后续 descriptor 扩展，而不是给每个 opcode复制参数。
- FIXP 完成后隐式 ACC invalid。非 ACC FIXP 不读取旧 ACC；ACC FIXP 建立 ACC RAW，
  但 ACC 不作为普通可分配 Tile。
- Shared form 不应在公开 API 中永久暴露手工 `SharedId`。当前显式 `SharedId` wrapper
  只能作为低层过渡和 MC 验证手段。
- 修改 TileOP API 后同步源码 include 树到：

  ```text
  /home/zhuwei/linx-llvm/build/lib/clang/15.0.4/include/tileop-api/
  ```

### 工作包 A：补齐四个基础 Local FIXP

这是当前最低风险、可独立完成的工作包。以现有两个 FIXP 为模板，新增四套
builtin/intrinsic/ISD/pseudo/expansion/API，但尽量抽取公共 helper，避免六个变体
长期复制同一套 `B.DATR + B.FPATR + B.DIM + B.IOT` 逻辑。

#### A1. `TMATMUL_BIAS_FIXP`

基础语义：

```text
D = FIXP(A * B + Bias)
```

Local source 顺序：

```text
A, B, Bias
```

基础展开：

```asm
BSTART.CUBE TMATMUL.BIAS.FIXP, AType
B.DATR      BType, RMode.NONE, Sat.Off
B.FPATR     0, 0, 0, 0, 0, 0, 0
B.DIM       M, 0, ->LB0
B.DIM       N, 0, ->LB1
B.DIM       K, 0, ->LB2
B.IOT       A, B, mask=PE_MASK
B.IOT       Bias, mask=PE_MASK, last, ->D<TSize>
```

建议 API：

```cpp
TMATMUL_BIAS_FIXP(d, a, b, bias);
```

约束：D 为普通 Local Tile；A 为 Left；B 为 Right；Bias 为 Local；Bias profile 和
shape 应复用已有非 FIXP `TMATMUL_BIAS` 校验规则；不读取旧 ACC。

#### A2. `TMATMUL_MX_FIXP`

基础语义：

```text
D = FIXP(MXMatMul(A, ScaleA, B, ScaleB))
```

Local source 顺序：

```text
A, ScaleA, B, ScaleB
```

基础展开：

```asm
BSTART.CUBE TMATMULMX.FIXP, AType
B.DATR      BType, RMode.NONE, Sat.Off
B.FPATR     0, 0, 0, 0, 0, 0, 0
B.DIM       M, 0, ->LB0
B.DIM       N, 0, ->LB1
B.DIM       K, 0, ->LB2
B.IOT       A, ScaleA, mask=PE_MASK
B.IOT       B, ScaleB, mask=PE_MASK
B.IOT       mask=PE_MASK, last, ->D<TSize>
```

建议 API：

```cpp
TMATMUL_MX_FIXP(d, a, scale_a, b, scale_b);
```

约束：A/ScaleA/B/ScaleB 均为 Local；Right 与 ScaleRight 的 shape/dtype/profile
校验复用已有非 FIXP `TMATMUL_MX`；不读取旧 ACC。

#### A3. `TMATMUL_MX_BIAS_FIXP`

基础语义：

```text
D = FIXP(MXMatMul(A, ScaleA, B, ScaleB) + Bias)
```

Local source 顺序：

```text
A, ScaleA, B, ScaleB, Bias
```

基础展开建议保持 source 紧凑顺序：

```asm
BSTART.CUBE TMATMULMX.BIAS.FIXP, AType
B.DATR      BType, RMode.NONE, Sat.Off
B.FPATR     0, 0, 0, 0, 0, 0, 0
B.DIM       M, 0, ->LB0
B.DIM       N, 0, ->LB1
B.DIM       K, 0, ->LB2
B.IOT       A, ScaleA, mask=PE_MASK
B.IOT       B, ScaleB, mask=PE_MASK
B.IOT       Bias, mask=PE_MASK, last, ->D<TSize>
```

建议 API：

```cpp
TMATMUL_MX_BIAS_FIXP(d, a, scale_a, b, scale_b, bias);
```

约束复用已有非 FIXP MX+B profile；Bias 始终为 Local；不读取旧 ACC。

#### A4. `TMATMUL_MX_ACC_FIXP`

基础语义：

```text
D = FIXP(ACC + MXMatMul(A, ScaleA, B, ScaleB))
```

普通 Tile source 顺序：

```text
A, ScaleA, B, ScaleB
```

ACC 是隐式输入，不占上述 source 顺序：

```asm
BSTART.CUBE TMATMULMX.ACC.FIXP, AType
B.DATR      BType, RMode.NONE, Sat.Off
B.FPATR     0, 0, 0, 0, 0, 0, 0
B.DIM       M, 0, ->LB0
B.DIM       N, 0, ->LB1
B.DIM       K, 0, ->LB2
B.IOT       A, ScaleA, mask=PE_MASK
B.IOT       B, ScaleB, mask=PE_MASK
B.IOT       mask=PE_MASK, last, ->D<TSize>
```

建议 API：

```cpp
TMATMUL_MX_ACC_FIXP(d, acc, a, scale_a, b, scale_b);
```

关键约束：`acc` 必须为 `Location::Acc`；pseudo 使用 `ACC_TILE_SRC`；不要把 ACC
作为 `TILE_Src_Reg` 或 inline asm `Tr` operand，否则可能再次触发 register
coalescing/分配错误。

#### A5. LLVM/Clang 修改入口

按现有两个基础 FIXP 的同层位置扩展：

```text
clang/include/clang/Basic/BuiltinsLinxV5.def
clang/lib/Sema/SemaChecking.cpp
clang/lib/CodeGen/CGBuiltin.cpp
clang/test/LinxV5/blk_builtin_call.cpp
llvm/include/llvm/IR/IntrinsicsLinx.td
llvm/lib/Target/LinxV5/LinxV5ISelLowering.h
llvm/lib/Target/LinxV5/LinxV5ISelLowering.cpp
llvm/lib/Target/LinxV5/LinxV5ISelDAGToDAG.cpp
llvm/lib/Target/LinxV5/LinxV5InstrInfo.td
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.cpp
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.h
llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll
```

MC opcode enum/parser/printer/encoding 已经基本存在，一般不需要重新定义 Function
10/12/13/14；实现前应先确认没有重复添加。

建议命名与现有代码保持一致，不要在本工作包内重命名历史 `MAMULB*`：

```text
blk_matmul_bias_fixp
blk_matmul_mx_fixp
blk_matmul_mx_bias_fixp
blk_matmul_mx_acc_fixp
```

具体 intrinsic/ISD/pseudo 名称可按现有命名体系调整，但必须一一对应且容易搜索。

#### A6. 工作包 A 验收标准

至少完成：

1. Clang `-emit-llvm` 能看到四个新 LLVM intrinsic。
2. `llc` 对四个 intrinsic 生成正确 Function 10/12/13/14。
3. 每个 block 恰有一个全零 `B.FPATR`。
4. `TMATMUL_MX_ACC_FIXP` 的反汇编中没有 ACC `B.IOT`。
5. destination 的 `TSize` 为有效 1–7，且只有最终 `B.IOT` 有 `last`。
6. TileOP API 四个 wrapper 能真实实例化、交叉编译和反汇编。

建议测试命令：

```bash
cmake --build build --target llvm-mc llvm-objdump clang llc -j2

build/bin/llvm-lit -v \
  clang/test/LinxV5/blk_builtin_call.cpp \
  llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll
```

TileOP API 实例化继续使用本文件前面记录的
`--target=linx64v5-unknown-linux-musl`、sysroot、`-mlxbc`、
`-fenable-matrix` 和 `-enable-all-vector-as-tilereg=true` 配置。

### 工作包 B：统一 FIXP Descriptor

不要为每个 FIXP opcode增加一长串彼此独立的 builtin 参数。建议先在 TileOP API
定义编译期 descriptor，再在 builtin/LLVM intrinsic 边界压平为固定 ABI。

descriptor 至少应表达：

```text
PreQuantMode
ReluMode
GroupNCode
RowMaxEn
GroupMaxEn
RowMaxInit
MaxAbsEn
RMode
Saturation
Quant 参数来源：None / Tile / GPR
ReLU 参数来源：None / Tile / GPR
可选 RowMaxIn
可选 RowMaxOut
可选 GroupMaxOut
```

推荐分层：

1. TileOP API：模板 descriptor 做大部分 compile-time legality check。
2. Clang Sema：检查必须为常量的 mode 字段和参数数量/类别。
3. LLVM intrinsic：固定顺序携带 attrs、可选 operand presence mask 和 operand。
4. 后端：统一解析 descriptor，生成 `B.FPATR`、`B.DATR`、可选 `B.IOR` 和紧凑
   `B.IOT` source/destination stream。

必须保持的 ABI 顺序：

- 固定 source 先出现：A/B/Bias 或 A/ScaleA/B/ScaleB/Bias。
- 后接按 mode 启用的 RowMaxIn、QuantParamTile、ReluParamTile。
- scalar Quant/LReLU 参数通过 `B.IOR` 的规定 RegSrc 槽传递。
- destination 紧凑顺序固定为：D、RowMaxOut、GroupMaxOut。
- 只有最后一条 `B.IOT` 设置 `last`。

详细组合、dtype 推导和约束以
`/tmp/DavinciOO-intrinsic/isa/intrinsic/header/B.FPATR.md` 为准。不要从普通
TMATMUL 或 TGEMV 的旧行为猜测 FIXP output dtype。

建议先只实现以下 descriptor 子集，再扩展：

1. 全零基础模式。
2. D rounding/saturation。
3. Tile quant 或 GPR quant，二选一。
4. PReLU/LReLU 参数。
5. RowMax。
6. GroupMax/MaxAbs 和多 destination 组合。

### 工作包 C：SharedTile SSA 与 Shared Right

Shared Right 的难点不在 opcode，而在公开类型和生命周期。推荐先建立编译器可见的
SharedTile handle，再接 CUBE Shared form。

SharedTile handle 至少需要携带或可追踪：

```text
Shared ID
version/generation
defined_mask
shape/dtype/layout/role
ready/event dependency
```

设计要求：

- 公开 API 不要求用户手工选择 `S#0..S#255`。
- publish/insert 产生新 Shared version；broadcast/extract/CUBE/TSTORE 消费指定 version。
- verifier 能拒绝未定义 PE fragment、过期 version 和错误 role。
- control-flow merge 后 Shared version 必须满足 SSA/phi 语义，不能仅靠 C++ 类型模板模拟。

接入 TMATMUL/TGEMV Shared Right 时：

- non-MX：仅 B/Right 可为 Shared；生成一个 `C.B.IOS`。
- MX：Right 和 ScaleRight 必须同时为 Shared，生成两个连续 `C.B.IOS`，顺序固定为
  Right、ScaleRight；两个 Shared ID 必须不同。
- A/Left、ScaleA、Bias、ACC 和所有 output 禁止 Shared。
- Shared form 固定 four-PE cooperative，`PE_MASK=1111`。
- 被 `C.B.IOS` 替代的 Shared operand必须从 Local `B.IOT` source stream 移除。
- 禁止 Local/Shared Right 与 ScaleRight 混用。

在 Shared SSA 未完成前，可以先做 MC 层单/双 binder 测试，但不要宣称公开 API 已对齐。

### 工作包 D：Shared TLOAD/TSTORE 与 SPART

参考定义已经区分三种 lowering：

```text
Local TLOAD/TSTORE
Shared full/core TLOAD/TSTORE
Shared TSTORE<pe_scope> -> TSTORE.SPART Function 12
```

建议依赖工作包 C 的 SharedTile handle/version：

- Shared TLOAD：exactly-one issuer，写入一个新的 Shared version；不提供
  `TLOAD<pe_scope>`。
- Shared full TSTORE：exactly-one issuer，使用普通 Shared store Function 1。
- `TSTORE<pe_scope>`：每个 `defined_mask` 中的 PE 通过自己的 pointer 写固定分区，
  使用 Function 12 `TSTORE.SPART`。
- full store 和 partition store 即使 participant 相同也必须使用不同 Function。
- event completion 不等价于跨 PE GM 可见性；需要 `SYNCALL<core_scope>()` 建立
  cross-PE happens-before。

MC 层已有 `TSTORE.SPART` 名称/Function 12 和基础 `C.B.IOS` 支持，可先写 encoding
测试，再接 intrinsic lowering。

### 工作包 E：TGEMV Function 16–22

参考文档已经给出：

```text
16 TGEMV
17 TGEMV.BIAS
18 TGEMV.ACC
20 TGEMVMX
21 TGEMVMX.BIAS
22 TGEMVMX.ACC
```

Function 19 和 23–31 reserved。建议实现步骤：

1. 补全 MC enum/parser/printer/encoding 和独立 MC 测试。
2. 仿照现有 TMATMUL 的 Local builtin/intrinsic/pseudo 路径实现六个基础 TGEMV。
3. 明确 ACC variant 的隐式 ACC dependency，不把 ACC 编为 Tile source。
4. 再复用工作包 C 的 Shared Right/双 binder 支持。
5. 最后接公开 TileOP API 和 shape/role verifier。

重要：参考 `header/B.DATR.md` 明确 TGEMV Function 16–22 暂时保留现有 fused-D
matrix subprofile。不要自动套用 TMATMUL FIXP 的 `B.FPATR` output 规则，也不要凭
名称创造 TGEMV FIXP opcode。

### 工作包 F：Event、Scope 与 Convergence

该工作包应建立统一 ABI，而不是为 GMOV/TLOAD/TSTORE 各自定义不兼容事件类型。

建议：

- `RecordEvent` 表示异步操作完成 token。
- `WaitEvents...` 在 Clang/LLVM 层压平为 chain/token dependency，确保调度不可越过。
- `pe_scope`、`core_scope` 使用编译期 tag；非法 scope 在 Sema/模板层拒绝。
- event completion 只表达对应操作 ready/completion，不自动表达 GM visibility 或
  Core barrier。
- Shared collective、GMOV Core4 和 scope 操作需要 MachineVerifier 或专用 pass 检查
  静态收敛性：所有必须参与的 PE 应在一致控制流中执行兼容操作。

推荐先支持单 event wait，再扩展可变 `WaitEvents...`，避免一开始引入复杂 tuple ABI。

### 推荐 Agent 拆分

如果使用多个 agent，建议按不重叠写范围拆分：

1. Agent A：四个基础 Local FIXP 的 Clang/LLVM/backend 和 lit 测试。
2. Agent B：TileOP API 四个 wrapper、约束、文档和真实实例化测试。
3. Agent C：FIXP descriptor 设计文档/原型，不与 Agent A 同时修改基础 lowering。
4. Agent D：Shared SSA/version 设计和低层 intrinsic ABI。
5. Agent E：TGEMV MC encoding/parser/printer 测试。

Agent A 与 Agent B 可以并行，但需要先约定 builtin 参数顺序。Shared、event 和完整
descriptor 会改变 ABI，建议在基础四 FIXP 合入并稳定后再开始大规模实现。

### 明确禁止的捷径

- 不要用 TileOP API inline asm 绕过 builtin/intrinsic 来宣称完整支持。
- 不要把 ACC 当普通 Tile 输入或输出。
- 不要让 Shared MX 只共享 Right 而 ScaleRight 保持 Local。
- 不要让 Shared collective 接受 partial `PE_MASK`。
- 不要让用户长期手工管理 Shared ID/version。
- 不要把 `RecordEvent` completion 当作 `core_scope` barrier 或 GM visibility fence。
- 不要修改或删除用户已有未跟踪文件和 patch 文件。

## SuperNPUBench multi_thread/vec 验证

核对和验证日期：2026-07-30。

使用的上游仓库与版本：

```text
https://github.com/PTO-ISA/SuperNPUBench.git
branch: main
commit: ebe6c9e2cb7dd300ee1274e76dc6c2c4d5c1d3e4
commit date: 2026-07-29
case: benchmark/one-level-arch/test/kernel/multi_thread/vec
```

验证使用当前 `/home/zhuwei/linx-llvm/build/bin/clang` 后端、安装工具链 sysroot/
linker/runtime，以及同步后的 TileOP API resource-dir。执行等价于：

```bash
make TESTCASE=vec \
  COMPILER_DIR=/tmp/linx-current-toolchain/bin \
  TileRows=16 TileCols=16

make TESTCASE=vec \
  COMPILER_DIR=/tmp/linx-current-toolchain/bin \
  TileRows=16 TileCols=16 diss
```

结果：编译、链接和反汇编成功。关键指令序列包括：

```asm
SSRGET       0x0802
BSTART.TLSU  TLOAD, FP32
B.IOT        mask=1111, TSize=2, last, ->T
BSTART.TLSU  TLOAD, FP32
B.IOT        mask=1111, TSize=2, last, ->U
BSTART.TEPL  TADD, FP32
B.IOT        T, U, mask=1111, TSize=2, last, ->T
BSTART.TLSU  TSTORE, FP32
B.IOT        T, mask=1111, last
```

验证过程中修复了 TileOP API 的 TSize 计算：`TileDType` 的 `sizeof` 表示单 PE
fragment，而 v5 `B.IOT.TSize` 表示 four-PE complete logical Tile。现在：

```text
Regsize      = sizeof(TileDType)       // PE-local bytes
TilesizeCode = encode(sizeof(TileDType) * 4) // complete logical Tile bytes
```

因此该测例每 PE 的 `4x16 FP32` fragment 为 256 B，完整 logical Tile 为 1 KB，
正确编码为 `TSize=2`。此前错误地按 256 B 查 v5 TSize 表，触发了“至少 512 B”的
静态断言。

相关 LLVM 回归测试结果：6/6 PASS：

```text
clang/test/LinxV5/blk_builtin_call.cpp
clang/test/LinxV5/peid.cpp
llvm/test/CodeGen/LinxV5/peid.ll
llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll
llvm/test/CodeGen/LinxV5/v5-shared-gmov.ll
llvm/test/MC/LinxV5/v5-shared-cube-encoding.s
```

TileOP API `TAdd` 测试也成功编译和链接；基础 FIXP API 临时实例成功编译并确认
`TMATMUL.FIXP` / `TMATMUL.ACC.FIXP` 均生成 `B.FPATR 0,0,0,0,0,0,0`，ACC 不进入
`B.IOT`。

额外发现：TileOP API 仓现有 `test/tileop_api/src/MatMul.cpp` 会在旧非 FIXP
`MATMUL` 路径触发 non-allocatable ACC output 的 SelectionDAG assertion。该问题与
本次 `multi_thread/vec`、TSize 修复和 FIXP output 路径不同，未在本次提交中扩展修复。
