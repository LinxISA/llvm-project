# Element-if Compiler Handoff

> 独立 agent 接手文档。最新状态以本文末尾为准。

## 1. 目标

为 LinxV5 LLVM 实现 scalar-style elementwise 前端：用户写普通 C++ 风格的
tile 元素访问、`for` 和 `if/else`，不直接调用 `blk_tload`、`TADD`、
`TSEL`、`TSTORE` 等 TileOP API；Clang/LLVM 将其转换为 LinxV5 TileOP。

目标代码形态：

```cpp
using tile = float tile_size(128);

void elementwise_if_for(tile &out, const tile &lhs, const tile &rhs) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) {
    if (lhs[i] > 0.0f)
      out[i] = lhs[i] + rhs[i];
    else
      out[i] = lhs[i] - rhs[i];
  }
}
```

端到端链路：

```text
scalar C++ -> Clang AST pattern recognition
  -> LLVM elementwise intrinsics -> LinxV5 SelectionDAG
  -> TileOp pseudo / MC expansion -> LinxV5 ET_REL object
```

## 2. 设计依据

相关工作：

- TileOP API #228：scalar-style / 仿 SIMT elementwise 前端总体方向。
- TileOP API #235：Local CUBE TileOp 的 GPR / PredicateCell ExecutionMask API。
- TileOP API #238：element-if 分层设计建议。
- PTO-SPEC PR #352：ExecutionMask 和 Local CUBE mask contract。
- LLVM #114：element-if 端到端 demo、产物和模型联调 issue。

规范判断必须以 PTO-SPEC normative ASL 为准，不以旧 issue 文本或模型临时
行为为准。当前规范：

```text
commit 53539ce34e103fc69b768a2ca67e34664f2e4c05
path /tmp/pto-spec-current
```

关键文件：

```text
asl/block/model/dispatch/tlsu-layout-conversion.asl
```

只要 `B.IOR` binding 存在：

```text
source0 = base address
source1 = row_stride_bytes
```

只有整个 `B.IOR` 不存在时才使用 dense stride 默认值。因此
`[base, zero, zero]` 表示真实 0 字节行跨度，不表示省略 stride。

## 3. 当前实现架构

### 3.1 Clang 前端

主要入口：

```text
clang/lib/Parse/ParsePragma.cpp
clang/lib/Sema/SemaStmtAttr.cpp
clang/lib/CodeGen/CGStmt.cpp
clang/include/clang/Basic/Attr.td
```

当前支持：

- `#pragma linx elementwise`。
- LLVM function 属性 `linx.elementwise` 和 `linx.elementwise.lanes`。
- pragma 直接修饰 elementwise `for`。
- `EmitLinxElementwiseForStmt` 对 AST 做受限 pattern matching。
- 当前完整 demo pattern 是 128-lane FP32 loop：
  - `for (unsigned i = 0; i < 128; ++i)`；
  - 条件 `lhs[i] > 0.0f`；
  - then `out[i] = lhs[i] + rhs[i]`；
  - else `out[i] = lhs[i] - rhs[i]`；
  - 输入、输出、compare base 通过一致性检查。
- 不支持的 pattern 应 fail closed 并给诊断，不能静默退回普通 scalar loop。

关键函数：

```text
clang/lib/CodeGen/CGStmt.cpp: EmitLinxElementwiseForStmt
clang/lib/CodeGen/CGStmt.cpp: ignoreElementwiseCasts
clang/lib/CodeGen/CGStmt.cpp: getElementwiseDeclRef
clang/lib/CodeGen/CGStmt.cpp: isElementwiseIndex
clang/lib/CodeGen/CGStmt.cpp: getElementwiseSubscript
```

### 3.2 LLVM IR

Clang 生成的 experimental intrinsic：

```text
llvm.linx.experimental.ew.texpands
llvm.linx.experimental.ew.tcmp
llvm.linx.experimental.ew.tadd.masked
llvm.linx.experimental.ew.tsub.masked
llvm.linx.experimental.ew.tsel
```

当前顺序：

```text
TLOAD lhs -> TLOAD rhs -> TEXPANDS 0.0f -> TCMP lhs, zero, GT
  -> TADD lhs, rhs, predicate, PredInv=0, Zero=1
  -> TSUB lhs, rhs, predicate, PredInv=1, Zero=1
  -> TSEL predicate, then, else -> TSTORE result
```

源码生成位置：

```text
clang/lib/CodeGen/CGStmt.cpp:1239-1282 附近
```

相关声明/检查：

```text
llvm/include/llvm/IR/IntrinsicsLinxV5.td
clang/lib/Sema/SemaChecking.cpp
clang/lib/CodeGen/CGBuiltin.cpp
```

### 3.3 LinxV5 后端

主要文件：

```text
llvm/lib/Target/LinxV5/LinxV5ISelLowering.cpp
llvm/lib/Target/LinxV5/LinxV5ISelLowering.h
llvm/lib/Target/LinxV5/LinxV5ISelDAGToDAG.cpp
llvm/lib/Target/LinxV5/LinxV5ElementwiseMask.cpp
llvm/lib/Target/LinxV5/LinxV5InstrInfo.td
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.cpp
```

elementwise lowering：

```text
lowerElementwiseTAddMasked
lowerElementwiseTSubMasked
lowerElementwiseTCmp
lowerElementwiseTSel
lowerElementwiseTExpands
```

对应 DAG selection：

```text
selectElementwiseTAddMasked
selectElementwiseTSubMasked
selectElementwiseTCmp
selectElementwiseTSel
selectElementwiseTExpands
```

mask pass：

```text
llvm/lib/Target/LinxV5/LinxV5ElementwiseMask.cpp
```

该 pass 使用 `linx.elementwise` 属性参与 mask / divergent control-flow 处理；
它不是完整 SIMT 前端，也不等于已经支持任意 C++ CFG、reconvergence 或
active-mask SSA。

## 4. CUBE memory 与 bundle 约束

当前 16x8 FP32 elementwise CUBE geometry：

```text
logical shape: 16 rows x 8 columns
LB0 = 8
LB1 = 16
LB2 omitted
TLOAD layout = ND2M16
TSTORE layout = M162ND
```

`lowerLOAD` / `lowerSTORE` 位于：

```text
llvm/lib/Target/LinxV5/LinxV5ISelLowering.cpp
```

elementwise path 的 stride：

```text
row_stride = valid_columns * element_size_bytes
```

本 demo 为 `8 * 4 = 32` 字节，入口生成：

```text
addi zero, 32, ->a3
```

普通非 elementwise vector LOAD/STORE 仍使用原有 `R0` 行为。

TileOp bundle 中 DIM 必须先于 IOT/IOR/IOS：

```text
BSTART.TEPL TCMP -> B.DATR -> C.B.DIMI 8 -> C.B.DIMI 16 -> B.IOT/B.IOR
```

DIM 顺序正确不等于 bundle 语义全部正确；必须同时检查 descriptor、layout、
mask carrier、source role 和 storage kind。

## 5. ExecutionMask / PredicateCell 状态

### 已有

- `PredInv` 和 `Zero` 的编译期 0/1 检查。
- `B.DATR` execution-mask 控制字段编码。
- `ExecMaskPresent` 的 `B.IOR` 变体编码。
- CUBE `M=16` / `M=32` 基本合法性检查。
- masked TADD / TSUB 的 elementwise predicate 依赖。
- PredicateCell-specific tile register class 的 element-if 路径。
- TCMP 结果作为 PredicateCell 逻辑值供 TSEL 和 masked arithmetic 使用。

### #114 历史问题已修复

1. masked `B.DATR` bit12 编码错误；
2. TSEL 物理 operand 顺序错误；
3. FP32 TLOAD/TSTORE descriptor 错误；
4. TCMP DATR layout 错误；
5. CUBE `LB0/LB1/LB2` 维度错误；
6. TEXPANDS 零 tile 缺少 `CUBE_M16` layout；
7. CUBE memory `B.IOR source1` 为零，未显式表达 row stride。

### 尚未完成

不要将当前 prototype 宣称为完整 #235 / PTO #352：

- GPR carrier 尚未完整按 dtype、layout、valid shape 选择 one/two word。
- PredicateCell carrier 尚未成为所有 Local CUBE operation 的统一完整接口。
- operation-owned GPR/Tile operand 与 ExecutionMask carrier 的完整分离仍需系统化。
- 连续 B.IOR/B.IOT record、最终记录标志和 source role schema 仍需审计。
- mask snapshot / alias analysis 尚未完整实现。
- `Zero=0` 的 merge-base tile 与 `Zero=1` 的真实 zero destination 语义仍需核对。
- 当前 operation closure 主要覆盖 TLOAD/TSTORE、TCMP、TADD、TSUB、TSEL、TEXPANDS。
- 尚未支持任意 C++ `if`、嵌套 CFG、任意 loop、break/continue、复杂表达式和通用短路语义。
- 双路径 if-conversion 对有 fault/status/副作用的 operation 不能默认合法，必须逐类证明或禁止。

## 6. 最新端到端 demo

文件：

```text
element-if-demo/elementwise_if_for.cpp
element-if-demo/elementwise_if_for.ll
element-if-demo/elementwise_if_for.o
element-if-demo/elementwise_if_for.dis
```

当前 commit：

```text
compiler/artifact: 70856b27ee79
handoff docs:      7d5319f40541
branch:            dev-llvm15_56
remote:            linxisa -> LinxISA/llvm-project
```

关键反汇编：

```text
addi zero, 32, ->a3
TLOAD  ..., [base=a1, stride=a3]
TLOAD  ..., [base=a2, stride=a3]
TEXPANDS ..., CUBE_M16
TCMP ... C.B.DIMI 8 ... C.B.DIMI 16 ...
TADD ... C.B.DIMI 8 ... C.B.DIMI 16 ...
TSUB ... C.B.DIMI 8 ... C.B.DIMI 16 ...
TSEL predicate, then, else
TSTORE ..., [base=a0, stride=a3]
```

关注 descriptor、logical role 和 shape，不要硬编码 allocator 产生的物理
`T#1` / `T#2` 编号。

最新哈希：

```text
object size: 1248 bytes
elementwise_if_for.ll  34839638f99fc0833c90c916e01b661393103095abaf6b5bc4d8d37bb7975eac
elementwise_if_for.o   a1f0fed91082b262d01c0f87b8dbc90123a0bb7290d222a81975be86917e8a87
elementwise_if_for.dis 9ce2ed298e796ad29ac4541a20fcbce93c26fe21c81ca47a6d6313780a87b0eb
```

## 7. 构建和测试

环境：

```text
repo:    /home/zhuwei/linx-llvm
clang:   /home/zhuwei/linx-llvm/build/bin/clang++
sysroot: /home/zhuwei/linx-toolchain-build-online-main/output/linx_blockisa_llvm_musl/sysroot
include: /home/zhuwei/linx-llvm/build/lib/clang/15.0.4/include/tileop-api
```

构建：

```bash
ninja -C build clang llc llvm-objdump
```

重新生成：

```bash
SYSROOT=/home/zhuwei/linx-toolchain-build-online-main/output/linx_blockisa_llvm_musl/sysroot
build/bin/clang++ --target=linx64v5-unknown-linux-musl -mlxbc \
  -fenable-matrix -std=c++20 -D__linx -DENABLE_TENSOR_INSTR -O2 \
  --sysroot="$SYSROOT" -Ibuild/lib/clang/15.0.4/include/tileop-api \
  -S -emit-llvm element-if-demo/elementwise_if_for.cpp \
  -o element-if-demo/elementwise_if_for.ll
build/bin/llc -mtriple=linx64v5 -mcpu=janus \
  -enable-all-vector-as-tilereg=true \
  -linxv5-enable-clock-hand-opt=false -filetype=obj \
  element-if-demo/elementwise_if_for.ll \
  -o element-if-demo/elementwise_if_for.o
build/bin/llvm-objdump -d element-if-demo/elementwise_if_for.o \
  > element-if-demo/elementwise_if_for.dis
sha256sum element-if-demo/elementwise_if_for.{ll,o,dis}
```

focused tests：

```bash
build/bin/llvm-lit \
  clang/test/LinxV5/elementwise-scalar-if.cpp \
  llvm/test/CodeGen/LinxV5/elementwise-tcmp.ll \
  llvm/test/CodeGen/LinxV5/elementwise-tsel.ll \
  llvm/test/CodeGen/LinxV5/elementwise-mask-tadd.ll \
  llvm/test/CodeGen/LinxV5/elementwise-mask-tsub.ll
```

截至本文生成时：5/5 通过。

相关测试：

```text
clang/test/LinxV5/elementwise-scalar-if.cpp
clang/test/LinxV5/elementwise-scalar-if-diagnostic.cpp
clang/test/LinxV5/elementwise-scalar-tadd.cpp
clang/test/LinxV5/elementwise-mask-tadd.cpp
clang/test/LinxV5/elementwise-mask-tadd-sema.cpp
clang/test/LinxV5/elementwise-mask-tsub.cpp
clang/test/LinxV5/elementwise-mask-tsub-sema.cpp
llvm/test/CodeGen/LinxV5/elementwise-tcmp.ll
llvm/test/CodeGen/LinxV5/elementwise-tsel.ll
llvm/test/CodeGen/LinxV5/elementwise-mask-tadd.ll
llvm/test/CodeGen/LinxV5/elementwise-mask-tsub.ll
llvm/test/CodeGen/LinxV5/elementwise-mask-cube-matmul.ll
llvm/test/CodeGen/LinxV5/elementwise-mask-cube-matmul-invalid.ll
llvm/test/CodeGen/LinxV5/elementwise-mask-contract.ll
```

## 8. LLVM #114 模型联调背景

最新回复：

```text
https://github.com/LinxISA/llvm-project/issues/114#issuecomment-5905347241
```

模型此前对 transport-patched 版本验证过：只把三个 stride 设置成 `a3=32`，
其余 machine word 不变，得到：

```text
128/128 output golden
element-if directed: 2797/2797
full diff_test: 1,146,962/1,146,962
gfrun ELF pass-list: 693/693
Python cross-model harness: 61 tests passed
```

这些是模型侧 patched fixture 的结果，不能说旧 compiler object 已通过。现在
`70856b27ee79` 已让编译器原生生成显式 stride，需要模型对新 object 重跑。

重要边界：

- object 仍是 ELF `ET_REL` relocatable object；
- 没有完成通用 ET_REL linker；
- 手工映射/重定位 harness 不等于 linked ELF 验收；
- 尚未实现 `.note.pto.isa` 或正式 PTO 0.59 object identity 验收；
- 不要放宽 model/gfrun legality 检查来让错误 machine word 通过。

## 9. 仓库状态

LLVM：

```text
/home/zhuwei/linx-llvm
branch dev-llvm15_56
remote linxisa
```

TileOP：

```text
/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API
branch fix/issue-111-timg2col-spart-inline
remote origin
```

TileOP 本地存在用户未提交修改，并与 `origin/linx` 分叉；不要 reset、clean
或覆盖。处理 TileOP 前先检查 status，并按 skill 要求先解决分叉同步策略。

PTO-SPEC：

```text
/tmp/pto-spec-current
branch main
commit 53539ce34e10
```

## 10. 推荐接手顺序

### P0：验证最新原始 object

1. 使用 #114 commit `70856b27ee79` 的 `.o`。
2. 不手工修改 transport word。
3. 只做 ET_REL bring-up 必需的 section 映射和合法常量重定位。
4. 确认三个 `B.IOR` 解码为 `source1=a3`，且入口 `a3=32`。
5. 跑 scalar golden、mixed/all-then/all-else、正负零、alias 和 multi-PE。

### P1：扩展前端

1. 更多比较方向和常量 RHS；
2. 已证明无副作用的乘法、最大最小等操作；
3. 更一般的 then/else expression tree；
4. 明确 shape/storage contract 后支持非 128 lane；
5. 所有 unsupported pattern 继续 fail closed。

### P1：统一 ExecutionMask carrier

1. 强类型 one-word/two-word GPR/PredicateCell carrier；
2. 分离 operation-owned GPR/Tile operand 与 mask carrier；
3. 按 dtype/layout/valid shape 计算 capacity；
4. 统一 B.IOR/B.IOT record 顺序、source role 和最终标志；
5. 接入 PredicateCell snapshot / alias analysis。

### P2：完整 mask 语义和 ELF

1. 实现并验证 `PredInv`、`Zero=1` zero destination、`Zero=0` merge base；
2. 对 fault/status/side-effect operation 禁止或证明 speculative if-conversion；
3. 明确 linker/relocation contract；
4. 生成 linked executable ELF；
5. 如合同要求，加入 `.note.pto.isa` 和正式 ISA identity。

## 11. 新 agent 开始命令

```bash
cd /home/zhuwei/linx-llvm
git status --short --branch
git log -5 --oneline --decorate
git fetch linxisa dev-llvm15_56
git diff linxisa/dev-llvm15_56 --stat

git -C /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API status --short --branch
git -C /tmp/pto-spec-current status --short --branch

rg -n "EmitLinxElementwiseForStmt|lowerElementwise|linx.elementwise|B.IOR|RowStride" \
  clang/lib/CodeGen/CGStmt.cpp llvm/lib/Target/LinxV5 \
  clang/test/LinxV5 llvm/test/CodeGen/LinxV5
```

处理 issue 时必须继续使用：

```text
/home/zhuwei/.agents/skills/pto-issue-triage/SKILL.md
```

流程是同步仓库和 ASL、读取全部评论、以 ASL 独立核实、修改/测试/推送、
回复 issue、更新 handoff。

## 12. 当前状态与下一步（2026-09-30）

- scalar-style `#pragma linx elementwise` 已有首条完整 FP32 element-if 路径。
- `for/if/else` demo 可生成 LLVM IR、LinxV5 ET_REL object 和反汇编。
- 当前 #114 发现的 descriptor、layout、DIM 顺序、TSEL 角色、masked arithmetic
  和显式 row stride 编译器问题均已修复。
- 最新 compiler commit：`70856b27ee79`。
- 最新 handoff commit：`7d5319f40541`。
- issue 回复：`#issuecomment-5905347241`。
- focused tests：5/5 通过。
- 下一关键动作：模型侧对未修改的新 object 做原始 decode/binder/execute。
- 不能宣称：完整 #235 ExecutionMask、任意 C++ element-if、通用 linker、linked
  ELF 验收或正式 PTO 0.59 支持已完成。
