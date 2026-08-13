# Codex 工作交接记录

> 最新状态日期：2026-08-13
> LLVM 仓库：`/home/zhuwei/linx-llvm`
> TileOP API 仓库：`/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API`
> PTO-SPEC 审计快照：`/tmp/pto-spec-audit-20260812`
> **重启后必须先阅读紧接本段的“2026-08-13 状态快照”。后续较早章节是历史记录；发生冲突时，以最新快照和当前 PTO-SPEC normative ASL 为准。**

## 2026-08-13 状态快照：inline-asm Matrix 全量交付 + TileOP 已推送

### 0. 重启后的最短启动流程

新 Codex 会话不要先按历史章节修改代码。依次执行：

```bash
cd /home/zhuwei/linx-llvm
sed -n '1,120p' CODEX_HANDOFF.md
git status --short --branch
git -C /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API \
  status --short --branch
```

### 1. 当前两仓精确基线（2026-08-13）

```text
LLVM:     /home/zhuwei/linx-llvm
  branch: dev-llvm15_56
  HEAD:   e4bbf35d87c0  [LinxV5] Align TLSU Function table with PTO v0.58 reissue
  远端:   linxisa/dev-llvm15_56 同步（无待推送）

TileOP:   /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API
  branch: linx
  HEAD:   713bbd03fe76  [tileop-api] docs: document full 12-op Matrix surface
  远端:   origin/linx 同步（713bbd0）

HANDOFF: 本文件保持 LOCAL ONLY，永不 commit/push。
```

### 2. 已完成的 Matrix 交付（P0 + P1 全绿）

按“inline-asm 最小交付范围”：
- **P0 全部完成**：12 个 op（6 TMATMUL + 6 TGEMV）× basic+options 双形式，
  全部 PostProcess（scalar/vector quant、LReLU/PReLU、RowMax、GroupMax、
  MaxAbs）+ `is_valid_fixp_attr` 合法性 + B.DATR Zero/B.FPATR 顺序 +
  数学源顺序 + alias（早期-clobber `=&Tr` read-old/write-new）+ dtype/shape
  检查 + 编码/反汇编验证 + 无 `.FIXP` mnemonic。
- **P1 全部完成**：compile.all 平台分组（31 linx-OK + 15 legacy 注释）、
  docs 迁移（tmatmul-fixp.md → matrix-postprocess.md，18 处
  TMATMUL_FIXP → TMATMUL）、属性组合测试 PostProcessCombos、Shared/Local
  形态测试 SharedMatrixForms（顺带修复 MX Shared scale：新增
  `is_any_tile_data_v` concept，options 重载接受 Shared scale）、负向测试
  PostProcessNegatives + run_negatives.sh（5 用例必编译失败）。

### 3. 已知遗留

- **make check 契约测试 3 个既有失败**（Shared/TMOV/TLSU 断言与 zhoubot
  Shared 改造漂移，非 Matrix 引入）：
  `test_shared_tile_bindings_use_b_ios`、`test_shared_tmov_...`、
  `test_tlsu_stride_is_expressed_in_logical_elements`。
- **LLVM 侧 FPATR 端到端真实值**（P2 暂缓）：`LinxV5MCCodeEmitter.cpp`
  仍硬编码 7 个 zero immediate，只影响未来 native-intrinsic 路径，不影响
  inline-asm（值已在 asm 文本中）。
- 旧 cpu_sim 时代 13 个 wrapper（TCOPY/TEXPAND/TPAD/TReshape/TROW*EXPAND+
  TileAcc）在 linx 平台不适用，compile.all 已注释保留。

### 4. 下一步

按本节后续 “2026-08-13: Tile datatype reinterpret 前端接口设计” 实现
`reinterpret_tile<NewDType>`（Local→Local、同位宽、零指令 view）。

---

## 2026-08-12 权威重启快照：PTO-SPEC / LLVM / TileOP 审计与下一步

### 0. 重启后的最短启动流程

新 Codex 会话不要先按历史章节修改代码。依次执行：

```bash
cd /home/zhuwei/linx-llvm
sed -n '1,470p' CODEX_HANDOFF.md
sed -n '1,560p' PTO_SPEC_LATEST_LLVM_TILEOP_FULL_AUDIT_20260812.md
git status --short --branch
git diff -- \
  llvm/lib/Target/LinxV5/AsmParser/LinxV5AsmParser.cpp \
  llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5BaseInfo.h \
  llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5InstPrinter.cpp \
  llvm/test/MC/LinxV5/v5-shared-cube-encoding.s

git -C /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API \
  status --short --branch
```

然后向用户确认要继续哪一个工作包。当前最自然的下一步是先完成第 9 节的 P0 修复；除非用户明确要求，不要 commit、push、rebase 或切换分支。

### 1. 当前三仓精确基线

#### LLVM

```text
path:   /home/zhuwei/linx-llvm
branch: temp/shared-tload-integration-20260811
HEAD:   eb64de8afcbda043aec7e56dae346905dc982039
commit: [LinxV5] Finalize Shared TLOAD binder encoding
date:   2026-08-11T21:32:41+08:00
```

该基线已经同时推送到两个远端的同名临时分支：

```text
github  git@github.com:PTO-ISA/linx-llvm.git
        refs/heads/temp/shared-tload-integration-20260811
        eb64de8afcbda043aec7e56dae346905dc982039

linxisa git@github.com:LinxISA/llvm-project.git
        refs/heads/temp/shared-tload-integration-20260811
        eb64de8afcbda043aec7e56dae346905dc982039
```

注意：当前 TLSU function 8–14 修复在 `eb64de8` 之上的本地工作树中，**尚未提交、尚未推送**。

#### TileOP API

```text
path:   /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API
branch: temp/shared-tload-integration-20260811
HEAD:   a1f085ea1d3ffd87560f213bedd9038e57b54917
commit: [tileop-api] Add output-form Shared TLOAD integration
date:   2026-08-11T21:32:41+08:00
remote: git@github.com:LinxISA/Linx-TileOP-API.git
```

#### PTO-SPEC

2026-08-12 已执行 `git fetch origin main` 并确认本地 HEAD 等于远端 `origin/main`：

```text
path:    /tmp/pto-spec-audit-20260812
branch:  main
commit:  4d115387b8a8a3c135f78189778d38547e75c697
date:    2026-08-12T11:02:22+08:00
subject: Close B.FPATR, DTYPE_NONE, and TMATMUL dimension gaps (#64 #68 #70)
version: PTO ISA 0.58.0
```

规范证据优先级：

```text
asl/ normative
spec/catalog/*.json generated projection
spec/evidence/*.json totality/negative evidence
Markdown generated/supplementary documentation
```

### 2. 必须保护的未提交工作

不要使用 `git reset --hard`、`git clean`、覆盖式 checkout、危险 rebase 或批量删除。两个仓库都有用户/agent 未提交内容。

LLVM 当前与最新 TLSU 审计修复直接相关的 tracked 修改：

```text
M llvm/lib/Target/LinxV5/AsmParser/LinxV5AsmParser.cpp
M llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5BaseInfo.h
M llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5InstPrinter.cpp
M llvm/test/MC/LinxV5/v5-shared-cube-encoding.s
```

LLVM 还有 handoff 和用户文件，包括但不限于：

```text
M  CODEX_HANDOFF.md
?? PTO_SPEC_LATEST_LLVM_TILEOP_FULL_AUDIT_20260812.md
?? PTO_TILEOP_SUPPORT_AUDIT_20260812.md
?? PTO_B_FPATR_POSTPROCESS_HANDOFF.md
?? PTO_B_FPATR_COMPILER_REQUIRED_INFORMATION_RESPONSE.md
?? TLOAD_SHARED_PARAM_ISSUE.md
?? llvm/test/CodeGen/LinxV5/v5-matmul-tilesize-encode.ll
?? llvm/test/MC/LinxV5/relax.s.o
?? 多个用户 patch、诊断文档和 tmp 目录
```

TileOP 当前用户未提交内容：

```text
M  docs/tileop-usage/tmatmul-fixp.md
?? test/tileop_api/src/MultiThreadAdd.cpp
```

后续若必须切分支/rebase，先做以下一种安全备份，并向用户说明：

```bash
git diff > /tmp/linx-llvm-before-switch-20260812.patch
git -C /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API diff \
  > /tmp/tileop-before-switch-20260812.patch
```

未跟踪文件需要另外复制或先用明确 path 的 `git add`/临时 commit；普通 `git diff` 不包含 untracked 文件。

### 3. 已完成并已推送的 Shared TLOAD / Shared TMOV 工作

当前两个临时远端分支已经包含此前联调工作：

1. LLVM 支持 GM → Shared register 的直接 `TLOAD` binder；
2. TileOP 支持 output-form `TLOAD(SharedTile<...>&, const GM&)`；
3. Shared register 使用绝对 `S0..S255`，没有引入相对 Shared index；
4. Shared value 在函数内/always-inline SSA 流中使用，`Sr` constraint 交给后端分配绝对 Shared register；
5. `TMOV_L2S_INSERT/PUBLISH` 同时支持返回式和 output parameter 形式；
6. `TMOV_L2S_PUBLISH(shared, local)` 可以直接表达 Local → Shared，不需要额外 copy/spill；
7. `TMOV_L2S_INSERT/PUBLISH` 的 Shared destination size 由 `B.IOS ->S<n><size>` 承载；
8. 无 Local destination 的 source binder 使用普通 `B.IOT src, mask=..., last`，不再携带 `TSize`；
9. 已删除歧义的 `B_IOT_OneSrc_NoDst_Size`，避免无 dst 指令被 objdump 误显示为 Local destination；
10. `B.IOS` 是 32-bit Shared descriptor；不要恢复旧 `C.B.IOS` 或 mask-only Shared `B.IOT` companion。

已知 ABI 边界：

- 非内联函数通过 `SharedTile&` 跨函数传递仍会形成 Shared handle 的普通内存/GPR copy；
- 当前没有 Shared register 的普通 store/spill/cross-function ABI；
- 这不是 Shared register allocator 阶段能补救的问题；
- 不要把 Shared payload 改成 Local Tile payload，也不要复用 `PseudoTSTORE`；
- 如果用户要求非内联 Shared 参数/返回，必须单独设计 ABI。

对应详细说明：

```text
TLOAD_SHARED_PARAM_ISSUE.md
CODEX_HANDOFF.md 中 2026-08-11 Shared TLOAD / Shared L2S 章节
```

### 4. 当前本地未提交的 TLSU encoding 修复

PTO-SPEC 最新 TLSU function 表是：

```text
TLOAD               0
TSTORE              1
TMOV                 2
TPREFETCH            3
MGATHER              4
MSCATTER             5
MGATHER.MASK         6
MSCATTER.MASK        7
MGATHER.CAS          8
TMOV.L2S.INSERT      9
TMOV.L2S.PUBLISH    10
TMOV.S2L.BROADCAST  11
TMOV.S2L.EXTRACT    12
GMOV                13
TSTORE.SPART        14
```

重要覆盖说明：历史 handoff 中“Function 9–14 全部 reserved/illegal”以及“TMOV.L2S.INSERT=8”的说法已经过期。最新规范提交 `4d115387` 和本节 function 表优先。

当前本地修改已经：

- 在 `LinxV5BaseInfo.h` 增加 `TPREFETCH=3`、`MGATHER_CAS=8`；
- 把 Shared TMOV 从旧 `8..11` 移到规范 `9..12`；
- 保持 `GMOV=13`；
- 把 `TSTORE.SPART` 从旧 `12` 移到 `14`；
- 在 parser/printer 增加 `TPREFETCH` 和 `MGATHER.CAS` 名称；
- 在 MC test 中加入精确 bytes 和 round-trip 检查。

精确编码：

```text
TPREFETCH          [0x81,0x11,0x31,0x08]
MGATHER.CAS        [0x81,0x11,0x81,0x08]
TMOV.L2S.INSERT    [0x81,0x11,0x91,0x08]
TMOV.L2S.PUBLISH   [0x81,0x11,0xa1,0x08]
TMOV.S2L.BROADCAST [0x81,0x11,0xb1,0x08]
TMOV.S2L.EXTRACT   [0x81,0x11,0xc1,0x08]
GMOV               [0x81,0x11,0xd1,0x08]
TSTORE.SPART       [0x81,0x11,0xe1,0x08]
```

已验证：

- `llvm-mc`、`llvm-objdump`、`llc` 构建成功；
- 定向 MC + Shared CodeGen 测试 `3/3 PASS`；
- 实际 CodeGen object 已确认 Shared TMOV 使用 function `9..12`；
- 全 LinxV5 MC 仍有 7 个旧语法既有失败，与这次 TLSU 修改无关，不要顺手修复。

### 5. 最新全量审计结论

完整报告：

```text
/home/zhuwei/linx-llvm/PTO_SPEC_LATEST_LLVM_TILEOP_FULL_AUDIT_20260812.md
```

PTO-SPEC 规模：

```text
109 accepted Tile operations
100 command forms
474 scalar forms
```

Tile operation 核心结论：

- TLSU `10/10` catalog operations 的 named assembly/function 数值一致；
- CUBE `12/12` named assembly/function 数值一致；
- TEPL `86/87` canonical name 可 assemble；
- 所有 accepted TEPL operation 的 selector 数值一致；
- 唯一 canonical name 缺失是 `TSORT`；LLVM/TileOP 仍使用旧名 `TSORT32`；
- 因此，在第 4 节本地未提交修复基础上，109 个 accepted Tile Operation **没有已知 selector/function 数值误差**；
- 这不等于 command/scalar、canonical syntax、legality 或完整 CodeGen 全部一致。

自动审计工件：

```text
/tmp/pto_tile_operation_matrix.tsv
/tmp/pto_command_decoder_matrix.tsv
/tmp/pto_scalar_decoder_matrix.tsv
/tmp/audit_pto_tile_ops.py
/tmp/audit_pto_forms.py
/tmp/refine_pto_audit.py
/tmp/refine_scalar_witness.py
```

这些 `/tmp` 文件重启机器后可能不存在；关键结论已经固化在完整报告和本 handoff 中。

### 6. TileOP API 缺失与实现边界

当前 public header 至少缺少以下规范 operation wrapper：

```text
TSORT
TMOV
TPREFETCH
MGATHER_CAS
TGEMV
TGEMV_BIAS
TGEMV_ACC
TGEMV_MX
TGEMV_MX_BIAS
TGEMV_MX_ACC
```

可以直接或低风险实施：

1. 普通 Local → Local `TMOV(dst, src)`：按普通 Tile 一进一出和现有 TLSU bundle 逻辑实现；
2. LLVM `TSORT` canonical parser name：映射现有 selector `0x06c`，暂时保留 `TSORT32` alias；
3. 规范 `BSTART.<operation>` input aliases：位编码不变，先保留当前旧 spelling/printer；
4. deleted/reserved TEPL parser/printer/decoder 清理；
5. `B.FPATR` 字段值和组合合法性检查。

不能机械改名/复制的 API：

- `TSORT` 规范要求两个输出：sorted values 和 original U32 indices；还有 `sort_width` 和 descending flag。现有 `TSORT32(dst, src)` 只有一个输出，不能直接改名冒充规范接口；
- `TPREFETCH` 没有 Tile destination，需确定 `byte_count` 是 shape、runtime GPR 还是双重重载，以及 inline-asm constraint；
- `MGATHER_CAS` 有 destination、base address、indices、expected、replacement 五个逻辑 operand，需确定 B.IOT/B.IOR 映射和 atomic contract；
- 六个 `TGEMV*` 虽然 LLVM function 已正确，但 TileOP 仍缺 C++ signature、shape、scale/bias/acc dtype 和 B.FPATR 动态参数/aux output 设计。

### 7. LLVM 已确认的非 encoding 数值问题

#### 7.1 Deleted/reserved TEPL raw 仍被接受

LLVM 仍保留或解码至少以下旧名称：

```text
TPRELU TAXPY TGATHERB TRANDOM TPARTARGMAX TPARTARGMIN
TRESHAPE TDEINTERLEAVE TINTERLEAVE TPUSH TPOP TALLOC TFREE
TFMOD TFMODS TADDC TSUBC TADDSC TSUBSC TLRELU
```

可直接修复：删除 parser/printer 旧名称，对规范 negative raw vector decoder fail 或显示 numeric/unknown，并补 MC tests。

#### 7.2 `B.FPATR` 只检查位宽

最新规范合法值：

```text
PreQuantMode = {0,1,2,3,4,5,12,13,16,17,18,19,20,23,24,25,26,27,28,32..39}
ReluMode     = 0..3
GroupNCode   = 0..9
boolean      = 0..1
```

组合约束还包括：

```text
RowMaxEn=0 => RowMaxInit=0
GroupMaxEn=0 => GroupNCode=0
GroupMaxEn=1 => GroupNCode!=0
RowMaxEn=0 && GroupMaxEn=0 => MaxAbsEn=0
```

LLVM 当前 `LinxV5InstrInfo.td` 只用 `uimm6/uimm3/uimm4/uimm1`，会错误接受：

```asm
B.FPATR 63, 7, 15, 1, 1, 1, 1
```

建议在 AsmParser instruction validation 中集中校验，并补正反例测试。

#### 7.3 `B.DATR` decoder overlap

规范合法 raw：

```text
[0x23,0x10,0x00,0x00]
```

应为全零字段 `B.DATR`，当前 LLVM 错误解码成 `B.CACR.STD 0`。需要分析 `B.CACR.STD` mask 和 decoder priority；不要仅凭调整 TableGen 定义顺序碰运气。

#### 7.4 `DTYPE_NONE=31`

LLVM 内部已有 `EMPTY_DataType=31`，但 printer 输出空字符串，parser 没有清晰的规范 spelling。最新规范至少允许它用于 `BSTART.TMOV` 和 `B.DATR`，但不是所有 form 均可用。

在修改 canonical printer 前，先从最新 ASL 确认最终 token 是 `DTYPE_NONE`、`NONE` 或其他 spelling，然后实施 per-form validation。

#### 7.5 `B.IOR` canonical syntax

最新 normative ASL：

```asm
B.IOR [<gpr>[, <gpr>[, <gpr>]]][, -><gpr>]
```

GitHub LLVM issue #38 曾使用无方括号示例，和当前 ASL 有冲突。不要直接按 issue 重写 parser/printer；先确认 issue 的规范基线或是否覆盖最新 ASL。反汇编还存在 bundle-context-sensitive 角色恢复问题。

### 8. Command / Scalar 已确认缺口

#### `ESAVE` / `ERCOV`

最新规范定义独立 32-bit command forms：

```text
ESAVE match=0x00002031 mask=0x06007fff operands=RegSrc0,RegSrc1,RegSrc2
ERCOV match=0x00003031 mask=0x06007fff operands=RegSrc0,RegSrc1,RegSrc2
```

LLVM 当前 `PseudoESAVE/PseudoERCOV` 是旧 Tile pseudo（Mode + Tile operand），不是同一接口。需要 ISA owner确认旧 pseudo 的迁移/兼容关系，不能直接替换。

#### 至少 8 个 32-bit scalar form 不一致

```text
CASB CASH CASW CASD DMA PRF PRFI.U BWT
```

- 前 7 个规范 raw 当前 decode fail；
- `BWT` raw 与 `trap` collision，当前错误打印为 `trap`；
- LLVM 的部分 `HL.*` 48-bit form不能替代规范 32-bit form；
- encoding 足以先补 MC skeleton，但完整 compiler support还需要 memory effects、ordering、register classes、intrinsic/lowering 决策。

自动 witness 注意事项：

- 全零 `ORI` 打印 `nop` 是合法 alias，不是 encoding 错；
- `BSTART.TLSU` 与 `BSTART.TLOAD` 可能只是 canonical 文本差异，位编码一致；
- `L.BSTART.*` 是两个 32-bit encoding pieces 组成的 64-bit form，不能只用第一片判断 decoder fail；
- 未经非零合法 witness 和人工复核的 identity difference 不要写成确定 bug。

### 9. 推荐后续实施顺序

#### P0：信息已清晰，可直接实施

1. 先保护工作树并验证第 4 节 TLSU function `8..14` 修复；只有用户明确授权后才提交/推送；
2. 清理 deleted/reserved TEPL parser/printer/decoder；
3. 实现 `B.FPATR` 合法值和组合校验；
4. LLVM 增加 `TSORT` canonical name，保留 `TSORT32` compatibility alias；
5. TileOP 增加普通 Local → Local `TMOV(dst, src)`；
6. 为以上行为增加精确 raw encoding 和 negative MC tests。

#### P1：encoding 清晰，但需兼容策略

1. 增加规范 `BSTART.<operation>` parser aliases；
2. 确认 spelling 后实现 `DTYPE_NONE=31` parser/printer 和 per-form validation；
3. 修复 `B.DATR`/`B.CACR.STD` decoder overlap；
4. 补齐 `TPREFETCH`/`MGATHER.CAS` 稳定 MC coverage 和必要 legality；
5. 分阶段实现 7 个缺失 scalar 32-bit MC form并单独解决 `BWT` collision。

#### 必须先问清再实现

```text
TileOP TSORT 双输出 ABI
TileOP TPREFETCH byte_count 与无 dst constraint
TileOP MGATHER_CAS 五 operand/atomic contract
六个 TGEMV* 完整 C++ ABI 与动态 B.FPATR schema
ESAVE/ERCOV 新 command 与旧 pseudo 的迁移关系
B.IOR 最终 canonical syntax/context-sensitive disassembly
DTYPE_NONE 最终 canonical token
B.DATR/CACR 和 BWT/trap overlap 仲裁规则
```

### 10. 关键报告和规范设计文档

重启后优先阅读：

```text
PTO_SPEC_LATEST_LLVM_TILEOP_FULL_AUDIT_20260812.md
  最新 4d115387 全量 audit、可实施/待澄清分类；本轮主报告。

PTO_TILEOP_SUPPORT_AUDIT_20260812.md
  SuperNPUBench 109 Tile operation 支持审计；其中早期 TLSU 状态已被最新报告修正。

PTO_B_FPATR_POSTPROCESS_HANDOFF.md
PTO_B_FPATR_COMPILER_REQUIRED_INFORMATION_RESPONSE.md
  B.FPATR / Matrix PostProcess 语义和 compiler-facing 答复。

PTO_B_FPATR_COMPILER_REQUIRED_INFORMATION.md
  早期编译器所需信息问题清单。

TLOAD_SHARED_PARAM_ISSUE.md
  Shared TLOAD、SharedTile 引用和 cross-function ABI 边界。
```

### 11. 对“当前所有 encoding 是否一致”的准确回答

只能按以下边界回答：

> 在当前本地未提交 TLSU 修复基础上，109 个 accepted Tile Operation 的 TLSU/CUBE function 和 TEPL selector 没有发现数值误差。

不能扩大成：

> 整个 LLVM ISA 与 PTO-SPEC 所有 command/scalar form、decoder legality、canonical syntax 和 CodeGen 都完全一致。

已确认仍存在 reserved TEPL、`B.FPATR` legality、decoder collision、command/scalar form 和 TileOP API 缺口。

### 12. 给新 Codex 的执行纪律

1. 开始任何修改前读本节和最新完整审计报告；
2. 每次改动先核验适用的 `AGENTS.md`；
3. 不覆盖用户未提交修改，不清理 untracked 文件；
4. 修改文件必须使用 `apply_patch`，不要用脚本整体重写大型源码；
5. 先跑最窄的 MC/CodeGen/TileOP compile test，再扩大验证；
6. 不顺手修复无关的 7 个既有 LinxV5 MC 失败；
7. 未经用户明确要求，不 commit、不 push、不 rebase；
8. 如果规范 ASL、issue 或旧 handoff 冲突，以最新 ASL 为准，并把冲突报告给用户，不自行猜测；
9. 涉及可能变化的远端规范/issue时重新 fetch 或在线核验，不假定本节永远是最新；
10. 任何声称“全部一致”的结论都必须限定到已实际审计的层次。

## 2026-08-11 B.FPATR / Matrix PostProcess 编译器实施设计（可开始，三项边界暂缓）

> **当前最高优先级工作包。** 本节基于冻结 handoff
> `PTO_B_FPATR_POSTPROCESS_HANDOFF.md` 和 compiler-facing 答复
> `PTO_B_FPATR_COMPILER_REQUIRED_INFORMATION_RESPONSE.md`。答复已经关闭绝大多数 compiler P0 信息，LLVM
> 可以开始实现。规范信息需求原表为 `PTO_B_FPATR_COMPILER_REQUIRED_INFORMATION.md`。
>
> 本节只把已经冻结的信息转成 LLVM/Clang 实施方法，不创建第二份 ISA 规范。若本节与落地后的 normative ASL
> 冲突，以 ASL 为准并停止相关实现，不得由 agent 自行选择。

### 0. 开始前状态与保护边界

- LLVM 当前基线：分支 `dev-llvm15-v5-encoding`，HEAD `9834588d5f959da92824970459c3d145ab056ab4`。
- `B.IOS` 32-bit reissue 已实现；不得恢复 `C.B.IOS` 或 mask-only Shared `B.IOT` companion。
- `B.FPATR` TableGen encoding 已存在；当前 MC emitter 仍把七个字段硬编码为 zero，只完成 canonical None skeleton。
- Function 9–14 必须保持 reserved/illegal；不得恢复六个 `*_FIXP` operation。
- `C` 始终是普通显式 Local source；不得恢复 architectural implicit ACC。
- 12 个 active operation ID、selector/function 不变。
- 当前工作树有用户文件和本地 handoff；不得 reset、checkout、删除或误提交无关内容。
- 本工作包默认只修改 LLVM 仓。TileOP API 适配应在 LLVM IR/backend ABI 稳定后单独实施和提交。

### 1. 已冻结、可以直接编码的合同

#### 1.1 B.FPATR

```text
width = 32
mask  = 0x00007fff
match = 0x00002023
canonical None = 0x00002023
```

字段顺序固定：

```text
PreQuantMode[5:0]
ReluMode[2:0]
GroupNCode[3:0]
RowMaxEn
GroupMaxEn
RowMaxInit
MaxAbsEn
```

- 12 个 active Matrix complete bundle 必须恰好有一条 `B.FPATR`。
- canonical None 也必须实际发出，不能根据全零配置省略。
- non-Matrix/non-CUBE 使用、missing 和 duplicate 属于 bundle-control legality。
- reserved mode 和 config/type/shape/operand 不匹配属于 tile legality。

#### 1.2 Accepted PreQuant / ReLU

完整 code、AccType、D dtype 和 scalar/vector carrier 直接使用
`PTO_B_FPATR_COMPILER_REQUIRED_INFORMATION_RESPONSE.md` 第 4、5 节，不重新编号。

实现必须特别保留：

- 未列出的 6-bit PreQuant code 全部 illegal；
- ReluMode 只接受 0–3；
- GroupNCode 只接受 0–9；
- U8 unsupported；
- generic FP8 归一化为 E4M3；
- shift code 12/13 的 descriptor `[31:13]` 是 signed S19，不是 FP19；
- None 要求 `RMode=NONE, Sat=0`；
- shift 要求 `RMode=NONE`，Sat 允许 0/1；
- 其他非零 mode 接受现有 RMode，NONE 按 RNE；
- RMode/Sat 只影响 D。

#### 1.3 Descriptor carrier

- scalar Quant：一个 i64/GPR raw descriptor；
- vector Quant：`1 x N` ND row-major U64 Local Tile；
- scalar LReLU：一个 i64/GPR，低 19-bit FP19，高位为零；
- vector PReLU：`1 x N` ND row-major U32 Local Tile，每元素低 19-bit FP19；
- constant 和 runtime descriptor 使用相同 raw ABI；
- unused/reserved bits 必须为零；
- constant descriptor 可在 Clang/LLVM 静态诊断，runtime descriptor 由 architecture preflight 检查。

#### 1.4 Operation base Local source 顺序

```text
TMATMUL             A, B
TMATMUL_BIAS        A, B, Bias
TMATMUL_ACC         C, A, B
TMATMUL_MX          A, ScaleA, B, ScaleB
TMATMUL_MX_BIAS     A, ScaleA, B, ScaleB, Bias
TMATMUL_MX_ACC      C, A, ScaleA, B, ScaleB
TGEMV               Matrix, Vector
TGEMV_BIAS          Matrix, Vector, Bias
TGEMV_ACC           C, Matrix, Vector
TGEMV_MX            Matrix, ScaleMatrix, Vector, ScaleVector
TGEMV_MX_BIAS       Matrix, ScaleMatrix, Vector, ScaleVector, Bias
TGEMV_MX_ACC        C, Matrix, ScaleMatrix, Vector, ScaleVector
```

PostProcess Local source append 顺序固定为：

```text
base Local mathematical sources
optional RowMaxIn          iff RowMaxEn && RowMaxInit
optional vector QuantParam iff PreQuantMode uses vector descriptor
optional vector PReLUParam iff ReluMode == PReLU
```

Local destination 顺序固定为：

```text
D
optional RowMaxOut
optional GroupMaxOut
```

GPR source dense order 固定为：

```text
optional scalar QuantParam
optional scalar LReLUParam
```

LReLU-only 必须使用 `RegSrc0`，不能保留旧固定 `RegSrc1` 规则。

#### 1.5 Shared routing

- Shared binder 不计入 8 个 Local Tile source 上限。
- RowMaxIn、vector Quant 和 PReLU 全部只能是 Local。
- Bias 和 C 保持 Local。
- plain TMATMUL：允许 Local A + Shared B，或 Shared A + Shared B；不支持仅 Shared A。
- MX Shared pair：两个 binder 是 Shared B/ScaleB；四个 binder 是 Shared A/ScaleA/B/ScaleB。
- Shared binder 使用独立 ordered `B.IOS` stream；Local operand 仍走 `B.IOT`。
- 所有 TGEMV 都是 Local-only，任何 `B.IOS` 都 illegal。

#### 1.6 Canonical B.IOT packing

```text
header_count = max(ceil(LocalSourceCount / 2), LocalDestinationCount)

for i = 0 .. header_count-1:
    src0 = S[2*i]     if present
    src1 = S[2*i + 1] if present
    dst  = D[i]       if present
    emit smallest exact B.IOT form; never add dummy Tile
    use common bundle PE_MASK
    set Last only on final header
    if dst exists, encode its own nonzero TSize
```

- source 和 destination 是独立 logical streams，同处一个 header 不建立角色配对。
- compiler 只生成上述最短 canonical packing。
- complete-bundle verifier 应重建 ordered streams；不能只按某一个固定 opcode 序列匹配。

#### 1.7 Result semantics

- D shape 为 `M x N`、ND row-major。
- RowMaxIn/Out dtype 为 AccType，shape 为 `M x 1`、ND row-major。
- GroupMaxOut dtype 为 AccType，shape 为 `M x ceil(N/GroupN)`、ND row-major。
- GroupNCode 映射：1–9 对应 8、16、32、48、64、80、96、112、128；0 是 disabled zero。
- RowMax/GroupMax 从 full-K P、在 quant/ReLU scaling 前计算。
- RowMaxInit=1 时读取 RowMaxIn，并输出 `max(old, current)`。
- GroupMax 每次 fresh 计算，无 input/init。
- D、RowMaxOut、GroupMaxOut 两两不同。
- `RowMaxIn==RowMaxOut` 和 `D==C` 合法，均为 read-old/write-new。
- 所有 Local source 必须在任何 destination publication 前 snapshot。
- D 和所有 enabled auxiliary outputs 是一个不可拆分的 result/commit group。

### 2. 三项暂缓边界：不得猜测

以下三项不阻止搭建完整 lowering 框架和实现其余模式，但相关最终诊断/测试必须暂缓：

1. **`PE_MASK=0000` fault precedence**：尚需确认它只绕过 data-path/runtime fault，还是也绕过
   missing/duplicate/config/schema 等结构性 fault。compiler 仍应始终生成结构合法 bundle；暂不写依赖某一 fault
   precedence 的负例结论。
2. **Local destination TSize 上限的统一表述**：当前 encoding 是 128B–8KB，代码可使用现有最小覆盖 size helper；
   但最终确认前，不新增声称 D/RowMax/GroupMax 超 8KB 精确 fault 分类的测试。任何不可表示的 compiler-generated
   result 必须静态拒绝，不能截断或隐式拆分。
3. **HiF8 的正式 dtype owner/枚举/DATR code**：PreQuant code 25/28 的 schema 和 carrier 可保留在表中，但在 owner
   确认前不得猜测 LLVM value type 或 B.DATR DataType。实现可先将 25/28 标记为 backend-known-but-lowering-gated，
   其他 accepted modes正常实施。

如实施过程中还需要改变 mode code、descriptor bits、operand order、operation selector、alias rule 或 fault class，必须停止
并返回用户；这些不属于编译器自决范围。

### 3. 推荐 LLVM IR 设计

#### 3.1 原则

- 非零 PostProcess 信息必须在 LLVM IR 中显式存在；MC emitter 不得根据 operation 名称猜测。
- 七个 FPATR fields 必须在 lowering 前成为 compile-time constant。
- D 和 auxiliary outputs 必须由同一个 intrinsic/call 语义产生。
- IR 可以使用 packed FPATR 或七个 immediate；推荐使用一个 target-specific packed i64 config，并在 verifier/helper 中
  统一解包，减少所有层重复 operand index。
- IR 中可以为 optional semantic role保留固定 slot；absence 必须由 config 决定，disabled Tile slot 使用 `undef`，不能
  用 GPR zero 表示 absence，因为 zero register 是合法参数值。
- 从 IR 到 Machine pseudo 时必须删除 disabled slots，只保留 architecture 实际消费的 ordered operands。

#### 3.2 推荐的固定 semantic slots

每个 operation intrinsic 在自己的 base mathematical operands 后增加：

```text
packed FPATR config
B.DATR RMode
B.DATR Sat
optional-role slot: RowMaxIn Tile
optional-role slot: vector Quant Tile
optional-role slot: vector PReLU Tile
optional-role slot: scalar Quant i64
optional-role slot: scalar LReLU i64
```

Config 决定每个 slot 是否必须存在。LLVM verifier/lowering 必须拒绝：

- enabled role 为 `undef`；
- disabled role 携带会被消费的非-undef Tile；
- mode 与 slot carrier 不一致；
- nonconstant config/RMode/Sat；
- result type、shape 与 config 不一致。

具体 intrinsic 命名和是否复用现有 D-only intrinsic 由实现 agent决定，但必须满足：

- canonical None 的现有 source/API 可以继续使用旧便利入口；
- lowering 最终都走同一个 Matrix PostProcess schema builder；
- 不形成“旧 intrinsic 不发 FPATR、新 intrinsic 才发 FPATR”的双轨。

#### 3.3 多结果表示

推荐按 enabled result mask 建立四种签名/返回形式：

```text
D
D + RowMaxOut
D + GroupMaxOut
D + RowMaxOut + GroupMaxOut
```

可使用 LLVM intrinsic 多返回、aggregate return 或对应固定 intrinsic family；不要把一个架构 Matrix operation拆成
多个独立 intrinsics。SelectionDAG node和 Machine pseudo必须在一个节点/指令上同时定义所有 enabled outputs。

为控制改动风险，实施顺序应为：

1. 先完成 D-only 非零 PostProcess；
2. 再加入 RowMax；
3. 再加入 GroupMax；
4. 最后覆盖 RowMax+GroupMax 和 3-destination packing。

### 4. Backend 内部设计

#### 4.1 建立单一 schema builder

不要继续在 `MCCodeEmitter` 中按 pseudo opcode和硬编码 operand index散落判断。新增或集中一个
`MatrixPostProcessSchema`/等价 helper，输入至少包括：

```text
operation kind
packed FPATR fields
RMode / Sat
base Local mathematical operands
Shared binders
optional semantic-role operands
enabled result types
PE_MASK
```

helper 输出：

```text
ordered Local sources
ordered Local destinations with TSize
ordered GPR sources
ordered Shared binders
validated B.FPATR fields
validated B.DATR fields
```

该 helper应成为 ISel/pseudo 构造、MC expansion 和 verifier 的共同逻辑来源；至少不能在三处复制不同的 arity 表。

#### 4.2 Pseudo operand layout

当前 `expandPseudoCCall` 依赖 `MI.getOperand(7)` 等固定下标并硬编码 FPATR zero。应重构为：

- 为 12 个 active Matrix pseudo建立共享 common-prefix；
- 显式携带七个 FPATR immediate或 packed config；
- 显式携带 RMode/Sat；
- 明确区分 base Local sources、optional Local sources、Shared binder和 GPR sources；
- pseudo `outs` 按 D、RowMaxOut、GroupMaxOut 顺序定义；
- 使用命名 enum/helper 访问 operand，不在 emitter 中继续散落裸数字 index；
- TableGen 用 multiclass生成 result-mask variants，避免手工复制 12×4 个 schema产生偏差。

#### 4.3 MC expansion 顺序

Matrix complete bundle 统一按当前 architecture owner要求生成：

```text
BSTART.CUBE
B.DIM / existing required headers
B.DATR
B.FPATR exactly once
ordered B.IOS stream if any
optional B.IOR exactly once if scalar parameters exist
canonical ordered B.IOT stream
```

如果当前已有 bundle 的非相关 header顺序与 normative ASL 不同，先核对 owner；不要仅为了本工作包重排无关 header。

`B.FPATR` emission 必须改为读取 pseudo config，删除“future change threads real operands”的 zero synthesis路径。

#### 4.4 DATR

- 从 PreQuant category、D dtype、RMode、Sat 联合验证和生成 DATR；
- canonical None 保持 AccType，RMode NONE、Sat 0；
- RowMax/GroupMax dtype始终是 AccType，不受 DATR RMode/Sat 影响；
- HiF8 DATR 暂按第 2 节 gate，不猜测 code。

### 5. 文件级实施顺序

#### 阶段 A：机械合同与 MC 基础

1. `llvm/lib/Target/LinxV5/LinxV5InstrInfo.td`
   - 收紧或增加 FPATR legal immediate predicate；
   - 建立 Matrix pseudo common schema/result-mask multiclass；
   - 保持 Function 9–14 无定义/illegal；
   - 不改变现有 B.IOS encoding。
2. `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.cpp`
   - 集中 12-operation identity/base schema；
   - 建立 schema/packing helper；
   - 生成 canonical B.IOT/B.IOR/B.IOS streams。
3. `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5MCCodeEmitter.cpp`
   - 删除七个 zero immediate synthesis；
   - 从 pseudo读取真实 FPATR/DATR/schema；
   - 保证 exactly-one FPATR。
4. `llvm/lib/Target/LinxV5/AsmParser/LinxV5AsmParser.cpp` 及 MC verifier 所在文件
   - 静态拒绝字段范围外 immediate；
   - reserved/config-context legality放入 complete-bundle verifier，不能让 raw decoder猜 dynamic arity。

阶段 A 可以使用 MIR/MC 或手工 pseudo测试先验证后端，不必等待 Clang API。

#### 阶段 B：LLVM IR 与 SelectionDAG

1. `llvm/include/llvm/IR/IntrinsicsLinx.td`
   - 增加统一 PostProcess semantic slots和多结果形式；
   - canonical None旧入口可转入同一 lowering。
2. `llvm/lib/Target/LinxV5/LinxV5ISelLowering.h/.cpp`
   - 扩展 `LinxV5ISD` Matrix nodes；
   - 替换仅按 `VUseNum` 拼接 operand 的 `lowerTemplateBLK/BLKMX` 路径；
   - 在单一 helper中解包 config、验证常量和构建 ordered roles；
   - 计算每个 enabled output 的 size，不只使用 `Op.getValueType()` 的单一 D size。
3. `llvm/lib/Target/LinxV5/LinxV5ISelDAGToDAG.cpp`
   - 根据 operation/result mask选择对应 pseudo；
   - 保持所有 outputs同一 node；
   - 保持 read-old/write-new source在 defs commit前可见。

不要把 optional operand扩展留到 MC emitter才第一次决定；ISel完成后 MachineInstr schema应已经确定。

#### 阶段 C：Clang builtin 和前端诊断

1. `clang/include/clang/Basic/BuiltinsLinxV5.def`
   - 增加或扩展 Matrix PostProcess builtins；
   - 保持 canonical None便利入口。
2. `clang/lib/CodeGen/CGBuiltin.cpp`
   - 构造统一 intrinsic；
   - 保持 config/RMode/Sat为 constant；
   - 按 result mask提取 D/RowMax/GroupMax。
3. LinxV5 Sema/CodeGen测试
   - 拒绝 nonconstant config、mode/carrier mismatch、缺失 enabled operand和错误 output type。

如果复杂 C++ Config主要由 TileOP模板提供，Clang builtin仍应保持低层机械 ABI，不把 PTO mode表复制成另一套不一致的
前端枚举。

#### 阶段 D：TileOP API（单独仓、单独提交）

- 在 `/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API` 基于现有 `fixp::Options`/公共 FPATR insertion演进；
- 公开统一 `postprocess::Config` 或等价 API；
- base Matrix API与非零 PostProcess都走同一 LLVM builtin/intrinsic ABI；
- 可短期保留 `fixp::Options` deprecated alias，但不得恢复 `TMATMUL_FIXP` operation；
- LLVM ABI未验证前不要先提交 TileOP 大规模接口重写。

### 6. 测试实施矩阵

#### 6.1 第一批必须通过

- `B.FPATR` canonical None word `0x00002023`；
- 12 个 active operation各恰好一条 FPATR；
- Function 9–14保持不能生成/不能接受；
- accepted/reserved immediate基础 MC正负例；
- D-only scalar Quant、vector Quant、LReLU-only、PReLU；
- code 12/13 signed S19 raw preservation；
- B.IOR Quant-only、LReLU-only、Quant+LReLU dense order；
- B.IOT 2/1、3/1、4/2、6/3、8/3 canonical packing；
- TGEMV使用 B.IOS负例；
- Shared TMATMUL auxiliary Local append order。

#### 6.2 Auxiliary output批次

- RowMax fresh；
- RowMax init；
- `RowMaxIn==RowMaxOut` read-old/write-new；
- GroupN 1–9；
- GroupN 0/10/15负例；
- partial final group和 GroupN>N shape；
- D+RowMax、D+GroupMax、D+RowMax+GroupMax；
- output-output alias拒绝；
- `D==C` 和合法 source/destination alias；
- 3-destination各自非零 TSize和 final Last。

#### 6.3 暂缓测试

- `PE_MASK=0000` 对 malformed bundle 的精确 fault precedence；
- 超 8KB destination 的最终 architecture fault class；
- HiF8 code 25/28 的 LLVM result type和 DATR encoding。

### 7. 推荐提交拆分

1. `[LinxV5][MC] Add Matrix PostProcess schema and canonical header packing`
2. `[LinxV5] Thread B.FPATR and DATR config through Matrix pseudos`
3. `[LinxV5] Add D-only Matrix PostProcess intrinsics and lowering`
4. `[LinxV5] Add RowMax and GroupMax multi-result lowering`
5. `[Clang][LinxV5] Add Matrix PostProcess builtin ABI`
6. TileOP 仓单独提交统一 Config/API迁移。

每个提交都必须保持 canonical None已有测试通过；不要把全部 12 operations、Clang、TileOP和多结果一次塞进一个不可审查
提交。

### 8. Agent 完成标准

一个实施 agent 只有在以下条件满足时才能声称对应阶段完成：

- 非零 FPATR不再由 MC emitter硬编码或猜测；
- 同一 config在 TMATMUL/TGEMV允许的 profile上使用同一 schema逻辑；
- Machine pseudo已经显式携带完整 dynamic schema；
- canonical emitter可以机械生成最多 8 Local sources、3 Local destinations和2 GPR sources；
- D 和 auxiliary outputs保持单一 IR/ISD/MI operation；
- missing/surplus optional role不能静默丢弃；
- Shared binder与 Local auxiliary stream不会混排；
- 三项暂缓边界没有被猜测或伪装关闭；
- focused MC/CodeGen/Clang测试和 `git diff --check` 通过；
- 未覆盖或受 baseline既有失败影响的测试在交接中精确列出。

### 9. 下一 agent 最短启动指令

```text
先实施阶段 A，不改 TileOP API。读取：
1. PTO_B_FPATR_POSTPROCESS_HANDOFF.md
2. PTO_B_FPATR_COMPILER_REQUIRED_INFORMATION_RESPONSE.md
3. CODEX_HANDOFF.md 本节

先建立单一 MatrixPostProcessSchema/helper，并用 MC/CodeGen 测试证明：
- 真实 FPATR fields 能从 pseudo进入编码；
- B.IOR dense packing正确；
- B.IOT 2/1 到 8/3 canonical packing正确；
- canonical None和现有 B.IOS行为无回归。

不要实施/猜测三项暂缓边界，不要恢复 FIXP functions或 implicit ACC，不要修改无关用户文件。
```

## 2026-08-07 B.IOS 替换 C.B.IOS —— 已实现（本地 commit，未推送）

> **最新状态**：32-bit `B.IOS` 替换 16-bit `C.B.IOS`（PTO v0.58 reissue / ADR 0054）已实现并验证，
> 保存在本地 commit（**未推送**）。本节的实现细节和验证结果是后续工作包的基线。

### 实现内容

**LLVM 仓**（commit `9834588d5f95`，11 文件）：
- 删除 `C_B_IOS`（16-bit），新增 `B_IOS`（32-bit，match `0x00001013`）。
- Shared 拼写 `S#n` → `S`（无 `#`）。
- 语法：source `B.IOS S12, mask=1111`；destination `B.IOS mask=0011, ->S12<128B>`。
  `printInst` 根据 TSize 0/非0 打印 source/dest 形式（方案 A：单个 B_IOS 指令）。
- `B_IOD_Nodst` 标记 `isCodeGenOnly`（B.IOD 待删，从 decoder 剔除，让 SharedTID=0 的 B.IOS 无歧义）。
- 删除 retired `B_IOT_NoSrc_NoDst_Size`（mask-only B.IOT companion，B.IOS 现承载 Shared size/mask）。
- MCCodeEmitter L2S/S2L/CUBE 生成点改为 B_IOS。
- 更新 v5-shared-* 测试（4 个，全部通过）。

**TileOP 仓**（commit `f6de757`，3 文件）：
- `template_asm.hpp`：38 处 `C.B.IOS` → `B.IOS`。
  - TMOV_L2S_INSERT/PUBLISH：destination `B.IOS mask=..., ->S<id><size>`。
  - TMOV_S2L_BROADCAST/EXTRACT：source `B.IOS S<id>, mask=...`。
  - CUBE Shared binders（SharedA/SharedB）：source `B.IOS S<id>, mask=1111`。
- `pto_tile.hpp`/`layout.hpp` 注释更新。

### 关键设计决策

1. **方案 A（单个 B_IOS 指令）**：用 TSize 0/非0 区分 source/dest，避免 decoder 歧义。
2. **B.IOS 自带 TSize/PE_MASK**：GM→Shared 的 size/mask 由 B.IOS 承载，**不再需要无 dst 带 TSize 的 B.IOT**。
   reissue 明确 "there is no mask-only B.IOT companion"。
3. **B.IOD 冲突**：B.IOS（SharedTID=0）与 B.IOD 编码重叠（都 opcode 0x13）。临时用 `B_IOD_Nodst`
   isCodeGenOnly 让 B.IOS S0 无歧义；B.IOD 待删（见 5.0.1）。

### 验证结果

- B.IOS source/dest round-trip、S0/S255 边界：✅
- L2S/S2L/CUBE 生成：✅（LLVM intrinsic + TileOP inline-asm）
- 7 个定向测试：✅ 全通过
- 全量 MC/CodeGen：✅ 无新增失败（历史遗留 Block-C/v4 测试不变；4 个 v5-shared 由失败转通过）

### GM→Shared TLOAD 接口（已内联汇编打通）

B.IOS 完成后，GM→Shared TLOAD 的编码基础已具备（B.IOS 承载 Shared destination 的 size/mask）。
已在 TileOP 库新增内联汇编接口（commit `d0678bd`，未推送）：

```cpp
SharedTile<T> sh = TLOAD<T, PEMask>(gm);
```

生成（符合 reissue GM-to-Shared Full Form）：
```asm
BSTART.TLSU TLOAD, FP32
B.DIM  ...
B.IOS  mask=1111, ->S0<4KB>   # B.IOS 承载 size + mask
B.IOR  [a0,a3], []             # GM 地址（RegDst 零）
```
端到端验证：TLOAD 到 Shared → TMOV.S2L 读回，round-trip 正常。
**不再需要 mask-only B.IOT companion**（B.IOS 直接承载 Shared size）。

### `TLOAD(sh, gm)`（SharedTile 作参数）—— 暂不可行

尝试 `TLOAD(a, b)`（a 是 SharedTile 参数）会在 RegAlloc 崩溃（`setRegClass` assertion）。
根因：`a` 是内存结构体对象，`=Sr` 输出后需把 Shared_ABS 值 store 到 `a` 内存，但
**Shared_ABS 后端无 store/spill/copy 支持**（copyPhysReg 无分支、loadRegFromStackSlot 不含
Shared_ABS）。深层原因：ISA **没有** Shared_ABS ↔ GPR/内存的通用搬移指令（Shared handle 是
8-bit 编号，`c.movr` 的 DstR/SrcL 只有 5-bit，无法编码 256 个 Shared 寄存器）。

**设计上 Shared handle 从不落内存**（reissue 语义），因此：
- 返回形式 `SharedTile<T> sh = TLOAD<T,15>(gm)` 可用（handle 保持寄存器）✅
- 参数形式 `TLOAD(a, b)` 需要 Shared 落内存，硬件不支持 ❌
- 「仿照 tile 照搬 copy」不可行（Tile 有 PseudoTCOPY/PseudoTSpill，Shared 没有对应搬移指令）

详细分析见本地文档 `TLOAD_SHARED_PARAM_ISSUE.md`（不提交）。

### 待办

- **B.IOD 正式删除**（等用户确定，见 5.0.1）。
- 提交暂存本地，未推送。

## 2026-08-10 SuperNPUBench Issue #42 审计与解决方案

> Issue：`https://github.com/PTO-ISA/SuperNPUBench/issues/42`。
> 审计对象：`bin-20260806`（356 ELF/356 disassembly）、当前 PTO spec、当前 LLVM/TileOP 主线。
> 本节是 Issue #42 后续派工入口。不要直接照抄 issue 中已经过时的 `docs/instructions/...` 路径。

### 结论与版本边界

Issue #42 的主体问题真实存在，不是 QEMU/gfrun 模型误报。`bin-20260806` release 明确使用 LLVM commit
`2a4648d` 构建，SuperNPUBench tag 为 `5831d7f`；Issue 中 1–6 项都能由发布源码、PTO 条款或当前 lowering
路径得到证据。第 7 项已正确标记为规范待确认，不能先当成模型 bug。

版本需要固定：Issue 标题写 PTO v0.3，但 `bin-20260806` programming guide 写的是 PTO v0.57.1；当前公开
pto-spec 已到 v0.58，目录从 `docs/instructions/...` 改为 `docs/block/...`、`docs/tile/...` 和 ASL。后续实现必须
记录具体 spec tag/commit，不能使用“latest”。本轮没有重新下载扫描 356 个 ELF（release 资产下载速度异常），
Issue 中的精确数量暂按报告保留；问题真实性基于规范和源码确认。

### 状态矩阵

| # | 问题 | 发布 ELF | 当前主线状态 | 结论 |
|---:|---|---|---|---|
| 1 | `B.DATR.DataType=31` | 报告确认 | LLVM 仍有 `EMPTY_DataType=31`，expander 仍可能写 BDATR | 🔴 真实，仍需修 |
| 2 | ordinary Local TMOV 丢 destination | 报告确认 | `606fa9833f57` 已修 `PseudoTCOPY` operand order | 🟡 历史真实，需回归封口 |
| 3 | TCMP/TCMPS 缺 CMode | 报告确认 | TileOP 部分 inline asm 不发显式 CMode；LLVM 有固定 EQ 风险 | 🔴 真实，仍需修 |
| 4 | CUBE D dtype/size 非法 | 报告确认 | `495cf9a`/`2a4648d` 已修 PE-local TSize；AccType/shape 仍需验证 | 🟡 部分已修 |
| 5 | `TMATMUL.ACC` C 顺序/类型错误 | 报告确认 | D/C 已部分显式化，canonical C/A/B 与 AccType 检查未封口 | 🟡 真实，仍需迁移 |
| 6 | 非法 dtype/删除 opcode | 报告确认 | wrappers 仍有宽泛 tile concept，旧 selector 仍可见 | 🔴 真实，需统一 gate |
| 7 | swiglu descriptor、MX legality | 待确认 | 缺完整 descriptor/MX legality matrix | ⚪ 暂不归责 |

### 1. B.DATR DataType=31

**根因**

- `LinxV5BaseInfo.h` 定义 `EMPTY_DataType=31`；当前 `B.DATR` 合法集合不含 31。
- `LinxV5InstPrinter.cpp` 把 31 打印为空字符串，掩盖非法编码。
- `LinxV5TileOpExpand.cpp` 的 TMOV/TLOAD 等路径和 `LinxV5MCCodeEmitter.cpp` 仍可能把该 sentinel 编入 BDATR。
- `PadValue=Null` 只能表示 PadValue union，不表示 dtype 可以为空。

**解决方案**

1. 将“未使用 dtype”与编码 31 分离；optional/sentinel 绝不能进入 MCInst operand。
2. 不需要 DATR 的 operation 直接不生成 `B.DATR`；需要 DATR 的 operation 使用真实合法 dtype。
3. parser/verifier 拒绝 15、21–23、29–31；printer 对 raw 31 显示 invalid，不再静默为空。
4. 扫描 LLVM、TileOP、SuperNPUBench 所有 `B.DATR ... Null` 模板：补实际 dtype，或删除无语义 DATR。

**文件/测试**：`LinxV5BaseInfo.h`、`LinxV5TileOpExpand.cpp`、`LinxV5MCCodeEmitter.cpp`、`LinxV5InstPrinter.cpp`、
`LinxV5AsmParser.cpp`；增加 MC invalid-dtype、TMOV/TLOAD CodeGen、objdump raw-31 负测；新 ELF 的 dtype=31 必须为 0。

### 2. ordinary Local TMOV destination

发布问题根因是 `PseudoTCOPY` 布局 `[Dst,TSize,Src]` 被按错误顺序展开，导致 `B.IOT` 丢失 `->Dst`。
`606fa9833f57` 已改正 operand `(0,1,2)`，该修复应保留。

**剩余方案**：增加正式 MC/CodeGen 回归；按 function verifier 区分 Local→Local（必须 src+dst、非零 size）、
Local→Shared Insert/Publish（允许 source-only，但必须 `B.IOS` Shared dst）、Shared→Local（必须 Local dst）。
普通 TMOV 不得使用 source-only TSize；重生成相关 17 个 ELF 后关闭子项。

### 3. TCMP/TCMPS CMode

PTO 0.58 的 TCMP/TCMPS DATR contract 要求 comparison 通过 `CMode` 显式表达。当前 TileOP 部分 wrapper 只发
BSTART/B.DIM/B.IOT，C++/CPU sim 中的 `CmpMode` 未可靠进入 asm；LLVM generic 路径还有固定 EQ 风险。

**解决方案**

1. `CmpMode` 作为 compile-time attr/immediate，TileOP 生成带 CMode 的 B.DATR。
2. LLVM TCMP/TCMPS pseudo 增加 CMode operand，expander读取 operand，删除固定 EQ。
3. parser/verifier 拒绝缺 CMode、重复 DATR 和 mode>5。
4. CPU sim/JCore 共用同一 mode 编码表。
5. 覆盖 EQ/NE/GT/LT/GE/LE 的 MC round-trip、缺失/非法 mode 负测和浮点/整型代表 case。

### 4. CUBE D AccType 与 PE-local 容量

当前 `495cf9a80681` 已改为 PE-local `128B..8KB` 口径，`2a4648d08088` 已从 tile type size 直接编码，旧
4-PE divisor 问题可视为已修。但 D dtype、shape 和容量仍需 verifier 封口。

**解决方案**

- canonical None 的 Matrix D 为 AccType（当前规则 S32/F32），shape 为 PE-local M×N。
- PostProcess 后 D/aux dtype 按 Config 推导，每个 destination 独立计算 size。
- lowering/verifier 校验 `required_bytes <= allocated_bytes` 以及 M/N/D bytes/TSize 一致；禁止 A/B dtype Tile 冒充 D。
- 64×64 FP32 做容量边界正负测；FA/cube 超限时分块，不得通过缩小 dtype 欺骗编码。

### 5. TMATMUL.ACC C 顺序/类型

Issue 对“C 是显式 AccType Tile、不是 implicit ACC”的判断正确。当前 helper 已采用 `TMATMUL_ACC(D,C,A,B)`，
但 LLVM `getBIOTFromInst()` 仍大量依赖 operand number 分支，需要 schema 化。

**解决方案**

- 固定 Local 顺序：base `A,B`；bias `A,B,Bias`；acc `C,A,B`，最后一条 B.IOT 写 `->D<Size>`。
- Shared A/B 从 Local stream 移除；C 始终 Local、第一 logical source。
- TileOP static_assert C/D 为 M×N AccType、layout/size compatible，A/B 的 role/K 正确。
- `D==C` 使用 read-old/rename-new；asm output early-clobber，C 保持独立 input。
- 覆盖 Local、Shared-B、Shared-A/B、同/不同 D/C 和错误 dtype/shape 负测。

### 6. 统一 opcode×dtype legality

本项真实，不能靠放宽模型 gate。当前 wrappers 仍有宽泛 `is_tile_data_v`，`TAXPY` 等旧 selector 仍可从
`template_asm.hpp` 发出。需要按固定 PTO spec source-lock 建 machine-readable legality 表。

**解决方案**

1. 表中固定 operation、active/removed、data/index/output dtype、required DATR fields。
2. TileOP 为 TABS、TANDS/TORS/TXORS/TSHLS/TSHRS、THISTOGRAM、TROWMAX/TROWSUM、TSELS、MSCATTER 使用专用 concepts。
3. LLVM parser/MC verifier重复做最终 gate；手写 asm也拒绝非法 tuple。
4. TAXPY/TPRELU 等不在 active catalog 时，从 BaseInfo、TableGen、printer、TileOP wrapper 和 benchmark compile list 删除。
5. 不支持的算法需求只能使用规范允许的 TCVT/布局变换或改算法，不能让模型接受非法 ELF。
6. 每个 Issue tuple 增加 compile-time/MC 负测和合法边界正测；release 前扫描全部 ELF。

### 7. swiglu/MX 暂不归责

Issue 的保守判断正确。TROWMAX 的每行结果被 TMAX 消费是否 descriptor-compatible，需要明确 valid-region、
physical allocation 和隐式 broadcast；MX A/B/Scale dtype、HiF4x2、REUSEA 需要完整 legality matrix。

**解决方案**

- PTO spec 增加 reduction-result descriptor/consumer contract；若禁止隐式兼容，SuperNPUBench 显式使用
  TEXPANDS/TRESHAPE/layout transform。
- 发布 machine-readable MX matrix（A/B/Scale dtype、AccType、layout、K granularity、reuse）。
- 合同缺失前 LLVM/TileOP fail-closed，不放宽 QEMU/gfrun，不以旧 ELF 是否运行反推合法性。

### 8. 实施顺序与发布门禁

建议独立提交：LLVM B.DATR dtype gate → TileOP/LLVM TCMP CMode → TMOV function-aware verifier →
operation×dtype concepts/MC gate → Matrix D/C schema → SuperNPUBench 重生成 → 双模型验证。

发布必须附 pinned PTO spec commit、LLVM/TileOP/SuperNPUBench manifest 和静态 ELF legality audit；分别统计
illegal、trap、timeout、exit、value mismatch。finisher PASS 不能替代数值 golden，非法 ELF 不得进入性能统计。

### Agent 分派模板

```text
阅读 CODEX_HANDOFF.md 的“2026-08-10 SuperNPUBench Issue #42 审计与解决方案”，只实现问题 N。
固定 PTO spec tag/commit，先核对当前 HEAD 是否已有部分修复，再修改对应仓和正负测试。
不要放宽模型接受非法 ELF，不要覆盖其他 agent 修改，不要提交 CODEX_HANDOFF.md。
验证通过后按仓分别 commit/push；若规范缺失或测试失败先汇报，不要 push。
```

## 2026-08-07 PTO ISA v0.58 指令设计审计（本地分析，未推送）

> ⚠️ **本节 0.1–0.4 基于原始 0.58（`f7d2d0c`）**，其中 C.B.IOS 已被 reissue（`8a77c9f`，ADR 0054）
> 的 32-bit B.IOS 取代。**以 reissue 为准**，见下方「2026-08-07 以 v0.58 reissue（ADR 0054）为准的待办更新」。
> 本节保留作历史审计参考。

> 仓库：`PTO-ISA/pto-spec`，tag `v0.58` = commit `f7d2d0c`（本地克隆 `/tmp/pto-spec`）。
> 权威依据：`docs/architecture-decisions/0052-pto-isa-0580-davincioo-catalog.md`（ADR 0052）、
> `docs/instructions/tile/*.md`、`asl/bundle/dispatch.asl`。本节为 v0.58 相对 0.57.1 的落地差异审计，
> 与本仓库 LLVM/TileOP 实现的对齐缺口。

### 0.1 v0.58 总体变更（ADR 0052）

- 操作数收敛 **120 → 109**：87 TEPL + 10 TLSU + 12 CUBE；99 bundle/command forms。
- 指令增删：
  - ➕ `GMOV`（TLSU Func13）、`TFMA`（Mode0/Func28 `0x01C`）、`MGATHER.CAS`（TLSU Func8）、
    `MGATHER.MASK`/`MSCATTER.MASK`（TLSU Func6/7）。
  - ❌ 删 `ACCCVT`、`TRANDOM`。
  - 🔄 `TSORT` 替代 `TSORT32`（显式 `sort_width`，selector `0x06C`）。
  - 🔧 `TSEL`=Mode0/Func26（`0x01A`）、`TSELS`=Mode1/Func26（`0x03A`）；`TADDC` 不接受。
- 架构模型：**无 architectural implicit ACC**；12 CUBE 全写显式 Local D；ACC variant 读显式 Local C
  （read-old/write-new）。`B.FPATR` 是 12 个 active operation 的 mandatory header。
- 保留 6 个 Linx-only 二值向量编码（`BSTART.VPAR` 等）但 PTO 不执行，
  在 `spec/catalog/linx-vector-reservations.json`。

### 0.2 B.IOT 三种形式（v0.58 关键设计）

`docs/instructions/block/operands/B.IOT.md` + `asl/bundle/dispatch.asl:1302-1360`：

| 形式 | 汇编 | 编码字段要求 |
|---|---|---|
| ① 普通 Local dst | `B.IOT SrcTile0, SrcTile1, mask, last, ->DstTile<SIZE>` | 有 src + TSize + DstTile |
| ② TMOV L2S source-only | `B.IOT SrcTile0, mask, TSize=SIZE` | 有 src + TSize≠0，**DstTile==0** |
| ③ mask-only companion | `B.IOT mask, last` | **无 src、TSize==0、DstTile==0** |

- ASL 用 `mask_only_binding`（无src && 无src1 && TSize==0 && DstTile==0）区分 ③，
  且 `shared_mask_only` 需 `BundleSharedTLSUSelected()`（selector 00000/00001/01110）。
- `local_to_shared` = TileMemory && selector∈{01001,01010}（TMOV.L2S）。
- `local_destination` = 有TSize && !local_to_shared && !mask_only。
- **无歧义**：三种形式字段组合互斥；mask-only 强制 TSize==0，与带 dst 形式编码可区分。
- **关键结论**：之前 LLVM 实验的 `B_IOT_NoSrc_NoDst_Size`（TSize≠0 无 dst）**不符合 v0.58**，
  应废弃。v0.58 的 mask-only 强制 `TSize==0`。

### 0.3 C.B.IOS 修改（本小节重点）

**当前 LLVM 写法**（与 v0.58 编码一致，语法有差异）：

```tablegen
def C_B_IOS : BlockModifierBase {
  bits<8> SharedTID;
  let Size = 2;
  let Inst{15-14} = 0b11;  let Inst{13-6} = SharedTID;  // 8-bit
  let Inst{5-4} = 0b11;    let Inst{3-1} = 0b110;       let Inst{0} = 0b0;
  let AsmString = "C.B.IOS" # "\t$SharedTID";
}
```
- `InstrInfo.td:2750`；Operand `SharedTIDOp`（imm，`parseSharedTID`/`printSharedTID`/`getImmOpValueSharedTID`）
  + `SharedRegOp`（`Shared_ABS` 寄存器，`RegisterInfo.td:577`）。
- **Parser**（`AsmParser.cpp:2349`）：只认 `s#` 前缀，如 `s#17` → TID=17，范围 0..255。
- **Printer**（`InstPrinter.cpp:421`）：输出 `S#` + Value，如 `S#17`。
- **编码**：16-bit，prefix `11`/`11`，opcode `110`，`SharedTID` 8-bit —— **与 v0.58 一致** ✅。
- **方向不编码**：`BindBundleSharedIO`（`asl/bundle/state.asl:337`）只存 8-bit `shared_id`，
  无方向位；source/destination 由 BSTART Function + binder 顺序确定。

**v0.58 与当前实现的差异（待对齐）**：

| 项 | 当前 LLVM | v0.58 要求 |
|---|---|---|
| source 拼写 | `C.B.IOS S#17`（带 `#`）| `C.B.IOS S17`（无 `#`）|
| destination 拼写 | 无 | `C.B.IOS -> S17`（新增）|
| 编码（16-bit, 8-bit TID）| ✅ 一致 | ✅ 一致 |
| 方向是否编码 | 不编码 | 不编码（上下文定）|
| TGEMV/reserved 拒绝 | 需确认 | 必须报明确 profile diagnostic |

- v0.58：`C.B.IOS S17`（source）/ `C.B.IOS -> S17`（destination）；旧 hash-prefixed `S#n`
  **不是 0.58.0 syntax**。role 由 Function/binder 个数/顺序确定，不另占字段。
- 方向语义：`-> Sx` = destination（TLOAD、Local→Shared TMOV）；`Sx` = source
  （TSTORE、Shared→Local TMOV、所有 CUBE binder）。
- Profile Collision：C.B.IOS 只对 CUBE Func 0-2、4-6 合法；TGEMV（16-18、20-22）与
  CUBE reserved（9-14）遇 prefix 必须报诊断；TLSU 允许 TLOAD Func0、TSTORE Func1/14、
  TMOV Func9-12 单 binder。

### 0.4 TLSU Function 表（v0.58 完整）

| Func | 操作 | schema |
|---:|---|---|
| 0 | TLOAD（Shared form = GM2S full）| Local `B.IOT(dst)+B.IOR`；Shared `C.B.IOS+B.IOR` |
| 1 | TSTORE（Shared = S2GM full）| Local `B.IOT(src)+B.IOR`；Shared `C.B.IOS+B.IOR` |
| 2 | Local TMOV | `B.IOT(src,dst)` |
| 3 | TPREFETCH | Local/cache |
| 4 | MGATHER | Local |
| 5 | MSCATTER | Local |
| 6 | MGATHER.MASK | masked-gather |
| 7 | MSCATTER.MASK | masked-scatter |
| 8 | MGATHER.CAS | atomic gather-CAS |
| 9 | TMOV.L2S.INSERT | `B.IOS+B.IOT(Local src)` |
| 10 | TMOV.L2S.PUBLISH | `B.IOS+B.IOT(Local src)` |
| 11 | TMOV.S2L.BROADCAST | `B.IOS+B.IOT(Local dst)` |
| 12 | TMOV.S2L.EXTRACT | `B.IOS+B.IOT(Local dst)` |
| 13 | GMOV | `B.IOT(Local src,dst,PE_MASK,TSize)+B.IOR(peer_tid,0,0)` |
| 14 | TSTORE.SPART | `B.IOS+B.IOR` |
| 15-31 | reserved | illegal |

- GM→Shared size 编码在 `B.IOS.TSize`；Local↔Shared 在 destination `B.IOS` capacity + source-only `B.IOT`。
- GMOV 是固定 Core4 collective，无 scope 重载；`TSize` 表示每个 PE 的 Local Tile，`B.IOR(peer_tid,0,0)`。
- TGEMV 拒绝所有 Shared binder。

## 2026-08-07 以 v0.58 reissue（ADR 0054）为准的待办更新（本地分析，未推送）

> **重要**：PTO ISA 0.58 在 reissue commit `8a77c9f`（"spec: reissue 0.58 with PE-local B.IOS"，#50）
> 被重新发布，ABI string 仍为 `0.58.0`，但内容擎换。**ADR 0054 取代本节 0.1–0.4 的
> 原始 0.58（`f7d2d0c`）设计**。所有待办以 **reissue（`8a77c9f`）为准**。
> 依据：`docs/architecture-decisions/0054-pe-local-tile-size-and-32-bit-shared-io-binding.md`。
> 旧 0.58（`f7d2d0c`）构建物 stale，不得与 reissue 工具链混用。

### 5.0 v0.58 reissue 的核心变更（ADR 0054）

1. **`C.B.IOS`（16-bit）被移除**，由 **32-bit `B.IOS`** 取代。
   - `B.IOS`：match `0x00001013`, mask `0xf00871ff`；`SharedTID[27:20]`、`PE_MASK[18:15]`、
     `TSize[11:9]`、`funct3[14:12]=001`、opcode `[6:0]=0x13`；reserved `[31:28]/[19]/[8:7]` 全零。
   - `TSize=0` = source form（`B.IOS S12, mask=1111`）；`TSize=1..7` = destination form
     （`B.IOS mask=0011, ->S12<128B>`），声明 per-PE 容量 128B/256B/512B/1KiB/2KiB/4KiB/8KiB。
   - C.B.IOS 历史 raw words 只解码为 active `C.B.DIMI`，不再保留旧 mnemonic。
2. **`TSize` 与 `B.DIM` 都是 per-PE**（非 Core 聚合）。Core allocation = `popcount(PE_MASK)*per_pe_size`。
   - mask bit 固定：`1000=PE0, 0100=PE1, 0010=PE2, 0001=PE3`；不允许向低位 pack。
   - `PE_MASK=0000` 是 strict no-op（不 allocation/rename/read/access/consume/fault）。
3. **Shared size 承载迁移**：GM→Shared TLOAD 的 size/mask 来自 `B.IOS`；不再用 `B.IOR.RegDst`
   重解释为 Shared size；**不再有 mask-only `B.IOT` companion**。
4. **`B.IOT` 只绑定 Local**：无 mask-only Shared form，无 `.reuse` modifier；
   source-only form `TSize=000`，size 来自 rename-resolved descriptor。
5. **`B.IOR` 只绑定 GGPR（R1..R23）**：绝对寄存器，value 0 无效；不接受相对 `T#/U#`。
   一个 block 最多一个 `B.IOR`；`B.IOR` 不承载 Shared size。
6. **编译器拥有物理 Shared 寄存器分配**：新 `Sx` 按 mask/descriptor 兼容性分配；
   destination write 更新 allocation mask 的 subset 但不得扩展。
7. **`B.IOS` 至多 4 个 ordered binder**；unconsumed ID 不得重复；role 必须与 operation schema 一致。
8. **reissue 的 TLSU/CUBE Function 表**（更新 0.4 节）：
   - TLSU：0=TLOAD,1=TSTORE,2=TMOV,3=TPREFETCH,4/5=MGATHER/MSCATTER,6/7=MASK,8=MGATHER.CAS,
     9/10=TMOV.L2S.INSERT/PUBLISH,11/12=TMOV.S2L.BROADCAST/EXTRACT,13=GMOV,14=TSTORE.SPART,15-31=reserved。
   - CUBE：0-6 active（TMATMUL 系列），8/9-14 reserved/illegal，16-18/20-22 TGEMV，23-31 reserved。
   - **CUBE 表中 Function 8 是 reserved**（legacy removed selector），**与 TLSU 的 Function 8=MGATHER.CAS 不同**。

### 5.0.1 B.IOD 待删除（⚠️ 团队决定，2026-08-07）

- **B.IOD（Block Input/Output Dependency）将被删除**（团队确认取消，删除时机待定，等用户确定后执行）。
- 当前 B.IOD 在 reissue 中仍标 `status: active`，但团队不再需要它。
- **编码冲突**：B.IOS 与 B.IOD 共享 opcode `[6:0]=0x13` 与 funct3 `[14:12]=001`。
  当 B.IOS 的 `SharedTID=0`（合法 S0）时，encoding 与 B.IOD（DepSrc0=0, DepDst=0）完全重叠，
  decoder 无法区分。**删除 B.IOD 后此冲突自然消失**。
- **TODO**：用户确定删除时，删除 LLVM `B_IOD`/`B_IOD_Nodst` 指令定义 + parser/printer/encoder/decoder，
  并同步 pto-spec。（当前实现先接受此冲突，SharedTID=0 的 B.IOS 暂不保证 decode。）

### 5.1 以 v0.58 reissue 为准的待办清单（含汇编/反汇编可行性）

> 原有工作包 A-J 需按 reissue 修订。以下为增量/修订待办，可行性标注：
> 🟢 直接可做（编码对齐）｜🟡 需设计/中等工作量｜🔴 高难度/需硬件确认。

| # | 待办 | 当前实现 | v0.58 reissue 要求 | 可行性 |
|---|---|---|---|---|
| R1 | **删 `C_B_IOS`，加 32-bit `B_IOS`** | `C_B_IOS`（16-bit，td:2750）+ parse/print/emit | 删除 C.B.IOS；新增 `B_IOS`（match `0x00001013`）| 🔴 高：指令身份 16→32，编码空间归 C.B.DIMI |
| R2 | **TLSU Function 偏移修正** | INSERT=8,PUBLISH=9,BRD=10,EXT=11,SPART=12 | INSERT=9,PUB=10,BRD=11,EXT=12,SPT=14；+MGATHER.CAS=8,TPREFETCH=3 | 🔴 高：需硬件确认编码 |
| R3 | **B.IOS 语法（S17 无# + `->S17<size>`）** | 无 | `B.IOS S12, mask=1111` / `B.IOS mask=0011, ->S12<128B>` | 🟡 中：parser/printer，块上下文敏感 |
| R4 | **废弃 mask-only B.IOT** | `B_IOT_NoSrc_NoDst_Size`（td:3002）| 删除该形式；B.IOT 只 Local | 🟢 低 |
| R5 | **B.IOR —— 不改动** | B.IOR 用 `GPRSrcNoR0`/`R0`（绝对 GPR，R1-R23），方括号格式 `[$src],[$dst]` | reissue：`B.IOR RegSrc0, RegSrc1, RegSrc2, ->RegDst`（无方括号，dest 用 `->`）| ✅ **决定不改动**：编码与 reissue 逐位一致（`[31:27]RegSrc2,[26:25]00,[24:20]RegSrc1,[19:15]RegSrc0,[14:12]0,[11:7]RegDst,[6:4]1,[3:1]1,[0]1`）；寄存器类（绝对 GPR）与 ABI 名（R0=zero,R1=sp,R2-9=a0-7,R10=ra,R11-19=s0-8,R20-23=x0-3）已一致。仅语法格式（方括号 vs 无方括号+`->`）不同，但**不追平**，保持现有 `[$src],[$dst]` 形式。|
| R6 | **B.IOR 块上下文 arity —— 不改动** | 固定 operand 数 | reissue 按 operation schema 决定省略/默认/zero 打印 | ✅ **决定不改动**：与 R5 一并保留现有固定多 operand 形式；不实现 issue #38 的块上下文 schema 解析（open detail，成本高）|
| R7 | **Shared 寄存器分配（编译器）** | 无 | 编译器按 mask/descriptor 分配新 `Sx` | 🟡 中 |
| R8 | **MGATHER.CAS（TLSU Func8）** | 无 | 新增 pseudo + MC 编码 | 🟡 中 |
| R9 | **TSORT32 → TSORT** | TileOP `TSORT32` | `TSORT` + `sort_width` operand（selector `0x06C`）| 🟡 中 |
| R10 | **ACCCVT 残留清理** | ISel 仍引用 `BLK_ACCCVT` | 删除 | 🟢 低 |
| R11 | **TFMA 独立指令** | 仅 TileOP `BSTART.TEPL 28` | LLVM 需独立指令（Mode0/Func28）| 🟡 中 |
| R12 | **TGEMV 拒绝 Shared binder** | 已实现 TGEMV | 验 reject 所有 `B.IOS` | 🟢 低（验证）|
| R13 | **B.DATR/B.DIM 可省略** | 每次发 | 普通 op 可省略，CUBE 必须 | 🟢 低（优化）|

### 5.2 汇编/反汇编可行性详析

**正汇编（parser + encoder）**：
- R1/R3（B.IOS）：中等。新增 32-bit 指令定义 + `parseBIOperand`（识别 `S17`/`->S17<size>`/`mask=`），
  encoder 按 `TSize` 零/非零写 source/destination role。需配合 C.B.DIMI 编码空间清理。
- R5/R6（B.IOR）：✅ **决定不改动**。B.IOR 当前已是绝对 GPR（`GPRSrcNoR0`，R1-R23），编码与
  reissue 逐位一致。保留现有方括号 `[$src],[$dst]` 语法与固定 operand 数，不追平 reissue 的
  无方括号 `src, ->dst` 形式，也不实现块上下文 schema 解析（issue #38 的 open detail，成本高）。
- R4（B.IOT mask-only 删除）：低。删 `B_IOT_NoSrc_NoDst_Size` + 拒绝 `mask-only` 文本。

**反汇编（disassembler + printer）**：
- R1/R3：中。disassembler 需识别 32-bit `B.IOS`（match `0x00001013`），并打印 `S17`/`->S17<size>`。
  C.B.IOS 历史 words 归 C.B.DIMI。**块上下文敏感**：size/role 打印需 bundle 解析。
- R6：✅ 不改动。B.IOR 反汇编保持现有固定 operand 形式，不实现块上下文 schema 解析。
- R2：中。Function 表修正后，反汇编按新表解码。

**依赖/阻塞**：
- R1/R2/R3 相互耦合（B.IOS 语法 + Function 表 + 弃 C.B.IOS）。
- R5/R6 已决定不改动，不再构成耦合。
- **R2 需硬件确认**（TLSU Function 偏移是否已按 reissue 布局）。
- 所有改动"no compatibility output"（ADR 0054），旧 0.58 ABI 不兼容。

## 2026-08-06 TSize 改为 PE 粒度（已推送）

### 需求

Tile size 从 **whole-core（4 PE 合计）粒度** 改为 **per-PE（fragment）粒度**。程序员定义时以 PE 为粒度
（含 M/N/K），填多大就编码多大；硬件自动 ×4 换算成 core size。合法显式 size 从 `512 B..32 KB` 改为
**`128 B..8 KB`（per PE）**。

### 编码与 ISA 对齐结论

- TSize 仍是 3-bit，编码编号 `001..111`（=1..7）**与 ISA 文档 `ENCODING.md` 完全一致**。
- 变化的是每个编号的**解释**：从 core 总量改为 PE 分量（PE×4=core）。ISA 文档 TSize 表文本仍写
  core 粒度（512B..32KB），未更新为 PE 粒度说明；这是文档语义与实现解释的差异，物理等价。
  若需严格对齐，请 ISA owner 更新 `ENCODING.md`/`B.IOT.md` 的 TSize 表标注。

### 提交（已推送）

**LLVM `495cf9a80681` → `linxisa/dev-llvm15_56`**，8 文件：
- `AsmParser/LinxV5AsmParser.cpp`：`matchTileSizeHelper` → 128B/256B/512B/1KB/2KB/4KB/8KB
- `LinxV5AsmPrinter.cpp`：`%Z` modifier TileSizes[] → PE 粒度
- `MCTargetDesc/LinxV5InstPrinter.cpp`：TileSizes[] → PE 粒度
- `LinxV5ISelLowering.cpp`：`calculateVCallSizeMask` 除以 4、检查 128B..8KB、编码 `Log2(peBytes)-6`
- `LinxV5InstrInfo.td`：TSize 注释更新
- 测试：`v5-b-iot-tilesize-syntax.s`、`v5-b-iot-non-last-dst.s`、`v5-matmul-local-tile-result.ll`

**TileOP `71d4b38` → `origin/linx`**，2 文件：
- `include/jcore/type.hpp`：`tile_type_traits.TilesizeCode = mapBytesToEnum(sizeof/4)`；`__tilesize_code`
  改为 128B/256B/512B/1KB/2KB/4KB/8KB；`IsValidActiveSize` 边界 128B..8KB；删除 `peLocalBytes` 变量
  （内联为 `coreTileBytes / 4`）
- `docs/tileop-usage/constraints.md`：PE 粒度 size 表

### 验证

- `TileRight<float,32,32>`（core 4096B，PE 1KB）→ `->t<1KB>` ✅
- `TileRight<float,8,16>`（core 512B，PE 128B）→ `->t<128B>` ✅
- `TileRight<float,128,64>`（core 32KB，PE 8KB）→ `->t<8KB>` ✅
- `TileRight<float,256,128>`（core 128KB，PE 32KB，越界）→ 后端正确拒绝 ✅
- 4 个活跃 v5 MC/CodeGen 定向测试全 PASS ✅

### 注意事项

- M/N/K 本来就是程序员原始值（未 ×4），无需改动；只有个别拼接/展开运算自带 `*2`/`*4`，属操作语义。
- `build/lib/clang/15.0.4/include/tileop-api/` 的 resource-dir 头已同步。
- 历史遗留失败（`b.iot.s`/`vcall`/`mcall`，v4 旧语法）与本次无关。

## 2026-08-05 最新 DavinciOO ISA 对齐总交接（当前最高优先级）

> **本节是当前唯一权威的“未实现需求与设计”入口。**
>
> 它依据 `/home/zhuwei/linx-llvm/DavinciOO_intrinsic_changes_since_3b4fe5e.md` 刷新，覆盖后文所有与
> implicit ACC、独立 `TMATMUL*_FIXP`、12 个 TMATMUL 接口统一加 FPATR、TGEMV Shared form 等相关的旧计划。
> 后文旧内容仅保留历史上下文，任何冲突均以本节为准。
>
> 本文件只允许保存在本地工作区；**禁止提交或推送 `CODEX_HANDOFF.md`**。

### 0. 权威来源与已发现的 ISA 文档冲突

当前实现应以以下优先级解释 ISA：

1. `/home/zhuwei/linx-llvm/DavinciOO_intrinsic_changes_since_3b4fe5e.md`：本轮最新 Matrix contract 的权威增量。
2. `/home/zhuwei/DavinciOO/isa/intrinsic/header/*.md`、总览页和已更新 operation 页：用于补充 bitfield、header
   顺序、operand role、shape 和 legality。
3. `/tmp/DavinciOO-intrinsic` 或 commit `3b4fe5e`：仅是旧基线，不得用于否定本轮增量。

本地 `/home/zhuwei/DavinciOO` 当前 HEAD 为 `ebe725b`，但工作树中的若干 Matrix 单页仍是旧基线内容。例如
`TMATMUL*.md`、`TGEMV*.md` 仍出现 `->ACC<Size>`、implicit ACC 或 ACCCVT 描述，且缺少 `PostProcessConfig`；
与此同时，变更摘要明确要求“ordinary Local D、无 architectural ACC、每个 active Matrix block 恰好一个
`B.FPATR`”。这是 **ISA 文档生成/落盘不完整**，不是实现应继续采用旧 ACC 模型的理由。

因此其他 agent 实现时必须遵循：

- 遇到单页旧 `->ACC` 与变更摘要冲突时，采用显式 Local `D`。
- 遇到旧 `*_FIXP` 页面、Function 9–14 或“仅 FIXP 发 FPATR”描述时，视为删除后的历史内容。
- 不自行猜测尚未在增量摘要中给出的 GMOV/event bitfield；缺少编码合同的部分先完成 API/验证框架或等待 ISA
  owner 补充，禁止臆造 encoding。
- 建议 ISA owner 另行把本轮最终 Markdown/HTML/Excel projection 同步到 `/home/zhuwei/DavinciOO`；此动作不属于
  linx-llvm agent 的提交范围。

#### 0.1 2026-08-05 最新两笔提交复核结论

本轮已重新阅读并对比：

- LLVM `36a8c5103097 [LinxV5] Emit B.FPATR for every TMATMUL/TMATMULMX CUBE family`
- TileOP `e6b6bc8 [tileop-api] Add FPATR options overload to all 12 TMATMUL* interfaces`

当前远端 HEAD 即为 LLVM `36a8c5103097` 和 TileOP `e6b6bc8`。本文中的“当前 v3 定义”特指本文件引用的
`DavinciOO_intrinsic_changes_since_3b4fe5e.md` Matrix 增量合同，而不是旧发布包中“6 base + 6 FIXP”的历史
operation 集合。

这两笔提交已经推送到各自远端 HEAD，工作区中不再是 WIP。它们解决了旧 v3 实现中的一个真实问题：

- LLVM 的旧 12 个 Matrix pseudo（6 个 base TMATMUL + 6 个 `*_FIXP`）现在都会发一条 FPATR。
- non-FIXP TMATMUL 默认发全零 FPATR，位置在 B.DATR 之后。
- TileOP 六个 non-FIXP TMATMUL 增加了无 options/default Attr 和显式 options 两种调用形式。
- 新增 MC/CodeGen/TileOP compile tests，固定了“每个旧 family bundle 一条 FPATR”的过渡行为。

但是，**这两笔提交实现的是旧 operation 集合上的 FPATR 扩展，不是当前 v3/最新 DavinciOO Matrix 定义的最终
实现**。最关键的集合差异是：

```text
提交中的“12 个” = 6 个 TMATMUL base + 6 个 TMATMUL_FIXP
当前定义的“12 个” = 6 个 TMATMUL + 6 个 TGEMV
```

因此不能因为测试覆盖了“12 个”就把 Matrix FPATR 工作整体标记完成。应按以下方式评价：

| 提交成果 | 是否保留 | 原因/后续动作 |
|---|---|---|
| `B.FPATR` MC encoding、header 顺序 | ✅ 保留 | 当前定义仍要求每个 active Matrix block 恰好一条 |
| non-FIXP TMATMUL 自动发 zero FPATR | ✅ 保留并泛化 | canonical None 合法；后续扩展到 TGEMV，并接真实 Config operand |
| `isMatmulPseudo()` 公共判断框架 | 🟡 重构后保留 | 删除 FIXP pseudo case，加入六个 TGEMV case，最好重命名 `isActiveMatrixPseudo()` |
| `isFixpResultPseudo()`/`isFixpMatmulPseudo()` | ❌ 最终删除 | 当前定义没有独立 FIXP operation/result family |
| 6 个 FIXP mnemonic/Function 9–14 | ❌ 删除 | 9–14 必须 reserved/illegal |
| TileOP options overload 机制 | ✅ 保留模式 | 无参数 overload 转发 canonical None 是推荐接口；options 类型需泛化为 PostProcessConfig |
| `is_basic_fixp_attr()` 限制 | ❌ 删除/替换 | quant/PReLU/RowMax/GroupMax 不应再要求 `.FIXP` variant |
| `MatmulFPAttr.cpp` 12 接口测试 | 🟡 重写 | 改为 6 TMATMUL + 6 TGEMV；删除所有 FIXP 调用 |
| `v5-matmul-fpatr.s` 12 Function 测试 | 🟡 重写 | active Function 改为 0–2、4–6、16–18、20–22；9–14 转为负测 |

#### 0.2 与当前 v3 定义仍存在的具体差异

1. **Operation 集合错误**：LLVM/TileOP 仍把 Function 9–14 和六个 FIXP API 当 active；TGEMV 16–22 未实现。
2. **FPATR 值仍未端到端传递**：LLVM commit 明确只合成七个 zero immediate；TileOP non-FIXP 虽接受
   `FixpAttr`，真实非零值主要依赖 inline-asm 文本，通用 pseudo/ISel schema 尚未接通。
3. **PostProcess 能力仍人为分裂**：non-FIXP API 通过 `is_basic_fixp_attr()` 只允许 keep-acc/f16/bf16/relu；
   quant、PReLU、RowMax、GroupMax 的诊断仍要求 `.FIXP variant`，与当前统一 PostProcess 定义冲突。
4. **physical ACC 尚未删除**：`Tile_ACC1`、`ACC_DST`、`ACC_SRC` 和 implicit ACC pseudo expansion 仍存在。
5. **API 类型命名仍是旧模型**：公开模板参数仍是 `FixpAttr`/`fixp::Options`，尚未形成 operation-independent
   `PostProcessConfig`；可保留 source alias，但实现内核不应继续以 FIXP 命名。
6. **Shared MX operand 仍不完整**：ScaleA/ScaleB 多处仍要求普通 Tile，未实现 Shared data/scale pair 和
   `Left, ScaleLeft, Right, ScaleRight` binder。
7. **测试验收标准仍按旧 family**：新测试证明的是旧 12 family 一条 FPATR，尚未证明当前 12 active Function、
   reserved 9–14、普通 Local D/C、TGEMV Local-only 和完整 PostProcess arity。

#### 0.3 基于这两笔提交的具体迁移方式

**LLVM：`LinxV5TileOpExpand.cpp/.h`**

- 保留“Matrix bundle 无条件插入一条 FPATR”的公共路径。
- 将 `isMatmulPseudo()` 重构为 active Matrix 集合：TMATMUL 0–2、4–6 和 TGEMV 16–18、20–22。
- 删除 `isFixpResultPseudo()`、`isFixpMatmulPseudo()` 及所有以 `.FIXP` 决定 B.DATR/result/ACC 语义的分支。
- zero FPATR 仅作为 canonical None fallback；为 active Matrix pseudo 增加七字段 immediate 和 Config 决定的可选
  Tile/GPR operand，不能永久在 expander 中写死七个零。
- 将 B.IOT 构造改为 schema 驱动：基础 source、PostProcess source、C/Bias、D/RowMaxOut/GroupMaxOut；所有 Local
  destination 带非零 size。

**LLVM：`LinxV5InstrInfo.td`、BaseInfo、InstPrinter**

- 删除六个 FIXP pseudo/mnemonic/profile；Function 9–14 不再映射 public operation。
- 新增六个 TGEMV pseudo/profile；与 TMATMUL 共用 Matrix FPATR operand class。
- 删除注释和 class 名中的“FIXP-only FPATR/result”假设。
- ACC variant 的 C 改为普通 `TILE_Src_Reg`，D 改为普通 `TILE_DstWithArrow`。

**LLVM：测试迁移**

- `v5-matmul-fpatr.s`：保留 0–2、4–6；将 9–14 正例删除并改成 reserved/illegal 负测；增加
  16–18、20–22 正例。
- `v5-matmul-fpatr.ll`：从“non-FIXP + FIXP 代表例”改为“TMATMUL + TGEMV 代表例”，增加非零 FPATR operand
  的 CodeGen 检查。
- `v5-matmul-fixp.ll`：迁移为 `v5-matrix-postprocess.ll`；所有 `.FIXP` expected mnemonic 改成 active opcode。
- `v5-shared-register-allocation.ll`：保留 Shared allocation 检查，但 expected block 使用 active TMATMUL + FPATR。

**TileOP：`include/jcore/template_asm.hpp`**

- 保留“无 options overload + 显式 options overload”接口模式。无 options 调用必须内部转发到 canonical None；
  不建议把 Config 放在 `WaitEvents...` 前用普通默认参数解决重载。
- 将 `FixpAttr`/`fixp::Options` 实现内核泛化为 `postprocess::Config`；为兼容已有源码，可暂时提供
  `using FixpAttr = PostProcessAttr`、`namespace fixp` deprecated builder alias。
- 删除六个 `TMATMUL*_FIXP` public wrapper 和 helper；其 quant/PReLU/RowMax/GroupMax 分支合并到六个 active
  TMATMUL helper。
- 删除所有 `"require a .FIXP variant"` 静态诊断和 `is_basic_fixp_attr()` 门禁，改成按 Config 精确验证
  source/destination arity、dtype、shape 和 storage。
- 新增六个 TGEMV overload，复用同一个 Config/builder，但 concept 强制 Local-only、`M==1`。

**TileOP：测试和文档**

- `MatmulFPAttr.cpp` 改为 6 TMATMUL + 6 TGEMV；每个 operation 覆盖无参数 canonical None 和显式 nonzero
  Config。
- 新增参数化 Config 测例：quant、PReLU、RowMax、GroupMax，确认不再需要 FIXP operation 名。
- `docs/tileop-usage/tmatmul-fixp.md` 删除或重写为 `matrix-postprocess.md`，明确 FPATR 是所有 active Matrix
  operation 的 mandatory header。
- 旧 FIXP 名若提供 deprecated source wrapper，测试必须确认它生成 Function 0–2/4–6，绝不能生成 9–14；
  最终推荐直接删除，不保留 wrapper。

本轮完成的是 commit diff、当前源码和 handoff 的静态审计，并未重新运行 `36a8c5103097`/`e6b6bc8` 的测试命令。
提交中声明的旧集合定向测试结果可以作为过渡实现记录，但工作包 J 仍必须针对当前 active Function 集合重新构建和
验证。

### 1. 最新 Matrix 合同摘要

#### 1.1 Active/Reserved Function 表

| Family | Function | 语义 | 当前实现状态 |
|---|---:|---|---|
| `TMATMUL` | 0 | `P=A*B; D=PostProcess(P)` | 🟡 有旧 opcode/lowering，缺统一 PostProcess，仍混有 ACC 模型 |
| `TMATMUL_BIAS` | 1 | `P=A*B+Bias; D=PostProcess(P)` | 🟡 同上 |
| `TMATMUL_ACC` | 2 | `P=A*B+C; D=PostProcess(P)` | 🔴 C/D 仍依赖旧 ACC operand/pseudo |
| `TMATMUL_MX` | 4 | `P=MXMatMul(A,SA,B,SB); D=PostProcess(P)` | 🟡 有旧实现，Shared scale role 不完整 |
| `TMATMUL_MX_BIAS` | 5 | `P=MXMatMul(...)+Bias; D=PostProcess(P)` | 🟡 同上 |
| `TMATMUL_MX_ACC` | 6 | `P=MXMatMul(...)+C; D=PostProcess(P)` | 🔴 C/D 仍依赖旧 ACC operand/pseudo |
| `ACCCVT` | 8 | legacy removed | 🔴 必须保持 reserved/illegal，清理 active 残留 |
| old `TMATMUL*_FIXP` | 9–14 | reserved/illegal | 🔴 LLVM/TileOP/测试仍完整暴露六个 FIXP opcode/API |
| `TGEMV` | 16 | `M=1; P=A*B; D=PostProcess(P)` | 🔴 未实现 |
| `TGEMV_BIAS` | 17 | `M=1; P=A*B+Bias; D=PostProcess(P)` | 🔴 未实现 |
| `TGEMV_ACC` | 18 | `M=1; P=A*B+C; D=PostProcess(P)` | 🔴 未实现 |
| `TGEMV_MX` | 20 | `M=1; P=MXMatMul(...); D=PostProcess(P)` | 🔴 未实现 |
| `TGEMV_MX_BIAS` | 21 | `M=1; P=MXMatMul(...)+Bias; D=PostProcess(P)` | 🔴 未实现 |
| `TGEMV_MX_ACC` | 22 | `M=1; P=MXMatMul(...)+C; D=PostProcess(P)` | 🔴 未实现 |

所有 12 个 active Matrix operation 的共同规则：

- 每个 block 必须有且只有一个 `B.FPATR`；canonical None 也不能省略。
- canonical None：`PreQuant=None`、`ReLU=None`、`RMode=NONE`、`Sat=0`；输出仍是 AccType/逻辑
  `TileAcc` role，但物理上是普通 Local TReg。
- quant/convert/ReLU 后，D 是 PostProcess 推导 dtype 的普通 Local ND Tile。
- base 从 zero 初始化；BIAS 在乘积累加前加 Bias；ACC 在乘积累加前读显式 C。
- PostProcess 只在完整 K 累加后执行一次。K blocking 使用显式 `_ACC(D,C,...)` 链；nondefault `AccPhase`
  必须拒绝。
- D、RowMaxOut、GroupMaxOut 的 destination 固定顺序为 D、RowMaxOut、GroupMaxOut，作为原子结果组
  ready/retire/flush。
- RowMax/GroupMax 在 ReLU、quant、convert 前观察 P；RowMax shape 为 `M x 1`，GroupMax shape 为
  `M x ceil(N/GroupN)`。
- config 精确决定附加 Tile/GPR source 与 auxiliary destination 数量，禁止携带未消费的闲置 operand。
- 所有 Matrix Local destination 必须有非零 `TSize`。

#### 1.2 Canonical block 顺序

所有 active Matrix bundle 固定为：

```asm
BSTART.CUBE <active-op>, <DataTypeA>
B.DATR ...                 # 仅当 DataTypeB/其他 DATR 非默认时
B.FPATR ...                # 必须且恰好一条
B.DIM ..., ->LB0           # M；TGEMV 必须静态/可证明为 1
B.DIM ..., ->LB1           # N
B.DIM ..., ->LB2           # K
C.B.IOS ...                # 仅 cooperative TMATMUL 有 Shared operand 时
B.IOT ...                  # Local source stream + Local D/aux destinations
B.IOR ...                  # FPATR 选择 GPR 参数时；cooperative 时各 PE 值必须相同
```

实现可按现有 parser/printer 的 canonical header 次序微调 `B.DIM` 与 binder 的内部收集方式，但最终汇编、
object round-trip 和 verifier 必须保证一个 Matrix bundle 恰好一条 FPATR，且 FPATR 位于 Matrix operation
数据/operand headers 之前，不能再依赖“opcode 名含 `.FIXP`”来决定是否生成。

### 2. Local/Shared operand 合同

#### 2.1 Ordinary Local D/C 与逻辑 TileAcc

- 架构上不再存在每 PE 唯一 implicit ACC singleton。
- `TileAcc` 只允许作为 C++/SSA 类型角色，最终分配到普通 Local `T/U/M/N` TReg。
- 所有 Matrix operation 显式定义 Local D；`*_ACC` 显式读取 Local C。
- `D == C` 使用 read-old/rename-new；只有 dtype、shape、layout、allocation 完全兼容时合法，禁止物理寄存器
  的 destructive in-place overwrite。
- 删除/停用 `ACC_DST`、`ACC_SRC`、`Tile_ACC`、`ACC_TILE_*`、`Tile_ACC1` 和依赖这些实体的 pseudo expansion。
- `ACCCVT` 不得作为把隐式 ACC 导出到 Tile 的迁移手段；Function 8 保持 illegal。

#### 2.2 Cooperative TMATMUL Shared A/B

只有六个 TMATMUL operation 支持 cooperative Core4 Shared input；TGEMV 全部禁止 Shared。

| 形式 | Shared binder 固定顺序 | Local `B.IOT` source stream |
|---|---|---|
| non-MX Local A + Local B | 无 | A, B, `[Bias/C]` |
| non-MX Local A + Shared B | `Right` | A, `[Bias/C]` |
| non-MX Shared A + Shared B | `Left, Right` | `[Bias/C]` |
| non-MX Shared A + Local B | **非法** | 不得生成 |
| MX Local pairs | 无 | A, ScaleA, B, ScaleB, `[Bias/C]` |
| MX Local A pair + Shared B pair | `Right, ScaleRight` | A, ScaleA, `[Bias/C]` |
| MX Shared A pair + Shared B pair | `Left, ScaleLeft, Right, ScaleRight` | `[Bias/C]` |
| MX Shared A pair + Local B pair | **非法** | 不得生成 |

额外约束：

- Shared A 是 `MShard4`，不是 broadcast/`Replicated4`；PE p 消费 A[p] fragment。
- MX data/scale 必须同存储域：A 与 ScaleA 全 Local 或全 Shared；B 与 ScaleB 同理，禁止混合。
- 所有 Shared ID 必须互不相同，并从 Local `B.IOT` source stream 中移除。
- 只要 B 或 B/ScaleB 为 Shared，就使用固定四 PE cooperative form，`PE_MASK=1111`。
- Shared input 必须 fully-defined；四 PE 必须静态收敛，并以相同动态顺序到达。
- cooperative block 的 FPATR GPR 参数、shape/dtype/config 等标量在四 PE 上必须相同。

#### 2.3 TGEMV 边界

- 六个 TGEMV operation 都是 single-PE、PE-local，且 `M=1`。
- A/B/ScaleA/ScaleB/Bias/C/D/aux output 全部只能是 Local Tile。
- 禁止 SharedTile、`C.B.IOS`、Core4 rendezvous、Shared pair 和 cooperative mask 语义。
- K blocking 只使用显式 `TGEMV_ACC(D,C,...)` 链。

### 3. 仓内实现对比与整体结论

#### 3.1 一句话结论

当前并不是从零实现：LLVM/TileOP 已具备 **B.IOT size、SharedTile/Shared register allocation、Local↔Shared
基础搬运、六个旧 TMATMUL 的 MC/inline-asm 骨架，以及完整度较高的旧 FIXP/PostProcess options**。这些基础约
一半可以复用；但 Matrix 的核心架构模型仍是旧 ISA，即“physical implicit ACC + 独立 FIXP Function 9–14”。
因此最新 ISA 对齐不是增量补几个 opcode，而是要先完成一次受控的 Matrix 模型迁移。

推荐理解为三类：

- **绿色：已经正确实现，可直接复用并保持不动。**
- **黄色：仓内已有功能或骨架，但语义属于旧 ISA，需要迁移/封口，不能直接宣称完成。**
- **红色：最新 ISA 功能基本未实现，需要新工作包。**

#### 3.2 已正确实现、应直接复用

| 能力 | LLVM 仓 | TileOP 仓 | 复用方式 |
|---|---|---|---|
| B.IOT destination `<size>` | parser/printer/encoding 已完成，commit `0509c2ca0c55` | inline asm 已同步，commit `94af6a6` | 保持 canonical `->t<Size>`，不要改 encoding |
| Shared absolute register | 已有绝对 `S#0..S#255` 类、分配 pass、`Sr` constraint | `SharedTile` 使用 compiler-managed handle | E/H/G 直接复用，不允许用户固定 S 编号 |
| Local→Shared 基础搬运 | `TMOV_L2S_INSERT/PUBLISH` 已编码、打印和 lowering | 已有 publish helper | 可作 fallback/producer，但不能替代 ISA 原生 Shared TLOAD |
| Local Matrix destination suffix | 普通 Local D 与非零 TSize 已有路径 | inline asm 使用 `=&Tr` 和 `%Z[TileSize]` | B/C 扩展到全部 D/aux output |
| 动态 valid shape | 后端 DIM 基础已存在 | 214 个静态 DIM operand 已迁移为运行时 GPR | TLOAD/TSTORE/Matrix 新接口复用 |
| 旧 PostProcess 字段编码经验 | `B.FPATR` MC/header 定义存在 | `fixp::Options` 已覆盖 quant、PReLU、RowMax、GroupMax、keep-acc | C 中泛化为 active Matrix `PostProcessConfig` |
| Shared inline-asm handling | constraint/modifier/fixup 基础由 `42bf7bd32bc8` 完成，当前 LLVM HEAD 为 `36a8c5103097` | SharedMatmul 已有 smoke usage | macOS/Linux 新模板继续走该基础 |

以上绿色能力不是本轮删除对象。尤其不能因为移除 FIXP API，就把 `fixp::Options` 中已经验证过的字段编码经验整体
删除；正确做法是把它迁移并重命名为 12 个 active Matrix operation 共用的 PostProcess 基础设施。

#### 3.3 已有实现但按最新 ISA 必须重构

| 旧实现 | 当前仓内情况 | 最新 ISA 要求 | 处理方式 |
|---|---|---|---|
| 六个 `TMATMUL*_FIXP` | LLVM 定义/打印 Function 9–14；TileOP 暴露六个 API；测试期待 `.FIXP` | 9–14 reserved/illegal | A/D 删除 public opcode/API，能力合并到 0–2、4–6 |
| physical ACC | `Tile_ACC1`、`ACC_DST/SRC`、`ACC_TILE_*` 和 pseudo 分支仍 active | TileAcc 仅逻辑 role，物理为普通 Local TReg | B 删除专用状态，所有 Matrix 显式 D，ACC variant 显式 C |
| 旧 12-family FPATR | LLVM `36a8c5103097` 已让 6 base + 6 FIXP 都发一条；TileOP `e6b6bc8` 已给 non-FIXP 增加 options overload | 当前 12 active 是 6 TMATMUL + 6 TGEMV；Config 必须支持完整 PostProcess | 保留公共插入/重载机制，删除 FIXP case、加入 TGEMV、接真实 Config operand |
| Shared matmul role 推断 | non-MX/部分 Shared A/B 已能生成；binder role 不完整 | 固定 Left/ScaleLeft/Right/ScaleRight | E 引入 role-aware binder 和 pair storage verifier |
| MX Shared scale | data A/B 可 Shared，ScaleA/ScaleB 多处仍按 Local `Tr` | data/scale pair 必须全 Local 或全 Shared | E 重写 MX helper，Shared scale 从 B.IOT 移除 |
| ACCCVT removed 注释 | 部分 pseudo 已删，但旧测试/文档/寄存器语言仍可能残留 | Function 8 永久 reserved/illegal | A/B 增加明确负测并清除 active 路径 |
| GMOV/TSTORE.SPART mnemonic | MC 层已有名称或旧编码测试 | 需满足新的 storage/distribution/participant 合同 | 不能按“有 mnemonic”判完成；等精确合同后做 G/H |
| Shared TLOAD 草案/WIP | TLOAD 基础存在，当前另有 agent 修改 TileOp expansion | 需要 Shared destination、scope、issuer、event、convergence 全合同 | 当前只算 WIP；完成 H/I 测试前不得标绿 |

黄色项目最容易造成误判：**能 assemble、能打印 mnemonic 或有 API，不代表已经符合最新 ISA。** 验收必须看
Function 编号、operand storage/role、FPATR 数量、D/C physical class、scope/convergence 和正负测试。

#### 3.4 基本未实现的最新 ISA 能力

| 能力 | 缺失内容 | 对应工作包 |
|---|---|---|
| 六个 PE-local TGEMV | Function 16–18/20–22、API、lowering、M=1/Local-only verifier、测试 | F |
| 统一 PostProcessConfig | active opcode 共用 API/schema、精确 Tile/GPR arity、dtype/shape 推导 | C/D/F |
| Matrix 原子多输出 | D/RowMaxOut/GroupMaxOut 固定次序、非零 size、原子 retire/flush | C |
| Shared-A/ScaleA 完整 cooperative form | role encoding、四 binder 顺序、pair storage 和重复 ID 检查 | E |
| Shared TLOAD/TSTORE/SPART | public API、scope、issuer、direct Shared operand、端到端 | H |
| RecordEvent/WaitEvents | SSA dependency、B.IOD/token lowering、优化属性 | I |
| pe/core scope 与 convergence | participant、相同动态顺序、identical scalar、fully-defined Shared verifier | I |
| 最新 GMOV | 最终 ISA 合同、各 direction/distribution、collective legality | G，当前被 ISA 文档阻塞 |
| reserved Function 完整边界 | 8、9–14 的 assembler/disassembler/objdump 负测 | A |
| Linux/macOS 全量验证 | clean build、MC/CodeGen、TileOP、SuperNPUBench、新功能端到端 | J |

#### 3.5 分仓结论

**linx-llvm 仓：**

- 已有 MC 编码、printer/parser、Shared register infrastructure、TileOp pseudo expansion 和旧 Matrix lowering 基础。
- 最大错误模型仍在 `LinxV5RegisterInfo.td`、`LinxV5InstrInfo.td`、`LinxV5ExpandPseudoInsts.cpp`、
  `LinxV5BaseInfo.h`、`LinxV5InstPrinter.cpp`：FIXP Function 和 physical ACC 尚未迁移。
- TGEMV 基本不存在；Shared movement/event/convergence 也未形成完整后端合同。
- 因而 LLVM 优先顺序是 A+B+C，再做 E/F/H/I。

**Linx-TileOP-API 仓：**

- 已有 SharedTile、inline-asm helper、动态 shape 和成熟的旧 `fixp::Options` builder。
- public API 仍暴露六个 FIXP operation；六个 base TMATMUL 还没有统一 Config；TGEMV/event/scope 缺失。
- Shared MX helper 的 scale storage 与 binder role 不满足最新 ISA。
- 因而 TileOP 优先顺序是等待 LLVM A+B+C schema 稳定，再做 D/E/F；H/I 可按明确 encoding 并行。

**DavinciOO ISA 仓：**

- 本轮变更摘要已经给出 Matrix 最新合同，是当前实现依据。
- 本地若干 operation 单页仍残留 `->ACC`/ACCCVT 旧文本，最终 GMOV 页也未在当前工作树中找到。
- 这些属于 ISA 文档同步问题：Matrix 按增量摘要实施；GMOV 不猜测，等待最终页/sidecar/Excel 行。

#### 3.6 总体实施判断

按功能量估算，绿色基础可复用约一半，但剩余工作集中在高耦合的核心模型，不能简单在旧 FIXP 路径旁边继续
叠加。最安全路线是：

```text
冻结 Function 表
  → 移除 physical ACC
  → 建立统一 FPATR/PostProcess schema
  → 迁移六个 TMATMUL API
  → 修正 Shared binder
  → 新增六个 TGEMV
  → Shared movement + event/convergence
  → Linux/macOS 全量验证
```

完成 A+B+C 前，不建议多个 agent 同时大规模修改 `LinxV5InstrInfo.td` 或 `template_asm.hpp`，否则很容易把旧
FIXP operand schema、Shared binder 和新 D/C 模型交叉冲突。可以并行的安全 sidecar 只有：准备 reserved Function
测试、整理 TGEMV 测试矩阵、取得 GMOV 最终 ISA 合同、准备 clean-build 脚本。

### 4. 当前实现状态矩阵

此前 LLVM 的 TileOp/FPATR 并发 WIP 已作为 `36a8c5103097` 提交并推送，当前 LLVM tracked 仅本地 handoff。
TileOP 的对应 API 工作也已作为 `e6b6bc8` 提交并推送。TileOP 仓仍有非本 agent 产生的 tracked 修改
`docs/tileop-usage/tmatmul-fixp.md`，以及既存 untracked
`test/tileop_api/src/MultiThreadAdd.cpp`。后续 agent 必须先确认这些改动的 owner/意图，不得覆盖、reset 或误并入
自己的提交。

| 需求 | LLVM | TileOP API | 测试/文档 | 结论 |
|---|---|---|---|---|
| Function 9–14 reserved | 仍定义/打印 FIXP | 仍有 6 个 API | MC/CodeGen 仍期待 `.FIXP` | 🔴 未实现 |
| 无物理 ACC | 仍有 `ACC_*` 类/pseudo | 文档/旧测例仍有 TileAcc/ACC 语言 | assembly test 仍用 `ACC#1` | 🔴 未实现 |
| 六个 TMATMUL mandatory FPATR | `36a8c5103097` 已对旧 base family无条件发一条，当前仍写死 zero | `e6b6bc8` 已有 default/options overload，但只允许 basic Attr | 有旧 12-family 正例 | 🟡 基础完成；完整 Config 与新 active 集合待迁移 |
| Shared A non-MX | 已有部分 inline-asm 支持 | 已支持代表形式 | 有 SharedMatmul smoke test | 🟡 需按 role/binder 新合同复核 |
| Shared A/ScaleA MX | data A 可 Shared，但 scale 多处仍为 Local `Tr` | pair storage 校验不完整 | 无四 binder 顺序覆盖 | 🔴 未实现 |
| 六个 TGEMV | 未见 Function/pseudo/lowering | 未见公开实现 | 无测试 | 🔴 未实现 |
| Matrix D/aux Local nonzero TSize | D 已有 Local result 路径 | D suffix 已支持 | aux output 未覆盖 | 🟡 部分实现 |
| RowMax/GroupMax 原子多输出 | FIXP options 有部分 operand 经验 | 绑定在旧 FIXP API | 无 active-op 多输出矩阵 | 🔴 需迁移重构 |
| ACCCVT Function 8 illegal | 有 removed 注释/部分删除 | 旧文档仍可能引用 | 缺明确 illegal MC test | 🟡 需封口 |
| GMOV 新 collective 合同 | 有旧 opcode/测试 | 无完整新合同审计 | ISA 细节不足 | 🟡 待 ISA 合同后实现 |
| Shared TLOAD | MC 有 TLOAD，Shared dst API 未完成 | 已有单独设计草案 | 无端到端 | 🔴 未实现 |
| Shared TSTORE/SPART | 有旧 mnemonic/encoding 痕迹 | API 未完成 | 无语义矩阵 | 🔴 未实现 |
| RecordEvent/WaitEvents/scope/convergence | 无完整表示/verifier | API 未形成 | 无负测 | 🔴 未实现 |
| B.IOT destination `<size>` | 已完成并推送 | 已同步 | 定向/端到端通过 | ✅ 保持不动 |
| Shared absolute register allocation/`Sr` | 已完成并推送 | 已使用 | 有定向测试 | ✅ 复用，不重写 |

### 5. 推荐提交拆分与依赖图

#### 工作包当前状态（复核 `36a8c5103097` / `e6b6bc8` 后）

| 工作包 | 状态 | 最新说明 |
|---|---|---|
| A Function/MC legality | 🔴 未开始 | 新提交仍保留 FIXP 9–14，TGEMV 未加入；现有测试需迁移而非直接删除 |
| B ordinary Local D/C | 🔴 未开始 | physical ACC 全部仍在 |
| C unified PostProcess/FPATR | 🟡 过渡基础已提交 | mandatory zero FPATR 和 overload 机制完成；真实 operand、完整 Config、多输出、新 active 集合未完成 |
| D TileOP TMATMUL API migration | 🟡 过渡 overload 已提交 | non-FIXP 已能接 basic options；仍需删除 FIXP API并吸收其完整能力 |
| E Shared binder | 🔴 未开始 | 新提交没有修正 Shared ScaleA/ScaleB 与 role binder |
| F TGEMV | 🔴 未开始 | 16–18、20–22 不存在 |
| G GMOV | ⛔ ISA 阻塞 | 等最终合同 |
| H Shared movement | 🔴 未完成 | 旧设计仍有效，需结合 scope/event |
| I event/convergence | 🔴 未开始 | 无完整 compiler-enforced contract |
| J full validation | 🟡 仅旧集合定向测试 | 新提交测试通过不等于当前 v3 全量通过 |

下一笔实现不应重复“让 base TMATMUL 多打一条 zero FPATR”；应直接从 A/B 或 C 的剩余项开始。

建议让不同 agent 按下列边界实现，避免一个巨型提交同时改 MC、寄存器模型、API 和 synchronization：

1. **A：冻结 active Function/MC legality**，先删除 FIXP opcode 可见性并建立 reserved 边界。
2. **B：移除 physical ACC，建立 ordinary Local D/C**；依赖 A。
3. **C：统一 Matrix PostProcess/FPATR operand schema**；依赖 B。
4. **D：迁移六个 TMATMUL API，删除六个 FIXP API**；依赖 C。
5. **E：修正 cooperative Shared-A/ScaleA binder**；依赖 B/C，可与 D 后半并行但写文件重叠大，最好串行。
6. **F：实现六个 TGEMV**；依赖 B/C，可在 E 完成后复用 helper。
7. **G：GMOV 合同对齐**；必须先取得精确 ISA 文档，可独立提交。
8. **H：Shared memory movement**；复用已有 Shared allocation，可独立于 Matrix 主线。
9. **I：event/scope/convergence**；是 cooperative/Shared movement 完整正确性的最终依赖。
10. **J：跨仓 clean build/end-to-end 回归**；所有功能提交后执行，不混入功能代码。

以下每个工作包都要求：LLVM 与 TileOP 分仓提交；不要提交本 handoff；不要触碰
`test/tileop_api/src/MultiThreadAdd.cpp`；不要顺手实现 boxed register alias。

### 6. 工作包 A：Matrix Function 表与 MC legality

#### 设计目标

让 assembler/disassembler/InstPrinter 只承认 12 个 active Matrix Function，并把 8、9–14 固定为非法兼容边界。
此包只处理 opcode/profile 可见性，不同时重构所有 CodeGen API。

#### LLVM 修改点

- `llvm/lib/Target/LinxV5/LinxV5BaseInfo.h`：删除六个 `TMATMUL*_FIXP` Function 枚举；加入/确认 TGEMV
  16–18、20–22；8、9–14 不应映射到 mnemonic。
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5InstPrinter.cpp`：删除 `.FIXP` printer case；增加 TGEMV canonical
  mnemonic case。
- `llvm/lib/Target/LinxV5/LinxV5InstrInfo.td`：删除独立 FIXP `BSTART.CUBE` profile/伪指令入口；更新
  “FPATR only for FIXP”注释。
- `llvm/lib/Target/LinxV5/AsmParser/LinxV5AsmParser.cpp`、Disassembler：确认 Function 8、9–14 解码时报
  reserved/unknown，而不是回落到旧 mnemonic。
- 清理 `llvm/test/MC/LinxV5/v5-shared-cube-encoding.s` 中 FIXP 正例，拆成 active function 正例与 reserved
  function 负例。

#### 测试设计

- 正例：0–2、4–6、16–18、20–22 均能 assemble/object/disassemble round-trip。
- 负例：文本 `.FIXP` mnemonic 被拒绝；手工 `.word`/`.byte` 构造 Function 8、9–14 时 objdump 不得打印旧
  operation 名。
- 保证 Function 编号没有被压缩或重排；尤其不得复用 8–14。

#### 完成标准

- `rg 'TMATMUL.*FIXP' llvm/lib/Target/LinxV5` 不再命中 active 定义/printer。
- 12 个 active Function round-trip，7 个 reserved Function 不映射到 public operation。
- 此提交允许旧 CodeGen 测例暂时失败，但提交说明必须列出后续 B/C/D 依赖；若仓库要求每提交绿，则 A 与 B
  合并，但不要与 TileOP API 迁移混为一个仓库提交。

### 7. 工作包 B：移除 architectural ACC，统一 Local D/C

#### 设计目标

把后端从“Matrix 写隐式 ACC，再导出/特殊读取”迁移到“所有 Matrix 显式定义普通 Local D；ACC variant 显式
读取普通 Local C”。逻辑 `TileAcc` 类型可以保留，但不得对应专用物理寄存器类。

#### LLVM 修改点

- `llvm/lib/Target/LinxV5/LinxV5RegisterInfo.td`：移除或彻底停用 `ACC_DST`、`ACC_SRC`、`Tile_ACC`、
  `ACC_TILE_*`、`Tile_ACC1` 等专用类/寄存器。
- `llvm/lib/Target/LinxV5/LinxV5InstrInfo.td`：Matrix pseudo 的 output 全改为普通
  `TILE_DstWithArrow`；`*_ACC` 的 C 改为普通 `TILE_Src_Reg`，并进入正常 `B.IOT` source 顺序。
- `llvm/lib/Target/LinxV5/LinxV5ExpandPseudoInsts.cpp`：删除根据 `Tile_ACC1` 决定 B.IOT/operand 数的分支；按
  operation schema 显式发 D/C。
- `llvm/lib/Target/LinxV5/LinxV5ISelLowering.cpp` 及 intrinsic lowering：TileAcc IR/前端值映射到普通 Tile
  register class。
- `ACCCVT` Function 8 保持无 pseudo/无 lowering；删除“Matrix 结果必须 ACCCVT 才可成为 Tile”的残留。

#### API/SSA 合同

建议保留现有类型系统中的 `TileAcc<...>` 作为 AccType partial-sum role，以减少上层算法迁移，但底层
`data()`/constraint 与普通 Local Tile 一致。建议签名：

```cpp
TMATMUL(D &d, A &a, B &b, PostProcessConfig cfg, WaitEvents &...events);
TMATMUL_ACC(D &d, C &c, A &a, B &b, PostProcessConfig cfg, WaitEvents &...events);
```

`D` 与 `C` 类型可相同；若上层传同一 SSA 对象，lowering 必须形成 use-old C + def-new D。inline asm 应使用
early-clobber output（现有 `=&Tr` 可复用）并把 C 作为独立 input，不能依赖 tied destructive operand。

#### Assembly 目标

```asm
BSTART.CUBE TMATMUL.ACC, FP32
B.FPATR 0, 0, 0, 0, 0, 0, 0
B.IOT t#A, t#B
B.IOT t#C, last, ->t<4KB>
```

具体 B.IOT 分组可由 operand 数决定，但语义顺序必须是 A、B、C，destination 是普通 Local t-reg 且 size 非零。

#### 测试设计

- base/BIAS/ACC/MX 三类至少各有 Local D 的 CodeGen 检查。
- `D` 与 `C` 不同对象和同一逻辑对象两种 ACC case；检查生成 use/def 顺序，避免 C 被提前 clobber。
- `llvm-readobj`/objdump 不出现 `ACC#1` 或 `->ACC`。
- MIR verifier/regalloc 测例确认 TileAcc 值走普通 Local Tile class。
- Function 8/ACCCVT 仍非法。

#### 完成标准

- `rg 'ACC_DST|ACC_SRC|Tile_ACC|ACC_TILE|Tile_ACC1' llvm/lib/Target/LinxV5` 无 active 使用。
- 所有 Matrix destination canonical 打印为普通 Local destination suffix，并带非零 size。
- 不引入隐式全局 accumulator 状态。

### 8. 工作包 C：统一 PostProcessConfig 与 `B.FPATR`

> **状态更新：🟡 过渡基础已由 LLVM `36a8c5103097`、TileOP `e6b6bc8` 完成。** 以下设计仍是最终
> 目标，但实现时应在现有公共 FPATR insertion 和 overload 上演进，不要回滚后重写。

#### 设计目标

把现有 `fixp::Options` 的成熟字段编码和 operand expansion 迁移为 12 个 active operation 共用的
`PostProcessConfig`，而不是简单把 `B.FPATR 0...0` 硬编码到每个 helper。

#### 建议 API 结构

为减少破坏，可先以现有 `fixp::Options` 为实现内核，公开重命名/别名为：

```cpp
namespace postprocess {
struct Config { /* compile-time fields + typed Tile/GPR operands */ };
constexpr Config none();
// pre_quant(...), quant(...), relu/prelu(...), row_max(...), group_max(...)
}
```

公开 Matrix API 的最后一个非 event 参数统一为 `const Config &cfg = postprocess::none()`；若当前编译器无法稳定处理
默认 NTTP/复杂 constexpr，可以提供无 cfg overload 转发到 `none()`，但所有 lowering 最终都必须走同一条 schema。

不要保留“base API 不发 FPATR、FIXP API 才发”的双轨。旧 `fixp::Options` 可短期作为 deprecated alias，但旧
`TMATMUL_FIXP` 函数名必须由工作包 D 删除。

#### Operand schema

Config 编译期决定：

- FPATR 七字段 immediate 的编码。
- Quant/ReLU 参数来自 Tile 还是 GPR。
- source 扩展顺序；先基础 Matrix sources，再 PostProcess Tile sources，再 PostProcess GPRs。
- destination 顺序：D、RowMaxOut、GroupMaxOut。
- D/aux dtype、shape、layout 与 allocation size。

必须复用现有 FIXP helper 中对 scalar quant、vector quant、PReLU、RowMax、GroupMax、keep-acc 的编码经验，但应
把宏/模板名字从 `PTO_FIXP_*` 泛化为 `PTO_MATRIX_PP_*`，避免 active base opcode 继续依赖 FIXP 命名。

#### LLVM 修改点

- `llvm/lib/Target/LinxV5/LinxV5InstrInfo.td`：为 12 个 active pseudo 建立共享 FPATR immediate + optional
  Tile/GPR source + optional aux dst schema。
- `LinxV5ExpandPseudoInsts.cpp`：对每个 active Matrix block无条件生成一条 FPATR；根据 schema 精确生成 B.IOT/
  B.IOR，不能多发空 operand。
- parser/bundle verifier：拒绝 0 条或多于 1 条 FPATR；拒绝非 Matrix block 使用 Matrix FPATR profile。
- `B.IOT` 多 destination 支持若现有 encoding 通过多条 header 表达，必须保证每个 destination 非零 TSize，最后一条
  标记 `last`。

#### 语义检查

- canonical None 保持 AccType；任一 convert/quant/ReLU 推导普通 ND D dtype。
- RowMaxIn/Out 为 `M x 1`；GroupMaxOut 为 `M x ceil(N/GroupN)`。
- RowMax/GroupMax 观察 pre-activation/pre-quant P。
- 不允许 config 声明 Tile 参数但缺 operand，或携带 config 未消费的 operand。
- D 与 auxiliary outputs 同一结果组；异常、flush 或 replay 不得只提交其中一部分。

#### 测试设计

- 12 个 active Function 各一条 canonical None，检查恰好一个 FPATR。
- 六个 TMATMUL 中至少覆盖 scalar quant、Tile quant、PReLU、RowMax、GroupMax 和组合 case。
- TGEMV 至少覆盖 None 与一种非零 config。
- 负例：缺 FPATR、重复 FPATR、错误 source arity、错误 aux shape、零 destination size、未消费 operand。
- object/objdump round-trip 确认七字段 encoding 与旧 FIXP equivalent case 一致（opcode Function 除外）。

#### 完成标准

- LLVM 不再有 `isFIXPOpcode => emit FPATR` 之类 predicate。
- 同一 Config 在 TMATMUL/TGEMV 对应 profile 上产生一致 FPATR/operand contract。
- 旧 FIXP options 的所有已支持模式都能由 active operation + Config 表达。

### 9. 工作包 D：TileOP 六个 TMATMUL API 迁移

> **状态更新：🟡 options overload 机制已完成，但 API 仍按旧 FIXP 分族。** 待办重点已从“给 base API 增加
> 参数”变成“移除 FIXP public family，并把其全部参数化 PostProcess 能力并入六个 active API”。

#### 设计目标

删除六个独立 FIXP public operation，把它们的功能合并到 Function 0–2、4–6 的六个 TMATMUL API；同步更新
docs/tests，消除旧 implicit ACC 文案。

#### 目标签名

名称应与当前仓库命名风格保持一致；概念签名如下：

```cpp
TMATMUL(D &d, A &a, B &b, const postprocess::Config &cfg = none());
TMATMUL_BIAS(D &d, A &a, B &b, Bias &bias, const Config &cfg = none());
TMATMUL_ACC(D &d, C &c, A &a, B &b, const Config &cfg = none());
TMATMUL_MX(D &d, A &a, ScaleA &sa, B &b, ScaleB &sb, const Config &cfg = none());
TMATMUL_MX_BIAS(D &d, A &a, ScaleA &sa, B &b, ScaleB &sb, Bias &bias,
               const Config &cfg = none());
TMATMUL_MX_ACC(D &d, C &c, A &a, ScaleA &sa, B &b, ScaleB &sb,
              const Config &cfg = none());
```

若需 WaitEvents，固定放在 Config 之后并遵循上游 PTO 最终声明；不要让可变参数吞掉 Config。`AccPhase` 只接受
default；nondefault 必须 `static_assert`/constraint failure。

#### TileOP 文件

- `include/jcore/template_asm.hpp`：合并当前 `matmul_*` 与 `matmul_*_fixp` helper；删除
  `TMATMUL*_FIXP` public wrappers 和 `.FIXP` opcode 文本。
- `include/common/pto_tileop.hpp`、`tileop_api.hpp`、`tileop_api_impl.hpp`：同步声明、forwarding、concept。
- `docs/tileop-usage/tmatmul-fixp.md`：删除或重写为 PostProcess 章节；更新 `cube.md`、alignment status。
- `test/tileop_api/src/SharedMatmul.cpp`、DynamicShape 和新增 MatrixPostProcess 测例：迁移调用。

#### 兼容策略

推荐直接删除旧 public `TMATMUL*_FIXP`，让源码在编译期暴露迁移点；不要继续生成 reserved Function 9–14。
如业务必须有短期 source compatibility，只能提供 `[[deprecated]]` forwarding wrapper，且必须生成 active opcode
0–2/4–6，绝不能生成旧 Function；此兼容 wrapper 应在单独提交并有删除期限。

#### 测试与完成标准

- 六个 API 的 none/nonzero Config compile tests。
- 所有旧 FIXP 测例迁移后，反汇编只出现 active mnemonic + FPATR。
- `rg 'TMATMUL.*FIXP|TMATMUL_MX.*FIXP' include docs test` 无 public active API/文档。
- Linux/macOS clean compile 均不依赖 operand 数量偶然匹配。

### 10. 工作包 E：Cooperative TMATMUL Shared-A/ScaleA binder

#### 设计目标

修正当前 inline asm helper 中“Shared data + Local scale”与无角色 `C.B.IOS` 的旧展开，严格实现
Left/ScaleLeft/Right/ScaleRight 固定 binder 顺序。

#### 实现策略

- 为 Shared operand constraint 增加 role-aware formatter/asm template，而不是根据出现顺序猜角色。
- non-MX：Local A/Shared B 发 `Right`；Shared A/Shared B 发 `Left, Right`。
- MX：Local A pair/Shared B pair 发 `Right, ScaleRight`；双 Shared pair 发
  `Left, ScaleLeft, Right, ScaleRight`。
- Shared binder 对应 source 不得重复出现在 B.IOT；Bias/C/PostProcess local sources 保持在 Local stream。
- 编译期 traits 验证 data/scale pair 同域、Shared A + Local B 非法、Shared IDs 不同、Shared distribution 是
  `MShard4`。
- 沿用现有 `Sr` constraint 和 Shared absolute register allocation，不新增用户可见固定 `S#` 编号接口。

#### Assembly 检查示例

```asm
# MX Shared A pair + Shared B pair
C.B.IOS sA, Left
C.B.IOS sSA, ScaleLeft
C.B.IOS sB, Right
C.B.IOS sSB, ScaleRight
B.IOT ..., last, ->t<Size>
```

实际 mnemonic/role token 拼写必须取自最新 `C.B.IOS` parser 定义；如果当前 parser 尚无 role 字段，先在 MC 层
实现 role encoding/printer，再改 TileOP asm，不能仅靠注释表示角色。

#### 测试设计

- 六个 TMATMUL operation 至少各有 Local/Local；每类至少覆盖 Local/Shared-B 和 Shared-A/Shared-B。
- MX 四 binder exact-order FileCheck。
- 负例：Shared-A/Local-B、Local A + Shared ScaleA、Shared B + Local ScaleB、重复 Shared ID、错误
  distribution、非收敛路径（后者最终由工作包 I verifier 覆盖）。
- 检查 Shared source 不重复进入 B.IOT。

#### 完成标准

- MX Shared pair 不再把 ScaleA/ScaleB 作为 Local `Tr` input。
- binder role 顺序可从 assembly/object 明确恢复，不依赖 API 参数位置猜测。

### 11. 工作包 F：六个 PE-local TGEMV

#### 设计目标

新增 Function 16–18、20–22 的端到端 LLVM/TileOP 支持，最大限度复用 TMATMUL 的 Local operand、FPATR、
aux output 和 D/C schema，但禁止任何 cooperative/Shared 路径。

#### API 设计

签名与对应 TMATMUL 同构：

```cpp
TGEMV(D &d, A &a, B &b, const Config &cfg = none());
TGEMV_BIAS(D &d, A &a, B &b, Bias &bias, const Config &cfg = none());
TGEMV_ACC(D &d, C &c, A &a, B &b, const Config &cfg = none());
TGEMV_MX(D &d, A &a, ScaleA &sa, B &b, ScaleB &sb, const Config &cfg = none());
TGEMV_MX_BIAS(..., Bias &bias, const Config &cfg = none());
TGEMV_MX_ACC(D &d, C &c, A &a, ScaleA &sa, B &b, ScaleB &sb,
             const Config &cfg = none());
```

所有 Tile concept 必须要求 Local storage；shape concept 要求/推导 `M == 1`。动态 M 若无法编译期证明为 1，应在
lowering 生成运行时 guard 或直接拒绝；首版建议只接受静态 M=1，避免 silent miscompile。

#### LLVM 修改点

- BaseInfo/InstPrinter/TableGen 增加六个 active Function/mnemonic。
- 复用 Matrix common pseudo schema，但增加 TGEMV profile flag：禁止 C.B.IOS、要求 M=1。
- lowering/expander 生成 ordinary Local D、mandatory FPATR、Local source B.IOT 和必要 B.IOR。
- parser/verifier 在 TGEMV block 出现 Shared binder 时诊断；M 非 1 时诊断。

#### 测试设计

- 六个 operation 的 MC encoding/disassembly 与 CodeGen。
- base/BIAS/ACC 的数学 source 顺序；MX scale 顺序。
- canonical None 与至少一种 quant/convert、RowMax/GroupMax case。
- 负例：M=0/2、任一 Shared operand、任一 C.B.IOS、nondefault AccPhase、错误 C/D shape。
- K-block chain：`TGEMV` 首块 + 一到多个 `TGEMV_ACC(D,C,...)`，确认 PostProcess 只在最终 block 使用；若 API
  不支持“中间不 PostProcess”，必须通过 Config/算法合同明确首/中间块的 canonical accumulate 类型。

#### 完成标准

- TGEMV 无任何 Shared/cooperative template 分支。
- 16–18、20–22 round-trip 且每个 block 恰好一个 FPATR。
- `M=1` 在 API 和 backend 至少一层强制验证，最好两层均验证。

### 12. 工作包 G：GMOV 最新合同对齐

#### 当前阻塞

变更摘要只写“收紧/统一 GMOV collective 与 Tile operand 表述”，但当前本地最新 ISA 目录没有可读取的
`GMOV.md` 最终页，无法安全推导 distribution、participant、source/destination role、mask、size 和 convergence
的精确 encoding。

#### Agent 开始前必须取得

- 最终 `GMOV.md` 或等价 sidecar/Excel 行。
- Local→Shared、Shared→Local、Shared→Shared 是否都 active。
- source/destination distribution（MShard4/Replicated4）与 participant mask 合法组合。
- issuer/非 issuer 的 operand 定义、fully-defined 要求和 rendezvous 规则。
- `B.IOT`/`C.B.IOS` 的确切 role、destination size 和 `last` 规则。

#### 取得合同后的实现框架

- 更新 MC opcode/profile/parser/printer/verifier。
- 复用 Shared absolute allocation 与 `Sr` constraint。
- API 使用 storage/distribution traits 静态筛选，不向用户暴露物理 S 编号。
- collective form 必须接入工作包 I 的 convergence/event 模型。
- 正例覆盖每个 active direction/distribution；负例覆盖 mask、participant、混合 storage、零 size、重复 Shared
  ID、发散控制流。

在精确合同到位前，**不要修改现有 GMOV encoding**；只可整理测试基线或增加 TODO/诊断骨架。

### 13. 工作包 H：Shared memory movement

#### 目标范围

完成 Shared TLOAD、Shared TSTORE 和 `TSTORE<pe_scope>`/SPART 的公开 API、inline-asm/lowering、合法性和
端到端测试。现有 handoff 的“Shared TLOAD 直接 inline asm MVP”可作为实现手段，但语义必须服从本节 storage/
participant/convergence 合同。

#### API 建议

```cpp
TLOAD(SharedTile<T, Shape, Dist> &dst, GlobalTensor src, Shape valid,
      core_scope_t, WaitEvents &...events);
TSTORE(GlobalTensor dst, const SharedTile<T, Shape, Dist> &src,
       core_scope_t, WaitEvents &...events);
TSTORE(GlobalTensor dst, const LocalTile<T, Shape> &src,
       pe_scope_t, WaitEvents &...events); // 对应 SPART，仅在 ISA 明确时
```

最终参数顺序应与现有 TileOP TLOAD/TSTORE 风格一致；关键是 scope 必须进入类型/overload resolution，不能只靠
运行时枚举选择完全不同的 collective 语义。

#### 实现设计

- Shared destination/source 使用现有 compiler-managed SSA handle 和 `Sr` constraint。
- destination 有 allocation 时必须发非零 size；无 Local destination 的 source-only form才可能使用 metadata
  推导 size。
- 直接 Shared TLOAD 若 ISA 支持，应直接生成 TLSU + Shared binder/destination，避免先 Local TLOAD 再
  `TMOV_L2S_PUBLISH` 的额外 Local allocation、带宽和 lifetime。
- 若硬件只允许单 issuer 发 memory request，API/lowering 仍要表示四 PE participant/rendezvous，不能让四 PE
  各自重复 load/store。
- memory operation 的 RecordEvent/WaitEvents、dependency header 与 visibility 必须由工作包 I 明确定义；
  `memory` clobber 不能替代硬件 event/barrier。

#### 测试设计

- Shared TLOAD 后直接喂 cooperative TMATMUL Shared A/B。
- Shared TSTORE/Local SPART 的 address、stride、valid shape、dtype、size、mask round-trip。
- 负例：错误 scope、错误 distribution、零 size、非 fully-defined Shared source、发散到达、重复 issuer、缺
  wait dependency。
- Linux/macOS clean compile，检查 inline asm constraint 在两平台一致。

#### 完成标准

- 不再要求用户通过 Local temporary + TMOV 才能完成 ISA 原生支持的 Shared movement。
- API 与汇编明确区分 PE-local 和 Core4 collective，不靠注释或调用约定暗示。

### 14. 工作包 I：RecordEvent、WaitEvents、scope 与 convergence

#### 设计目标

为 cooperative TMATMUL、GMOV、Shared movement 建立统一的同步合同；不能只靠 `asm volatile`、`memory`
clobber 或 PE_MASK=1111 假设四 PE 已收敛。

#### 前端/API

- 定义 `RecordEvent` 返回对象及 `WaitEvents...` 参数传递；事件必须是 SSA dependency，不是无语义 token。
- `pe_scope` 仅允许 PE-local operation；`core_scope`/Core4 只允许 ISA 声明的 collective。
- operation traits 标注 participant count、issuer policy、是否要求 identical scalar operands、是否要求
  fully-defined Shared input。
- dynamic control flow 中不能证明四 PE 同序到达的 collective 应在编译期拒绝或进入明确的 convergent IR
  representation。

#### LLVM/IR/CodeGen

- builtin/intrinsic 或 inline-asm wrapper 必须带 convergent/sideeffect/appropriate memory effects，防止 hoist、
  duplicate、merge 或跨 barrier 重排。
- 将 WaitEvents 降为现有 `B.IOD`/dependency header 或 ISA 指定 token；RecordEvent 映射到 destination dependency。
- verifier 检查 Core4 operation 的 PE_MASK、相同动态顺序、相同 shape/dtype/config/GPR、Shared fully-defined。
- 如果仅靠后端无法证明 convergence，应要求上层结构化 region/marker，而不是静默接受。

#### 测试设计

- 正例：直线控制流、相同循环 trip count、相同 event chain 的 cooperative sequence。
- 负例：仅部分 PE 分支调用、不同循环次数、不同 collective 顺序、不同 FPATR GPR、缺失 participant、未等待
  producer event。
- 优化回归：O0/O2 下 collective 不被删除、复制、交换顺序或移出控制区域。
- 多次相邻 collective 保持动态顺序，event use-def 可从 MIR/asm 验证。

#### 完成标准

- “四 PE 必须收敛”成为 compiler-enforced contract，而非文档提醒。
- Shared producer→consumer 可由 event/dependency 明确追踪；`memory` clobber 仅承担编译器内存重排语义。

### 15. 工作包 J：验证、提交与回滚边界

#### 定向验证顺序

1. `llvm-tblgen`/目标库 build。
2. Matrix MC positive/negative + object/disassembly round-trip。
3. Matrix CodeGen/MIR regalloc tests。
4. TileOP compile tests：Local、Shared、PostProcess、TGEMV、movement、events。
5. Linux clean toolchain rebuild。
6. macOS clean rebuild，重点检查 inline asm operand 数、constraint、fixup 和 bundle parser。
7. SuperNPUBench Local/Shared matmul 与新增 TGEMV/movement 端到端。
8. 完整 `check-llvm`/项目回归；FA/boxed alias 仅在独立复现后另开工作包。

#### 建议测试文件

- LLVM MC：`v5-matrix-active-functions.s`、`v5-matrix-reserved-functions.s`、
  `v5-matrix-fpattr-legality.s`、`v5-tgemv.s`、`v5-shared-a-binders.s`。
- LLVM CodeGen：替换 `v5-matmul-fixp.ll` 为 `v5-matrix-postprocess.ll`；扩充
  `v5-matmul-local-tile-result.ll`；新增 `v5-tgemv.ll`、`v5-matrix-no-physical-acc.mir`。
- TileOP：`MatrixPostProcess.cpp`、`TGEMV.cpp`、`SharedMatrixBinders.cpp`、`SharedMovement.cpp`、
  `CollectiveEvents.cpp`。

#### 提交边界

- LLVM 与 TileOP 各自按工作包提交，提交信息明确 ISA Function/语义变化。
- 不提交 `CODEX_HANDOFF.md`、patch 文件、`tmp/`、`relax.s.o`、`.claude/`、`.gitlab/`。
- 不修改/删除 TileOP untracked `test/tileop_api/src/MultiThreadAdd.cpp`。
- 不把 boxed alias、FA unrelated fix、SIMT spill 或其他本地 patch 混入。
- 每个 agent 完成后在其回复中列出 commit、修改文件、测试命令、失败的 unrelated test 和下一工作包依赖；不要
  把这些运行记录写回并提交 handoff。

### 16. 给下一批 agent 的最短任务分派模板

可直接复制以下任务描述：

```text
阅读 CODEX_HANDOFF.md 的“2026-08-05 最新 DavinciOO ISA 对齐总交接”，只实现工作包 X。
以 DavinciOO_intrinsic_changes_since_3b4fe5e.md 为最高 ISA 增量；不得采用旧 implicit ACC/FIXP opcode。
不要修改或提交 CODEX_HANDOFF.md，不要触碰 MultiThreadAdd.cpp，不要混入其他 untracked patch。
先报告现状与计划，再实现、跑定向测试，最后给出修改文件、测试结果和建议 commit 信息。
```

优先分派顺序：A+B → C → D → E → F；G 等 ISA owner 给出 GMOV 精确合同；H 可并行；I 在 E/H 基础上
收口；J 最终集成。

## 2026-08-05 旧实现快照（历史记录；Matrix 计划已被上节覆盖）

> 本节覆盖后文所有 2026-08-03 的“WIP/尚未提交”描述。后文保留为历史分析记录；若与本节冲突，以本节为准。

### 远端与工作区状态

| 仓库/分支 | 当前远端 HEAD | 同步状态 | 本地剩余 |
|---|---|---|---|
| `LinxISA/llvm-project:dev-llvm15_56` | `42bf7bd32bc8` | ✅ 本地 HEAD 与远端 `0/0` | tracked 仅 `CODEX_HANDOFF.md`；永远禁止提交/推送 |
| `LinxISA/Linx-TileOP-API:linx` | `94af6a6` | ✅ 本地 HEAD 与远端 `0/0` | 仅 untracked `test/tileop_api/src/MultiThreadAdd.cpp`，不要提交 |

最新提交链：

- LLVM `42bf7bd32bc8 [LinxV5] Complete Shared inline asm handling`
- LLVM 前置 `0509c2ca0c55 [LinxV5] Restore B.IOT tile size suffix syntax`
- TileOP `94af6a6 [tileop-api] Restore B.IOT tile size suffix syntax`
- TileOP 前置动态 shape `6f72422 [tileop-api] Support dynamic valid shape across TileOPs`

### B.IOT `<size>` 最终规则

- encoding **没有变化**；只修改汇编 parser、canonical printer 和 TileOP inline-asm 文本。
- 有 destination 的 B.IOT canonical 格式：

  ```asm
  B.IOT mask=1111, last, ->t<16KB>
  ```

  不再 canonical 打印 `TSize=6, ->t`；旧输入仍可兼容汇编，并打印成 destination suffix。
- 无 destination 且需要 size 的形式继续使用 `TSize=N`，这是历史既有设计，不属于遗漏。例如：

  ```asm
  BSTART.TLSU TMOV.L2S.PUBLISH, FP32
  C.B.IOS S#0
  B.IOT t#1, mask=1111, TSize=6, last
  ```

- TileOP 最新 `template_asm.hpp` 共转换 152 条“有 dst 的 `TSize=`”；两条无 dst 的 `TSize=` 保持不变。
- boxed register alias (`tile_t1` 等) 与 `<size>` 无关，已从 LLVM `<size>` commit 中移除；远端当前不包含该 alias 功能。

### 最新验证结果

- LLVM 定向测试全部通过：

  ```bash
  build/bin/llvm-lit -v \
    llvm/test/MC/LinxV5/v5-b-iot-tilesize-syntax.s \
    llvm/test/MC/LinxV5/v5-b-iot-non-last-dst.s \
    llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll \
    llvm/test/CodeGen/LinxV5/v5-matmul-local-tile-result.ll
  ```

- 新旧 B.IOT size 输入生成完全相同的机器编码。
- 完整工具链已基于 LLVM `0509c2ca0c55` 重建并安装到：

  ```text
  /home/zhuwei/linx-BLK-build/output/linx_blockisa_llvm_musl
  ```

- TileOP `DynamicShape`、`SharedMatmul` 编译通过。
- SuperNPUBench 最新 `origin/main` multi-thread matmul 用以下命令编译通过并成功生成 ELF：

  ```bash
  make -C benchmark/one-level-arch/test/kernel/multi_thread/matmul \
    TESTCASE=matmul \
    COMPILER_DIR=/home/zhuwei/linx-BLK-build/output/linx_blockisa_llvm_musl/bin \
    B=1 M=256 N=256 K=256 \
    tM=32 tN=32 tK=32
  ```

  关键反汇编：

  ```asm
  BSTART.TLSU TMOV.L2S.PUBLISH, FP32
  C.B.IOS S#0
  B.IOT t#1, mask=1111, TSize=6, last
  BSTART.CUBE TMATMUL, FP32
  C.B.IOS S#0
  B.IOT u#1, mask=1111
  B.IOT mask=1111, last, ->t<16KB>
  ```

- TileOP 旧测例 `MatMul`/`MatMacc` 使用已废弃的 `MATMUL`/`MATMACC`；`MatMul_e4m3` 使用 SIMT、`TileAcc` 等旧接口，仍失败，但与 `<size>` 修改无关，不要为本提交顺带修复。

### 历史待办（仅供追溯；不要按本节分派新任务）

> 本节记录的是 `<size>`/旧 FIXP 方案当时的状态。当前未实现项请只看文档顶部的“当前实现状态矩阵”和工作包 A–J。

1. 完整 FA/two-level 回归尚未完成。boxed alias 的实验实现已从 `<size>` 提交排除；若 FA 确实需要，应单独设计、测试和提交。
2. Shared A + Local B 仍因单个 `C.B.IOS` 无法表达 operand 角色而被 API 编译期拒绝；现支持 Local/Local、Local/Shared-B、Shared-A/Shared-B。
3. **下一项明确需求：`TLOAD(SharedTile, GlobalTensor)`，即 GM→Shared full load。** 设计和实现计划见下方“2026-08-05 新需求”。Shared TSTORE、`TSTORE<pe_scope>` SPART、TGEMV Function 16–22、RecordEvent/WaitEvents 和 scope/convergence 仍待实现。
4. **新增矩阵需求：所有 12 个 `TMATMUL*` family 都增加 FPATR/options 参数，并且每个 bundle 恰好生成一条 `B.FPATR`。** 以现有 `TMATMUL_FIXP` 端到端链路为参考，但必须抽公共层，详细设计见“所有 TMATMUL family 接收 FPATR 参数”。
5. TileOP 本地 `test/tileop_api/src/MultiThreadAdd.cpp` 是旧 SIMT 调试文件，不属于正式修改；保留但不要提交。
6. `CODEX_HANDOFF.md` 永远仅本地保存，禁止加入任何 commit 或推送。

## 2026-08-05 新需求：TLOAD 支持加载到 SharedTile

### 当前执行决策（下一个 agent 直接按此实现）

**本轮只实现 TileOP inline-asm MVP，不实现 LLVM intrinsic、SelectionDAG、Machine pseudo、
issuer verifier 或 convergence pass。** 目标是最快让以下代码在 Linux/macOS 工具链中编译、生成
目标文件并正确反汇编：

```cpp
SharedTile<TileRight<float, 32, 32>> shared_b;
TLOAD(shared_b, global_b);
TMATMUL(dst, local_a, shared_b);
```

本轮修改范围优先限制在 `Linx-TileOP-API`：

1. 在 `include/jcore/template_asm.hpp` 增加 SharedTile TLOAD overload。
2. 如公开 include 路径确有需要，在 `include/common/tileop_api.hpp` 增加同签名转发；先确认
   `__linx` 当前是否已直接暴露 `template_asm.hpp::TLOAD`，不要制造重复/二义性 overload。
3. 新增 `test/tileop_api/src/SharedTLoad.cpp`，至少覆盖 Shared load 后作为 Shared-B 做 matmul。
4. 只有 inline asm 在 asm/object 阶段实际失败时，才修改 `linx-llvm`；不得预先增加 intrinsic/pseudo。
5. 不实现 Shared TSTORE、SPART、CPU simulator、exactly-one verifier，也不顺带修复旧 TileOP 测例。

建议直接实现的 JCore wrapper 形态如下，允许根据仓库真实 concept/include 关系做最小调整：

```cpp
template <is_shared_tile_v shared_shape, is_global_data_v gm_shape>
PTO_SHARED_INLINE void TLOAD(shared_shape &dst, gm_shape &src) {
  using local_shape = typename shared_shape::LocalTileType;

  static_assert(
      std::is_same_v<typename shared_shape::DType, typename gm_shape::DType>,
      "Shared TLOAD requires matching GM and Shared dtypes");
  static_assert(
      tile_type_traits<typename local_shape::TileDType>::IsValidActiveSize,
      "Shared TLOAD logical Tile size must be 512 B..32 KB");

  const size_t valid_col = dst.GetValidCol();
  const size_t valid_row = dst.GetValidRow();

  asm volatile(
      "BSTART.TLSU TLOAD, %c[SrcType]\n"
      "C.B.IOS %S[Shared]\n"
      "B.DIM %[VCOL], 0, ->lb0\n"
      "B.DIM %[VROW], 0, ->lb1\n"
      "B.DIM zero, %c[COL], ->lb2\n"
      "B.IOT mask=15, TSize=%c[TileSize], last\n"
      "B.IOR [%[Src],%[GMStride]], []\n"
      : [Shared] "=Sr"(dst.handle_ref())
      : [Src] "r"(src.data()),
        [SrcType] "i"(type_traits<typename gm_shape::DType>::TypeCode),
        [TileSize] "i"(
            tile_type_traits<typename local_shape::TileDType>::TilesizeCode),
        [VCOL] "r"(valid_col), [VROW] "r"(valid_row),
        [COL] "i"(local_shape::Cols),
        [GMStride] "r"(
            gm_shape::RowStride * sizeof(typename gm_shape::DType))
      : "memory");
}
```

实现时必须保留以下关键点：

- Shared output 使用 `"=Sr"(dst.handle_ref())`，绝不能使用 `"=Tr"` 或 `dst.data()`。
- `C.B.IOS %S[Shared]` 负责绑定 Shared destination。
- B.IOT 必须是 **无 destination** 的 `TSize=N` 形式：

  ```asm
  B.IOT mask=1111, TSize=6, last
  ```

  不得生成 `->t<16KB>`，因为那会错误地产生 Local destination。
- 保留 `"memory"` clobber，避免 GM read 被错误重排。
- 第一版只要求 static valid shape 正确。当前 dynamic `SharedTile` 默认 valid row/col 为 0，若测试确认
  dynamic 需要 setter/构造参数，再以最小改动增加 metadata 初始化；不要阻塞 static MVP。
- exactly-one issuer 本轮作为调用方前置条件记录，不做编译器诊断。正确使用示例：

  ```cpp
  if (get_thread_idx() == 0)
    TLOAD(shared_b, global_b);
  ```

下一个 agent 的完成标准：

1. `SharedTLoad.cpp` 使用当前安装工具链编译通过。
2. `-S` 输出包含 `BSTART.TLSU TLOAD`、`C.B.IOS S#0` 和 no-dst `B.IOT ... TSize=N`。
3. `-c` 生成目标文件成功，不出现 `fixup_linxv5_invalid` 或 Shared operand Match Error。
4. `llvm-objdump` 显示同一直接 Shared TLOAD 序列，且没有额外
   `TMOV.L2S.PUBLISH`、Local 临时 TLOAD destination。
5. 既有 `DynamicShape`、`SharedMatmul` 和 Linux Shared LLVM 定向测试不回归。
6. TileOP commit 独立提交；不提交 `MultiThreadAdd.cpp`，不提交或推送 `CODEX_HANDOFF.md`。

后文的 builtin/intrinsic、issuer verifier 和 CPU simulator 内容是**未来增强规划**，不属于当前 agent
的实现范围，除非 inline-asm MVP 被实际后端缺口阻塞。

### 需求合同

新增公开类型重载，使现有 `TLOAD` 可以直接完成 GM→Shared full load；接口名不增加
`TLOAD_SHARED` 等后缀：

```cpp
using LocalB = TileRight<float, 32, 32>;
using SharedB = SharedTile<LocalB>;
using GlobalB = global_tensor<float, RowMajor<32, 32>>;

void load_shared(SharedB &dst, GlobalB &src) {
  TLOAD(dst, src);
}
```

该重载的语义是：

- 从 Global Memory 直接产生一个新的 SharedTile full version，不经过程序员可见的 Local Tile。
- 新版本的 `defined_mask=1111`，对同一 Core 的 4 个 PE 可见。
- 只能由 **exactly-one PE** 发起；首版至少接受编译器可静态证明的
  `get_thread_idx() == constant`/等价单分支形态。0 个或多个 issuer 都是非法程序。
- Shared ID/version 继续由编译器通过 `Shared_ABS` 分配；C++ 调用方不能指定 `S#n`。
- `dst` 保留被包装 LocalTile 的 dtype、shape、layout、role 和 dynamic valid shape；完成 load 后更新
  `GetValidRow()/GetValidCol()` 对应的元数据。
- 逻辑 Shared tile size 必须是 v5 可编码的 512 B..32 KB；dtype 必须与 GM 元素类型一致。
- 本需求只覆盖 full GM→Shared load；Shared→GM full store 与
  `TSTORE<pe_scope>`/`TSTORE.SPART` 分开实现。

### 为什么不能只用 `TLOAD(Local)+TMOV_L2S_PUBLISH`

可以先用组合序列作为功能对照或 bring-up fallback，但不能作为最终公开语义：

```cpp
LocalB tmp;
TLOAD(tmp, src);
dst = TMOV_L2S_PUBLISH(tmp);
```

原因：

1. 它额外占用一个 Local tile register，并产生两条 TLSU 操作，不是 GM→Shared 的直接指令合同。
2. `PUBLISH` 受单 PE Local payload 上限约束，不能自然覆盖完整 32 KB Shared load。
3. exactly-one issuer、Shared full definition、版本生命周期和 readiness 属于 GM→Shared TLOAD
   本身的语义，不能靠两个普通 API 调用可靠表达。
4. 单纯把这两个现有 API 拼接起来，优化器无法把它识别为一次 GM→Shared full definition，也无法
   对 issuer 和 Shared version 生命周期做专门检查。

因此 canonical codegen 必须直接生成 Shared TLOAD 汇编序列；组合序列仅允许用于早期结果对照，
不得作为提交后的输出。

### TileOP API 设计

涉及仓库：`/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API`。

1. 在 `include/common/tileop_api.hpp` 增加 Shared overload：

   ```cpp
   template <is_shared_tile_v shared_shape, is_global_data_v gm_shape>
   PTO_SHARED_INLINE void TLOAD(shared_shape &dst, gm_shape &src);
   ```

   保持参数顺序 `TLOAD(dst, src)`，与 Local TLOAD 一致。首版采用输出参数，不新增返回值版本，
   以对齐网站合同和后续 `TSTORE(gm, shared)` 对称接口。

2. 在 JCore 路径增加 `TLOAD_SHARED_Impl`/低层 wrapper；不要调用 `dst.data()`，因为
   `SharedTile` 故意没有 Local payload。第一阶段直接沿用当前 `template_asm.hpp` 的手写
   block-asm 风格，输出使用 `dst.handle_ref()` 和 `"=Sr"` constraint。现有后端已支持
   `BSTART.TLSU TLOAD`、`C.B.IOS`、no-dst `B.IOT TSize=N` 和 `B.IOR`，原则上不需要先新增
   LLVM intrinsic 才能完成可用实现。

3. Shared overload 必须 `always_inline`，避免 opaque Shared handle 经普通 C++ ABI/GPR/stack
   传递。沿用 `PTO_SHARED_INLINE`。

4. 编译期静态约束：
   - `shared_shape::LocalTileType` 与 `gm_shape::DType` dtype 一致；
   - shape/layout 复用 Local TLOAD 的 GM stride 和 layout 约束；
   - logical tile size 为 512 B..32 KB；
   - dynamic valid row/col 传入 intrinsic，并写回 `SharedTile` 元数据；
   - 禁止 Local Tile、SharedTile 嵌套和 Shared output alias 普通 `Tr` operand。

5. CPU simulator 先实现语义模型，而不是伪造 `Sr`：给 `SharedTile` 的 cpu-sim 特化增加受控 payload
   或 simulator-side shared object，验证 full load、defined mask 和后续 broadcast/matmul；JCore
   `SharedTile` 仍保持 handle-only。

### LLVM/CodeGen 设计

涉及仓库：`/home/zhuwei/linx-llvm`。

分为两个阶段，先交付直接编码，再补完整静态语义证明。

#### 阶段 A：直接 Shared TLOAD inline-asm（首个可用提交）

1. TileOP wrapper 直接生成：

   ```asm
   BSTART.TLSU TLOAD, FP32
   C.B.IOS S#0
   B.DIM <valid-col>, 0, ->lb0
   B.DIM <valid-row>, 0, ->lb1
   B.DIM zero, <cols>, ->lb2
   B.IOT mask=1111, TSize=<size>, last
   B.IOR [<gm-base>,<gm-stride>], []
   ```

   Shared destination 由 `C.B.IOS` 绑定，因此 B.IOT **无 Local destination**；需要 size 时继续使用
   no-dst `TSize=N`，不要错误改成 `->t<size>`。B.DIM/B.IOR 顺序与现有 Local TLOAD 保持一致。

2. `"=Sr"` output 让现有 `Shared_ABS` allocator 分配 `S#0..S#255`；复用
   `42bf7bd32bc8` 的 `%S` printer、SharedTID parser/printer/encoder 修复。

3. 为 assembler/object/disassembler 增加 MC 回归，确认上述直接序列在 Linux 和 macOS 都能从
   inline asm 生成目标文件。若当前 generic instruction expansion 已能编码，不改
   `LinxV5TileOpExpand`；只有出现实际编码缺口时才增加专用 MC pseudo。

4. 阶段 A 可以验证功能和编码，但 inline asm 对中端仍然是不透明 side effect，无法完整证明
   exactly-one issuer。因此阶段 A 完成后只能标记“编码/API 可用”，不能标记“4-PE 语义完全合规”。

#### 阶段 B：issuer/convergence 完整实现

为了落实规范要求的 exactly-one 编译器证明，再增加可识别的 compiler representation；优先考虑
专用 builtin/intrinsic，而不是尝试从任意 inline-asm 字符串反推语义：

1. **LLVM intrinsic**：在 `llvm/include/llvm/IR/IntrinsicsLinx.td` 新增
   `llvm.linx.v5.shared.tload`。建议返回 `i64` Shared handle，并携带：dtype、layout/shape 参数、
   GM pointer/stride、valid row/col；必须声明 memory read + side effect，不能标成 `IntrNoMem`。

2. **SelectionDAG node**：新增 `LinxV5ISD::V5_SHARED_TLOAD`，保留 chain，确保 GM load 不可被删除、
   越过同步或与 Shared version definition 错误重排。

3. **Machine pseudo**：新增 `PseudoV5SharedTLOAD`：
   - output：`SharedRegOp:$SharedDst`；
   - inputs：TLSU TLOAD function、dtype、GM base/stride、B.DIM 参数、TSize；
   - `hasSideEffects=1`、`mayLoad=1`；
   - Shared output 进入现有 `LinxV5SharedRegAlloc`，分配到 `S#0..S#255`。

4. **Emitter/TileOpExpand**：让 pseudo 在 `LinxV5MCCodeEmitter`/统一 tile expansion 中展开为
   `BSTART.TLSU + C.B.IOS + B.DIM + B.IOT(NoSrc/NoDst/Size) + B.IOR`。不要先生成
   Local TLOAD 再生成 `TMOV.L2S.PUBLISH`。

5. **issuer verifier**：单独实现 exactly-one issuer 检查，不把它伪装成固定 `PE_MASK`：
   - 首版识别由 `get_thread_idx()`/`get_thread_id()` 与常量比较形成的单 PE region；
   - 接受常量 0..3 中任意一个 owner；
   - 无法证明 exactly-one 时编译期报明确诊断；
   - verifier 放在控制流已规范化、但 Shared pseudo 尚未展开的位置；
   - 后续再扩展等价 switch、dominator/post-dominator 和 convergence 证明。

6. builtin/intrinsic 路径最终生成与阶段 A 完全相同的 canonical assembly；完成后可保留阶段 A wrapper
   作为 builtin 封装，但不能同时维护两套不同编码。

### 建议实施顺序

1. 冻结 API 签名和汇编 canonical 序列。
2. 增加 TileOP JCore inline-asm overload，编译最小 `TLOAD(shared, gm)` C++ 测例。
3. 验证 asm/object/objdump，必要时仅补 MC parser/emitter 缺口。
4. 增加 cpu-sim 语义模型和 Shared matmul 端到端功能测试。
5. 新增 builtin/intrinsic、SelectionDAG node、pseudo 和 Shared register def-use。
6. 实现 exactly-one issuer verifier；在 verifier 完成前不得宣称多线程语义完全合规。
7. 用 builtin/intrinsic 替代 opaque inline-asm 语义入口，但保持相同 canonical assembly。
8. 更新网站对齐文档、TileOP usage 文档和本 handoff 状态。

### 文件级实现清单

**linx-llvm 阶段 A（按实际缺口修改，可能只需测试）**：

- `llvm/test/CodeGen/LinxV5/v5-shared-tload-inline-asm.ll`
- `llvm/test/MC/LinxV5/v5-shared-tload-encoding.s`
- 若实际 object 编码失败，再修改 `LinxV5AsmParser.cpp`、`LinxV5InstrInfo.td`、
  `LinxV5MCCodeEmitter.cpp`/`LinxV5TileOpExpand.cpp`

**linx-llvm 阶段 B（预计）**：

- `llvm/include/llvm/IR/IntrinsicsLinx.td`
- `llvm/lib/Target/LinxV5/LinxV5ISelLowering.h`
- `llvm/lib/Target/LinxV5/LinxV5ISelLowering.cpp`
- `llvm/lib/Target/LinxV5/LinxV5ISelDAGToDAG.cpp`
- `llvm/lib/Target/LinxV5/LinxV5InstrInfo.td`
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.cpp` 和/或
  `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5MCCodeEmitter.cpp`
- issuer verifier 对应的新 pass/现有控制流 pass
- `llvm/test/CodeGen/LinxV5/v5-shared-tload.ll`

**Linx-TileOP-API（预计）**：

- `include/common/tileop_api.hpp`
- `include/common/pto_tile.hpp`（仅 dynamic metadata/cpu-sim 特化需要时）
- `include/jcore/template_asm.hpp` 或独立 `include/jcore/TLoadShared.hpp`
- `include/cpu_sim/TLoadBackend.hpp`/Shared simulator storage
- `test/tileop_api/src/SharedTLoad.cpp`
- `docs/tileop-usage/constraints.md`
- `docs/tileop-usage/davincioo-alignment-status.md`

### 验证矩阵与完成标准

**正向测试**：

- static RowMajor、ColMajor、TileLeft/TileRight compatible layout；
- dynamic valid row、dynamic valid col、二者同时 dynamic；
- 512 B、1/2/4/8/16/32 KB size 边界；
- FP32、FP16/BF16 和至少一种整数 dtype；
- owner PE0 与非零 owner（如 PE2）；
- `TLOAD(shared_b, gm_b)` 后直接作为 `TMATMUL` Shared operand；
- 两个连续 Shared TLOAD 分配不同 `S#n`，生命周期结束后可回收；
- asm、object 和 objdump 三条路径一致。

**负向测试**：

- 无条件由 4 个 PE 同时执行；
- 无法证明 exactly-one 的动态条件；
- dtype、shape/layout、size 不合法；
- 32 KB 以上；
- Shared handle 跨普通函数 ABI、落栈或被当作 GPR；
- Shared TLOAD 结果错误传给只接受 Local Tile 的算子。

**完成标准**：

1. C++ `TLOAD(SharedTile&, GlobalTensor&)` 编译通过，并且不产生 Local 临时 tile。
2. 反汇编出现单个 `BSTART.TLSU TLOAD`、单个 `C.B.IOS S#n`、无 destination 的
   `B.IOT ... TSize=N` 和正确 `B.IOR`。
3. Linux 定向 LLVM/TileOP 测试全部通过；macOS 干净重建后同一最小测例和完整多线程测例通过。
4. 阶段 A 至少明确记录 exactly-one 是调用方前置条件；阶段 B 完成后，合法与非法 issuer 控制流
   均有回归测试和明确诊断。
5. 不回归 Local TLOAD、TMOV L2S/S2L、Shared Matmul、B.IOT `<size>` 和 Shared inline-asm。

## 历史：所有 TMATMUL family 接收 FPATR 参数（已被最新 ISA 合同废弃）

> 旧方案错误地把 12 个接口理解为“六个 TMATMUL + 六个独立 FIXP 变体”，并要求保留 `TMATMUL*_FIXP`。
> 最新 ISA 已删除 Function 9–14；请改看上方工作包 C/D/F。

### 需求结论

将当前 `TMATMUL_FIXP` 使用的 FPATR/options 参数扩展到其余所有矩阵乘 TileOP。所有
`TMATMUL*` 公开接口都增加一个位于参数列表最后的 options 参数，并且每个 CUBE bundle 都必须在
`B.DATR` 之后、`B.DIM` 之前生成 **恰好一条** `B.FPATR`：

```asm
BSTART.CUBE <TMATMUL function>, AType
B.DATR BType, byte0, Null
B.FPATR PreQuant, Relu, GroupNCode, RowMaxEn, GroupMaxEn, RowMaxInit, MaxAbsEn
B.DIM M, 0, ->lb0
B.DIM N, 0, ->lb1
B.DIM K, 0, ->lb2
...
```

本需求会改变当前 LLVM/TableGen 注释中的旧合同：

```text
B.FPATR required exactly once by TMATMUL*.FIXP
```

新合同应改为：

```text
B.FPATR required exactly once by every supported TMATMUL/TMATMULMX family
```

因此不能只改 TileOP inline asm；LLVM assembler、canonical printer、pseudo expansion 和 verifier 中
任何“只有 FIXP opcode 才有 FPATR”的判断也必须同步泛化。

### 覆盖的 12 个公开接口

统一覆盖以下 family，参数始终放在最后：

| 基础 | ACC | BIAS |
| --- | --- | --- |
| `TMATMUL(..., options)` | `TMATMUL_ACC(..., options)` | `TMATMUL_BIAS(..., options)` |
| `TMATMUL_FIXP(..., options)` | `TMATMUL_ACC_FIXP(..., options)` | `TMATMUL_BIAS_FIXP(..., options)` |
| `TMATMUL_MX(..., options)` | `TMATMUL_MX_ACC(..., options)` | `TMATMUL_MX_BIAS(..., options)` |
| `TMATMUL_MX_FIXP(..., options)` | `TMATMUL_MX_ACC_FIXP(..., options)` | `TMATMUL_MX_BIAS_FIXP(..., options)` |

示例：

```cpp
TMATMUL(dst, a, b, fixp::keep_acc());
TMATMUL_ACC(dst, acc, a, b, fixp::keep_acc().relu());
TMATMUL_BIAS(dst, a, b, bias, fixp::f16());

TMATMUL_MX(dst, a, scale_a, b, scale_b, fixp::keep_acc());
TMATMUL_MX_ACC(dst, acc, a, scale_a, b, scale_b,
               fixp::keep_acc());
TMATMUL_MX_BIAS(dst, a, scale_a, b, scale_b, bias,
                fixp::bf16());
```

现有 FIXP 六个接口已经有 options 参数，保持参数顺序和语义；非 FIXP 六个接口新增相同形态的最后
参数。需求明确要求调用方显式传入参数，因此第一版不建议保留无 options 的兼容 overload；默认行为
由显式 `fixp::keep_acc()` 表达。若必须做源码兼容，只能提供标记 deprecated 的短期转发，并确保它
同样生成全零 `B.FPATR`，不能继续省略该指令。

### 是否可以参考 TMATMUL_FIXP 端到端逻辑

**可以，而且应当以现有 `TMATMUL_FIXP` 为唯一参考实现；但要抽公共基础设施，不能把整套宏和
dispatch 复制 12 份。**

可以直接复用的部分：

1. `FixpAttr` 七字段定义和 `encoding()` 对应关系。
2. `fixp::Options<Attr,...>` 的 compile-time `Options::Attr` 提取方式。
3. `is_valid_fixp_attr(Attr)`、PreQuant/ReLU/GroupN/RowMax/MaxAbs 合法性检查。
4. `PTO_FIXP_ATTR` 汇编文本和 `PTO_FIXP_ATTR_INPUTS` 七个 `"i"` immediate operands。
5. `B.FPATR` 固定插入位置：`B.DATR` 后、首条 `B.DIM` 前。
6. Local/Shared A/B binder、MX scale、BIAS、ACC dependency 和 B.IOT destination 的既有处理。
7. asm/object/objdump 三阶段回归方式，以及 Linux/macOS 完整工具链验证方式。

不能机械复制的部分：

1. `TMATMUL_FIXP` 的 quant Tile/GPR、PReLU Tile/GPR、RowMaxIn、RowMaxOut、GroupMaxOut
   operand dispatch 是由 FIXP mode 决定的额外数据流；不能仅因为所有 opcode 都有 `B.FPATR`，就
   自动假定每个非 FIXP opcode 都支持所有附加 operand。
2. FIXP opcode 的 destination dtype 由 `PreQuantMode` 推导；普通 TMATMUL/ACC/BIAS/MX 的原有
   output/ACC 语义必须保留，不能套用 `is_fixp_output_type` 后无意改变结果类型。
3. FIXP 完成后 implicit ACC invalid、RowMax/GroupMax atomic commit 等规则不能自动外推到普通
   opcode，除非更新后的 ISA 合同明确要求。
4. `isFixpMatmulPseudo()` 目前同时承担“识别 FIXP opcode”和“决定插入默认 B.FPATR”的职责；
   泛化时应拆分为两个 predicate，例如：
   - `isMatmulFamilyPseudo()`：所有 12 个 family，决定必须存在 FPATR；
   - `isFixpResultPseudo()`：仅六个 `.FIXP` opcode，保留 FIXP result/ACC 特有语义。

### 第一阶段语义边界

为了在不误扩展 ISA operand contract 的前提下完成“所有 matmul 都多一个 FPATR 参数”，建议分两层：

1. **所有 12 个接口都接受同一个 `fixp::Options` 类型，并总是生成其 `Options::Attr` 对应的
   `B.FPATR`。**
2. **FIXP 六个接口**继续支持完整 options：scalar/vector quant、LReLU/PReLU、RowMax、GroupMax、
   MaxAbs 及其附加 inputs/outputs。
3. **非 FIXP 六个接口**第一阶段只开放“不需要附加 Tile/GPR operand”的 attribute subset：
   - `PreQuantMode::None`；
   - parameter-free convert mode 是否允许，按更新后的 ISA 确认；
   - `ReluMode::None/Relu` 是否允许，按更新后的 ISA 确认；
   - `RowMaxEn=0`、`GroupMaxEn=0`、`RowMaxInit=0`、`MaxAbsEn=0`；
   - 禁止 scalar/vector quant、LReLU/PReLU、RowMax/GroupMax options，给出 compile-time
     `static_assert`，而不是生成缺 operand 的错误 bundle。

如果新 ISA 已明确普通 opcode 也拥有完整 FIXPIPE 输入/输出合同，再第二阶段把现有
`PTO_FIXP_DISPATCH` 泛化为 opcode 参数化的公共 emitter；在获得该确认前，不要猜测附加 B.IOT/B.IOR
角色。

### TileOP API 设计

建议保留 `fixp` namespace 和现有 options builder，避免再造第二个内容相同的 `FPAttr` 类型：

```cpp
template <typename Options>
constexpr void validate_matmul_fpatr_options() {
  constexpr FixpAttr Attr = Options::Attr;
  static_assert(is_valid_fixp_attr(Attr), "invalid B.FPATR configuration");
}

template <typename Options>
constexpr void validate_non_fixp_matmul_options() {
  validate_matmul_fpatr_options<Options>();
  static_assert(/* no auxiliary operands and allowed attribute subset */,
                "this TMATMUL opcode does not support parameterized FIXPIPE operands");
}
```

公共 matmul header 改为始终接收 FPATR 文本：

```cpp
#define PTO_MATMUL_HEADER(OPCODE) \
  "BSTART.CUBE " OPCODE ", %c[DataTypeA]\n" \
  "B.DATR %c[DataTypeB], byte0, Null\n" \
  PTO_FIXP_ATTR \
  "B.DIM %[M], 0, ->lb0\n" \
  "B.DIM %[N], 0, ->lb1\n" \
  "B.DIM %[K], 0, ->lb2\n"
```

每个 asm operand list 统一追加 `PTO_FIXP_ATTR_INPUTS`。不要继续使用当前
`PTO_MATMUL_HEADER(OPCODE, EXTRA_ATTRS)` 中传空字符串或硬编码
`"B.FPATR 0, 0, 0, 0, 0, 0, 0"` 的方式；否则普通、FIXP、MX、Shared 变体会继续分叉。

建议在 `pto_matmul_detail` 抽三层：

1. `emit_matmul_header<Attr>(Opcode, AType, BType, M, N, K)` 的公共宏/inline-asm片段；
2. matrix operand/binder 层：Local/Shared A/B、MX scales、BIAS、ACC；
3. FIXPIPE auxiliary operand 层：仅在 options 需要 quant/ReLU/max inputs/outputs 时 dispatch。

### LLVM/MC 设计

当前 TileOP 主要使用 inline asm，因此首要任务是让所有新 bundle 能经过 assembler/object 路径；
同时 LLVM 自身的 pseudo expansion 也必须保持同一合同。

1. `LinxV5InstrInfo.td`：
   - 更新 `B_FPATR` 注释，不再写“Required exactly once by TMATMUL*.FIXP”；
   - 指令 encoding 本身不变；
   - 如存在 bundle verifier，允许/要求所有 matrix functions 后接一条 FPATR。

2. `LinxV5MCCodeEmitter.cpp`/`LinxV5TileOpExpand.cpp`：
   - 新增或改造 `isMatmulFamilyPseudo()`，覆盖 12 个 pseudo；
   - 从 pseudo operands 读取七个 FPATR 字段并生成 `B_FPATR`；
   - 删除“仅 FIXP 自动插入全零 FPATR”的硬编码；
   - 保留 `isFixpResultPseudo()` 处理 FIXP-only result/ACC 行为。

3. pseudo/TableGen operands：所有 12 个 matrix pseudo 都增加七个 FPATR immediate operand，或增加
   一个能稳定展开为七字段的 operand group。推荐七个显式 immediate，便于 MIR/FileCheck 和 encoder
   读取；不要把 C++ struct encoding 当单一 opaque `i32` 后再在多个层重复拆位。

4. SelectionDAG/intrinsic：如果普通 matmul intrinsic 仍由手写 LLVM IR/旧 builtin 使用，所有对应
   intrinsic/lowering 也需追加 FPATR 参数，默认调用方显式传七字段。TileOP inline asm 可以先完成，
   但 LLVM CodeGen tests 不能长期保留“不带 FPATR 参数却由 emitter 猜全零”的双重合同。

5. canonical 顺序固定：

   ```text
   BSTART.CUBE → B.DATR → B.FPATR → C.B.IOS(optional) → B.DIM/B.IOT/B.IOR
   ```

   如果现有 canonical 顺序要求 `C.B.IOS` 位于 B.DIM 后或 B.IOT 前，以 backend 当前 bundle parser
   的实际合法顺序为准；唯一不可变要求是 `B.FPATR` 紧随 matrix attributes，且每 bundle 恰好一次。

### 文件级实现清单

**Linx-TileOP-API**：

- `include/common/pto_tile.hpp`：复用/补充 options traits 和 non-FIXP subset verifier。
- `include/jcore/template_asm.hpp`：统一 header、12 个 API 参数、FPATR inputs、公共 dispatch。
- `include/common/tileop_api.hpp`：若这里仍暴露旧 MATMUL/MATMACC wrapper，同步签名。
- `include/jcore/MatMul.hpp`、`include/jcore/MatMacc.hpp`：旧 API wrapper 若仍参与 JCore build，
  同步 options 参数或明确迁移到 template-asm implementation。
- `include/cpu_sim/MatMul.hpp`、`include/cpu_sim/MatMacc.hpp`：至少接受相同签名；支持的 Attr 做语义模拟，
  暂不支持的 mode 给出明确 static_assert。
- `test/tileop_api/src/MatmulFPAttr.cpp`：覆盖 12 个接口的 compile/assembly contract。
- `docs/tileop-usage/cube.md`、`docs/tileop-usage/tmatmul-fixp.md`、constraints/alignment 文档。

**linx-llvm**：

- `llvm/lib/Target/LinxV5/LinxV5InstrInfo.td`
- `llvm/lib/Target/LinxV5/LinxV5ISelLowering.h/.cpp`
- `llvm/lib/Target/LinxV5/LinxV5ISelDAGToDAG.cpp`
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileOpExpand.cpp`
- `llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5MCCodeEmitter.cpp`
- 必要时更新 Linx intrinsic/builtin 定义和 clang 侧签名
- `llvm/test/MC/LinxV5/v5-matmul-fpatr.s`
- `llvm/test/CodeGen/LinxV5/v5-matmul-fpatr.ll`
- 更新既有 `v5-matmul-fixp.ll`、`v5-matmul-local-tile-result.ll`、Shared matmul tests

### 推荐实施顺序

1. 先新增 MC 测试，确认 `B.FPATR` 可跟随 12 个 BSTART.CUBE function 编码/反汇编；若硬件/decoder
   不接受非 FIXP function，立即停止并确认 ISA，而不是只改 API。
2. 抽取 TileOP 公共 FPATR header/inputs，不改变现有 FIXP 行为。
3. 给非 FIXP 六个 API 增加 required options 参数，并限制为首阶段安全 subset。
4. 更新所有 FIXP/非 FIXP、Local/Shared、MX/BIAS/ACC wrapper，确保每 bundle 恰好一条 FPATR。
5. 泛化 LLVM pseudo FPATR operands 和 expansion，拆分 family predicate 与 FIXP-result predicate。
6. 更新 CPU sim 和文档。
7. 完成 Linux/macOS clean rebuild 与端到端 SuperNPUBench 回归。

### 验证矩阵与完成标准

**必须覆盖**：

- 12 个公开接口，每个至少一个 Local/Local case；
- Local/Shared-B 和 Shared-A/Shared-B 的代表 case；
- MX、BIAS、ACC operand 顺序不变；
- `fixp::keep_acc()` 生成全零 FPATR；
- 至少一个非零、无附加 operand 的合法 Attr；
- FIXP scalar quant、vector quant、PReLU、RowMax、GroupMax 既有高级 options 不回归；
- 每个 bundle 正好一条 `B.FPATR`，位置正确；
- asm、object、objdump canonical 输出一致；
- 旧无 options 调用给出预期迁移诊断，或仅通过 deprecated 转发生成全零 FPATR；
- 非 FIXP 传入需要 quant/PReLU/max 附加 operand 的 options 时得到明确 compile-time 错误，直到 ISA
  明确支持为止。

**完成判据**：

1. 12 个 TileOP 的最终公开签名都有最后一个 options 参数。
2. 12 种 CUBE function 都生成且只生成一条 `B.FPATR`。
3. 现有 `TMATMUL_FIXP` 全 options 端到端功能和编码保持不变。
4. LLVM 不再用“FIXP opcode 才插 FPATR”的单一 predicate。
5. Linux 和 macOS 完整 matmul 测例通过，且没有 operand Match Error、invalid fixup 或 bundle 顺序错误。
6. `CODEX_HANDOFF.md` 继续仅本地保存，不纳入任何 commit/push。

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
git push linxisa HEAD:dev-llvm15_56      # linx-llvm；不要推到同名本地分支
git push origin linx                     # tileop-api
```

### 已完成的全部工作

#### 最新已推送基线（2026-08-04）

| 仓库/分支 | 远端 HEAD | 状态 |
|---|---|---|
| `LinxISA/llvm-project:dev-llvm15_56` | `0509c2ca0c55` | ✅ 已推送 |
| `LinxISA/Linx-TileOP-API:linx` | `94af6a6` | ✅ 已推送 |

最新完成项：

- ✅ LLVM Shared 全局绝对寄存器类、`Sr` inline-asm constraint 和专用绝对索引分配 pass：`0b09bb4911c9`。
- ✅ TileOP `SharedTile` 与 `TMOV_L2S_PUBLISH`：`9c277bb`。
- ✅ 所有公开 Matmul API 支持 Local/Local、Local/Shared-B、Shared-A/Shared-B，包括基础、ACC、BIAS、FIXP、MX 及其组合：`f817fdc`。
- ✅ 完整 `TMATMUL_FIXP` FPATR options API 与使用文档：`4ef8a11`。
- ✅ superscalar 前端遇到 `__vec__`、`__mtc__` 或 `<<<>>>` 时用英文诊断拒绝 SIMT；TileOP superscalar include 路径不再编入 SIMT 实现：LLVM `c38775c`、`4364c94`，TileOP `2d4bed5`。
- ✅ GitHub Issue #35：Shared inline asm 的 `%S` operand 不再被通用 printer 吞掉，能够稳定输出并编码 `C.B.IOS S#n`：`abc0233`。
- ✅ Issue #35 端到端隔离验证：`SharedMatmul.cpp` 成功生成 ELF；2 条 `TMOV.L2S.PUBLISH`、13 条 CUBE Matmul、28 条 `C.B.IOS S#n`。
- ✅ 全量 Linx one-level TileOP 动态 valid shape：所有 public inline-asm 的 `ValidRow/ValidCol` 均通过 GPR `B.DIM`，物理 Rows/Cols 与 TileSize 保持编译期常量；SharedTile 同步保存运行时 shape。TileOP `6f72422`。
- ✅ B.IOT destination size suffix 已完成并同步推送：LLVM `0509c2ca0c55`、TileOP `94af6a6`；encoding 不变，有 dst 使用 `->t<16KB>`，无 dst size 继续使用 `TSize=N`。
- ✅ 基于上述提交重建完整工具链，并通过 SuperNPUBench multi-thread matmul 端到端编译与反汇编验证。

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

旧的 `ACC_TILE_SRC`/Acc 寄存器描述已废弃。当前 Matmul 的 destination、ACC input、Bias input 都使用普通 Tile；例如 `TMATMUL_ACC(dst, acc, a, b)` 中 `dst` 和 `acc` 都是普通 Tile，`acc` 作为普通 `B.IOT` source 编码。

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
| fa_2d_unroll_gmma.cpp | Tm=16 Tk=64, 修 TMATMUL 顺序+删 ACCCVT | 🟡 完整 FA 尚未复测；boxed alias 未进入当前本地或远端 LLVM |

编译通过的测例文件在 `/home/zhuwei/docs/test_cases/`。

原 FA 分析曾尝试通过 `tile_tN`/`tile_uN`/`tile_mN`/`tile_nN` parser alias 解决输入寄存器名问题，但该实验与 B.IOT `<size>` 无关，已从工作区和提交中移除。当前本地及远端均不包含 boxed alias；必须先用最新工具链完整复现 FA，再决定是否需要独立实现。

### 已知遗留问题

1. **完整 FA 回归未完成**：boxed register alias 未进入远端；`fa_2d_unroll_gmma.cpp` 仍需在明确需求后单独复测和实现。
2. **Shared A + Local B 未支持**：当前单个 `C.B.IOS` 无法区分“Shared A”与既有“Shared B”角色；API 有意在编译期拒绝。现支持 Local/Local、Local/Shared-B、Shared-A/Shared-B。
3. **Shared movement/TGEMV/synchronization 尚未完成**：详见工作包 D/E/F。

### 当前未提交边界（不要混入已推送功能）

- LLVM tracked：仅 `CODEX_HANDOFF.md`，本地专用，禁止提交或推送。
- LLVM 其他 untracked `.patch`、`tmp/`、`relax.s.o` 等均为本地调试产物，不属于当前正式修改。
- TileOP tracked：无；本地与 `origin/linx` 同步。
- TileOP untracked：`test/tileop_api/src/MultiThreadAdd.cpp`，旧 SIMT 调试内容，不要提交。

### 仍缺失（按工作包）

- ✅ **工作包 B / FIXP options**：已完成统一 `fixp::Options`/builder 风格参数，覆盖 PreQuant、Quant、PReLU/ReLU、RowMax、GroupMax、keep-acc 等 FPATR operand；TileOP commit `4ef8a11`。仍需按未来 ISA 文档补充尚未定义的新模式，而不是重新设计 descriptor。
- 🟡 **工作包 C / SharedTile + Matmul**：Shared 全局绝对寄存器类、`Sr` constraint、专用分配 pass、`TMOV_L2S_PUBLISH` 及全部 TMATMUL/FIXP/MX 接口已完成。LLVM commit `0b09bb4`；TileOP commits `9c277bb`、`f817fdc`。剩余仅 TGEMV Shared 支持，以及 ISA 若要求 Shared-A/Local-B 时新增可区分角色的编码/operand。
- ❌ **工作包 D / Shared memory movement**：Shared TLOAD/TSTORE 与 `TSTORE<pe_scope>` SPART 尚未形成完整 API、lowering 和端到端测试；当前主要通过 `TMOV_L2S_*` 产生 SharedTile。
- ❌ **工作包 E / TGEMV**：Function 16–22 及 Local/Shared operand 组合仍待实现。
- ❌ **工作包 F / synchronization**：RecordEvent、WaitEvents、`pe_scope`、`core_scope` 和 Core4 convergence 仍待实现。
- ✅ **工作包 G / B.IOT `<size>` 汇编格式**：encoding 不变；LLVM `0509c2ca0c55` 与 TileOP `94af6a6` 已同步提交并推送，工具链和 SuperNPUBench matmul 端到端验证通过。
- ❌ **工作包 H / boxed register alias**：确认与 `<size>` 无关，已从该提交移除；远端未实现。仅在完整 FA 复现证明必要后，作为独立功能重新设计和提交。
- ✅ **工作包 I / 全量动态 valid shape**：已完成 Linx one-level public TileOP 的 `RowValid/ColValid=-1` 支持，214 个静态 DIM operand 迁移为运行时 GPR，Local/Shared Matmul 均保留动态 M/N/K；TileOP `6f72422` 已推送。

### 关键设计约束（必须保留）

- v5 Matmul 不再有 Acc register class；所有 Matmul 输出均为普通 Tile，`TMATMUL_ACC` 的累加输入也是普通 Tile source。
- FIXP 基础调用可生成全零 `B.FPATR`；带 `fixp::Options` 的调用必须按开发者指定字段生成非零 FPATR，不得强制清零。
- PE_MASK 初始化为全 1（`0b1111`）
- 当前所有 Matmul/Shared/FIXP 快速打通路径允许并实际使用 TileOP inline asm；后续若迁移 builtin/intrinsic，必须保持接口和编码兼容。
- SharedTile 是编译器管理的 SSA 值，后端分配绝对 `S#0..S#255`；用户不得手写或固定具体 Shared 编号。
- Matmul Shared operand 当前支持 Local/Local、Local/Shared-B、Shared-A/Shared-B；Shared-A/Local-B 必须编译期拒绝，除非 ISA 新增角色区分。
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

## 历史附录（已被 2026-08-04 权威快照覆盖）

> 以下记录用于追溯探索过程，包含当时的 WIP、错误判断和后来已移除的 boxed alias 实验。不得据此判断当前工作区或远端状态；当前状态只看文档开头的“2026-08-05 最新 DavinciOO ISA 对齐总交接”。

## 2026-08-02 更新:B.IOT `<size>` parser 真相 + 当时状态

### 关键发现(用户提示后确认)

用户记得"之前 b.iot 实现就是 `<8KB>` 写法"——属实:**HEAD(`5a1be738`)的 B.IOT 就是方括号 + bracket 语法**,AsmString `"B.IOT\t[]${Group}$DstTile$TileSize"`(`[]` src + GroupOp + dst + `<size>` 紧邻),operand 用 `TileSizeWithBracket`(printTileSizeWithBracket 打 `<8KB>`)。`b.iot.s` 测试就是这个语法(`B.IOT [], last ->t<512B>`)。

**但 `b.iot.s` 从项目最初 commit `f25aa63` 就一直 FAIL**(从未 PASS)。根因是 `->t<512B>` 的 `<` 在 AsmParser 报 `unknown token in expression`——`parseTileSizeWithBracket` 的 custom parser 没被 generated matcher 在 `<` 处调用,`<` 被当 `MCK__LT_` token(用于 `<<M:>>` BSTART 语法)。

### 我之前的 `B_IOT_TileSize_Op` + mask= 逗号语法是多余的

我之前(2026-08-02 早些)造的 `B_IOT_TileSize_Op` + `${PE_MASK}${Last}, $DstTile$TSize`(mask= 逗号语法)**与 HEAD 的方括号语法冲突**,已全部回退(InstrInfo.td/AsmParser/InstPrinter 恢复到 HEAD)。正确方向是**用 HEAD 的方括号 + bracket 语法**,只需修 parser 让 `<size>` 能解析。

### 当前干净状态(本轮保留的改动)

只有 2 个文件改动(其余在 HEAD):
- `LinxV5AsmPrinter.cpp`:PrintAsmOperand 加 `%Z` modifier(TSize imm 0..7 → `512B`/`8KB`/... 文本,**无尖括号**,inline-asm 写 `->%[Dst]<%Z[TileSize]>` 提供 `<>`)。已验证 IR 路由正确(`%Z[name]`→`${N:Z}`→PrintAsmOperand 'Z')。
- `LinxV5AsmParser.cpp`:`parseTileSizeWithBracket` + `parseGPRWithBracket` 的 4 处反向 `peekTok.compare(">")` bug 修复(吃 `>` 应检查 `getTok`(当前)非 `peekTok`(下一个),`!getTok().compare(">")` 时 Lex)。这是 `b.iot.s`/inline-asm `<size>` 解析的必要修复,但**不够**(`<` token 歧义仍在)。

### 仍卡的核心:`<` token 歧义(generated matcher)

`->t<512B>` 的 `<` 在 generated `LinxV5GenAsmMatcher.inc` 里被 lex 成 `MCK__LT_`(用于 `<<M:N:K:...>>` BSTART 语法),不走 `parseTileSizeWithBracket` 的 custom parser。trace 确认:parseTileSizeWithBracket **没被调用**(无 DBG 输出),`<` 直接报 unknown token。

这是 B.IOT `<size>` 语法的根本障碍,从项目最初就存在。需解决 `<` 在 dst 后触发 parseTileSizeWithBracket 而非 MCK__LT_——可能:
1. 调整 generated matcher 优先级(让 custom parser 先于 MCK__LT_ 尝试)—— TableGen 层面。
2. 或 `<` 在 `->` 后/特定上下文用 parseTileSizeWithBracket,其他用 MCK__LT_——需改 lexer/parser 调度。
3. 或 `b.iot.s` 改用无歧义语法(如 `->t<512B>` 改 `->t size=512B`?但用户要 `<8KB>`)。

### 回归
MC 7 既有失败不变(`b.iot.s`/vcall/mcall/CUBE/relax/MTC_*),1 XFAIL,8 PASS。CodeGen 3/3 PASS(v5-matmul-fixp/local-tile-result/shared-gmov)。无新回归。`v5-b-iot-tilesize-syntax.s`(我之前基于 mask= 语法加的)已删,不适用方括号语法。

### 下一步方向
1. 解决 `<` token 歧义(让 parseTileSizeWithBracket 在 dst 后被调)——这是 `b.iot.s` + inline-asm `<size>` 的共同关键。可能需 trace generated `MatchOperationImpl` 看 `<` 为何走 MCK__LT_ 而非 custom parser,或调整 AsmOperandClass 优先级。
2. 或确认 `<` 的 lexer token 类型,改 parseTileRegWithArrow 后主动调 parseTileSizeWithBracket(在 custom parser 内串联,不依赖 generated matcher 调度)。

## 2026-08-03:task #12 B.IOT TileSize 迁移完成(LLVM MC + inline-asm)

### 完成
B.IOT encoding 不变,文本从 `TSize=N` 改 destination suffix `<8KB>`。asm/disasm/inline-asm 全工作。

**LLVM MC 层**(`llvm/lib/Target/LinxV5/`):
- `LinxV5InstrInfo.td`:新增 `B_IOT_TileSizeAsmOperand` + `B_IOT_TileSize_Op`(3-bit,ParserMethod=parseTileSizeWithBracket,PrintMethod=printTileSizeWithBracket,EncoderMethod=getImmOpValueTSize)。`B_IOT_NoSrc/OneSrc/TwoSrc_Dst` 的 ins `TSize_Op`→`B_IOT_TileSize_Op`,AsmString `${PE_MASK}, ${TSize}${Last}, $DstTile`→`${PE_MASK}${Last}, $DstTile$TSize`(TSize 移 dst 后作 suffix)。`TSize_Op` 保持 printTSize 给非 B.IOT(NoDst_Size 等)。加 legacy-with-last InstAlias(`B.IOT $PE_MASK, $TSize, $Last, $DstTile`,TSize=N 兼容)。
- `AsmParser/LinxV5AsmParser.cpp`:加 `isB_IOT_TileSize()` 谓词(=isTileSizeWithBracket)。修 `parseTileSizeWithBracket`/`parseGPRWithBracket` 的 4 处反向 `peekTok.compare(">")` bug(吃 `>` 应检查 `getTok`(当前)非 `peekTok`(下一个),`!getTok().compare(">")` 时 Lex)。
- `LinxV5AsmPrinter.cpp`:PrintAsmOperand 加 `%Z` modifier(TSize imm 0..7 → `512B`/`1KB`/.../`32KB` 文本,**无尖括号**——inline-asm 写 `->%[Dst]<%Z[TileSize]>` 由 asm string 提供 `<>`)。IR 路由 `%Z[name]`→`${N:Z}`→PrintAsmOperand 'Z'。

**inline-asm 迁移**(`Linx-TileOP-API/include/jcore/template_asm.hpp`):146 处 `TSize=%c[N], last, ->%[Dst]` 转 `last, ->%[Dst]<%Z[TileSize]>`。当时暂留的 2 处 Shared L2S 无 dst `TSize=` 已在 2026-08-11 后续清理；Shared size 只由 `B.IOS` 承载。

### 关键 insight
- inline-asm dst(`"=Tr"`)展开成 `t`(**无 #N**,Tile_T/TILE_DST);src(`"Tr"`)在 boxed layout(TileLeft/Right)展开成 `t#N`(canonical,Tile_TOS_N/TILE_SRC)。用户澄清"相对索引 t#N 只在输入寄存器,dst 写 t"——属实,InstPrinter printTileDstReg 打 `->t`(无编号)。
- `%Z` modifier **不带尖括号**是关键:之前带括号导致 `<<512B>>` 双括号,改成只打 `512B` 文本,inline-asm 的 `<` `>` 提供括号。

### 验证(全 PASS,无新回归)
- 新增 `llvm/test/MC/LinxV5/v5-b-iot-tilesize-syntax.s`:7 size(512B..32KB)+ OneSrc/TwoSrc + legacy TSize=N 兼容,asm/disasm 一致。PASS。
- 更新 `v5-b-iot-non-last-dst.s`/`v5-matmul-local-tile-result.ll`/`v5-matmul-fixp.ll` CHECK 到新格式。PASS。
- `matmul.cpp`(SuperNPUBench)编译通过,disasm 全新格式 `->t<16KB>`/`->n<16KB>`,无 `TSize=`。
- MC:9 PASS + 7 既有失败(b.iot.s/vcall/mcall/CUBE/relax/MTC_*,从项目最初就失败,非本任务)+ 1 XFAIL。CodeGen 4/4 PASS。
- 当前验收 `rg 'TSize=%c\[TileSize\]' template_asm.hpp` 为零；无 dst `B.IOT` 不再携带 TSize。

### 注意
- `b.iot.s` 仍 FAIL(既有,从 f25aa63 开始)——它用方括号 `B.IOT [], last ->t<512B>` 语法,与当前 mask= 逗号语法不符;是历史遗留,待整体方括号语法迁移或删该测试。
- 带 src 的 inline-asm 在 boxed layout src 用 canonical `t#N`（matmul.cpp 验证通过）；裸 `float tile_size`（非 boxed）src 当时会展开成 parser 不认识的 `tile_t1`。该项已由后文 task #13 的本地 alias 修改解决，但尚未提交/推送。

## 2026-08-03:task #13 boxed-layout register parser 完成

### 完成
`MatchLinxV5TileRegisteName`(AsmParser)只认 `t#N`/`u#N`/`m#N`/`n#N`(MC alias),不认 boxed 名 `tile_t1`/`tile_u2`/`tile_m3`/`tile_n4` 等(`Tile_T_N`/`Tile_U_N`/... RA regs 的 asm 名)。inline-asm `"Tr"` 约束在某些场景(裸 `float tile_size`,非 boxed layout TileLeft/Right)src 展开成 `tile_t1`,导致 Match Error。

修复:在 `MatchLinxV5TileRegisteName` 加 64 个 boxed 名 alias(`tile_t1..tile_t16`/`tile_u1..tile_u16`/`tile_m1..tile_m16`/`tile_n1..tile_n16`),映射到对应 `Tile_TOS_N`/`Tile_UOS_N`/`Tile_MOS_N`/`Tile_NOS_N`(`Tile_T_N` 和 `Tile_TOS_N` 同 encoding,是同物理 reg 的 RA/MC 别名)。

### 验证(全 PASS,无回归)
- 新增 `llvm/test/MC/LinxV5/v5-tile-boxed-name-alias.s`:`tile_t1, tile_u2` 等输入,round-trip 输出 canonical `t#1, u#2`。PASS。
- 带 boxed src 的 inline-asm(裸 `float tile_size`)编译通过(之前 Match Error)。
- MC:10 PASS(新增 boxed-name-alias + 之前的 tilesize-syntax/non-last-dst)+ 7 既有失败(b.iot.s/vcall/mcall/CUBE/relax/MTC_*,从项目最初失败)+ 1 XFAIL。CodeGen 4/4 PASS。
- boxed layout(TileLeft/Right)src 在 matmul.cpp 展开成 canonical `t#N`(不触发 boxed 名),裸 `float tile_size` 展开成 `tile_t1`(现在认)。

### 注意
- canonical 输出仍 `t#N`(InstPrinter `printTileSrcReg` 用 `getRegisterName(Tile_TOS_N)`=`t#N`),`tile_tN` 只作输入 alias。InstPrinter 不打印 boxed 名。
- FA 测例(`fa_2d_unroll_gmma.cpp`)在 SuperNPUBench two-level-arch,需单独验证(boxed layout tile register 名是 FA 的阻塞点之一)。

## 2026-08-03 补充:task #13 输出寄存器只写 `t`(无编号)的澄清

用户强调:**输出寄存器(destination)只可以写 `t`(无编号),不可以写 `t#1`**;相对索引 `t#1` 只用于输入寄存器(src)。

#13 的 boxed 名 alias 实现**符合此约束**,澄清如下:
- `TILE_DST`(输出 register class)的 MC reg 是 `Tile_T`/`Tile_U`/`Tile_M`/`Tile_N`(**无编号**,encoding 0/1/2/3),**不含** `Tile_TOS_N`(`t#N` 带编号)。RA 用 `Tile_ABS`(含 boxed `Tile_T1`)。
- `MatchLinxV5TileRegisteName` 加的 64 个 boxed 名 alias(`tile_t1`→`Tile_TOS1` 等)映射到 `Tile_TOS_N`(MC `t#N`,**带编号**)。
- 所以 boxed 名 `tile_t1` 在**输入(src)**位置被接受(映射 `t#1`,在 `TILE_SRC` class,合法);在**输出(dst)**位置被拒绝(`Tile_TOS1` 不在 `TILE_DST` class,reg class 验证失败 → Match Error),符合"输出只写 `t`"。
- dst 的 `parseTileRegWithArrow` 和 src 的 `parseTileReg` 共用 `MatchLinxV5TileRegisteName`,但 dst 的 `TILE_DstWithArrow`(TILE_DST class)自动拒绝带编号 reg,无需在 parser 层额外区分。

实测确认:
- src `tile_t1` / `t#1` → 接受(输出 canonical `t#1`)
- dst `t` → 接受;dst `t#1` / `tile_t1` → 拒绝(Match Error)

即 #13 只让**输入**接受 boxed 名,**输出**仍只接受 `t`/`u`/`m`/`n`(无编号),不变。

## 2026-08-03 修正:WIP 完整清单 + 改动恢复确认

handoff:148 之前只列了 `InstrInfo.td`,但实际 B.IOT `<size>` + boxed alias 的 WIP 涉及更多文件。完整未提交 WIP(均已验证 build+回归通过):

**linx-llvm tracked WIP**:
- `llvm/lib/Target/LinxV5/LinxV5InstrInfo.td` — B_IOT_TileSizeAsmOperand + B_IOT_TileSize_Op;B_IOT_*_Dst ins 用它,AsmString dest-suffix `${PE_MASK}${Last}, $DstTile$TSize`;legacy-with-last InstAlias
- `llvm/lib/Target/LinxV5/AsmParser/LinxV5AsmParser.cpp` — `isB_IOT_TileSize` 谓词;`parseTileSizeWithBracket`/`parseGPRWithBracket` 4 处 peekTok.compare(">") 反向 bug 修成 getTok;`MatchLinxV5TileRegisteName` 加 64 个 boxed 名 alias(tile_t1..16/u/m/n → Tile_TOS_N)
- `llvm/lib/Target/LinxV5/LinxV5AsmPrinter.cpp` — PrintAsmOperand `%Z` modifier(imm 0..7 → 512B/1KB/.../32KB 文本无括号)
- `llvm/test/CodeGen/LinxV5/v5-matmul-fixp.ll` / `v5-matmul-local-tile-result.ll` — CHECK 更新到 `->dst<size>`
- `llvm/test/MC/LinxV5/v5-b-iot-non-last-dst.s` — CHECK 更新到 bracket 形式

**linx-llvm untracked tests**:
- `llvm/test/MC/LinxV5/v5-b-iot-tilesize-syntax.s`(7 size + OneSrc/TwoSrc + legacy 兼容)
- `llvm/test/MC/LinxV5/v5-tile-boxed-name-alias.s`(boxed 名 round-trip)

**TileOP tracked WIP**:
- `include/jcore/template_asm.hpp` — 带 Local dst 的 `TSize=%c` 已迁移为 `last, ->%[Dst]<%Z[TileSize]>`；Shared L2S 无 dst 的 2 处 `TSize=` 已删除。

**验证状态(全 PASS,无回归)**:
- MC:10 PASS(v5-b-iot-tilesize-syntax + v5-tile-boxed-name-alias + v5-b-iot-non-last-dst + 既有)+ 7 既有失败(b.iot.s/vcall/mcall/CUBE/relax/MTC_*,从项目最初失败)+ 1 XFAIL
- CodeGen:v5-matmul-fixp/local-tile-result/shared-gmov/peid 全 PASS
- matmul.cpp 端到端编译通过，disasm 使用新格式 `->t<16KB>`；活动源码不再保留 `NoDst_Size`。

**改动恢复说明**:本会话中途 AsmParser/AsmPrinter 改动曾因 git checkout 误操作丢失,已重新补齐(isB_IOT_TileSize/getTok 修复/boxed alias/%Z modifier)。InstrInfo.td 一直未丢。当前 WIP 完整一致,build+回归通过。

**提交计划**(handoff:141 要求 LLVM/TileOP 同步):
- 拆两提交:LLVM MC/parser/printer/AsmParser+AsmPrinter 一个(#17 工作包G),TileOP template_asm.hpp inline-asm 一个。
- boxed alias(#18 工作包H)可并入 LLVM MC 提交或单独。
- 不提交 MultiThreadAdd.cpp(untracked 用户文件)、relax.s.o(临时)。

## 2026-08-11: Shared TLOAD 引用接口的最小实现与边界

### 已实现

- `Linx-TileOP-API/include/jcore/template_asm.hpp` 新增：
  `TLOAD(SharedTile<shp> &dst, const gm_shape &src)`。
- 指令序列与 Shared TLOAD 返回形式一致：`BSTART.TLSU TLOAD` + `B.DIM` +
  `B.IOS mask=..., ->S<n><size>` + `B.IOR`。
- Shared 仍使用 `unsigned long` opaque handle 和 `Sr` constraint；后端仍分配
  绝对 `S0..S255`，未引入相对 Shared 索引。
- 新增 TileOP 编译用例 `test/tileop_api/src/SharedTLoad.cpp`，并加入
  `test/tileop_api/compile.all`。

### 已验证

- 函数内声明 Shared、`TLOAD(shared, src)` 后立即
  `TMOV_S2L_BROADCAST(out, shared)`：编译通过。
- `always_inline` wrapper 接收 `SharedTile&`：内联后编译通过。
- 两种形式均生成同一个绝对寄存器，例如 TLOAD 写 `S0`，后续 S2L 读 `S0`；
  无 Shared→GPR copy，无 `C.B.IOS`。

### 明确缺失

- 非内联函数 `void f(SharedTile<T>&, const GM&)` 仍会在 IR 中形成 Shared handle
  的普通内存 store，并导致 `Shared_ABS -> mixedgpr` 非法 copy。
- 这不是 `LinxV5SharedRegAlloc` 可解决的问题；它发生在 Shared allocator 之前。
- Shared 没有普通 store/spill/cross-function reference ABI。若必须支持非内联引用参数，
  需要单独设计 Shared 参数/返回 ABI，不能复用 Local `PseudoTSTORE`。
- 已验证并撤回 `SharedTile` payload 改成 Local `TileDType` 的实验：它会错误生成
  `Shared_ABS -> Tile_ABS -> PseudoTSTORE`，语义不正确。

## 2026-08-11: 删除 Shared L2S 的重复 B.IOT TSize

- `TMOV_L2S_INSERT/PUBLISH` 的 Shared 目标 size 已由目标形式
  `B.IOS mask=..., ->S<n><size>` 完整承载。
- 无输出 Local source binder 必须使用标准
  `B.IOT src, mask=..., last`；它的编码固定 `TSize=0`。
- 已从 TileOP 两处 inline asm 删除 `TSize=%c[TileSize]`。
- 已将 `PseudoV5SharedL2S` 展开从 `B_IOT_OneSrc_NoDst_Size` 改为
  `B_IOT_OneSrc_NoDst`，并删除遗留且存在编码歧义的
  `B_IOT_OneSrc_NoDst_Size` 指令定义。
- 原 `_NoDst_Size` 编码会与 Local 目标 `->t<size>` 产生相同机器码，objdump
  可能误显示为存在 Local 输出寄存器，因此不能保留。
- 验证结果：SharedMatmul 生成 `B.IOS mask=1111, ->S0<1KB>`，随后为
  `B.IOT t#1, mask=1111, last`；MC 编码为 `[0x13,0xd0,0x0f,0x00]`，
  反汇编保持无输出形式。

## 2026-08-11: Shared L2S 输出参数接口

- 新增以下接口，同时保留原返回式接口：
  - `TMOV_L2S_INSERT(SharedTile<LocalTile>& dst, const LocalTile& src)`；
  - `TMOV_L2S_PUBLISH(SharedTile<LocalTile>& dst, const LocalTile& src)`。
- 返回式接口现在直接复用输出参数实现，避免两套 inline asm 漂移。
- `SharedTile::SetValidShape()` 同步 Local Tile 的运行时 valid row/col，动态
  shape 语义与原 `SharedTile(local)` 构造保持一致。
- 静态 INSERT/PUBLISH、动态 valid metadata 和原返回式接口均编译通过。
- 输出参数形式生成 `B.IOS mask=..., ->S<n><size>` 与无 dst
  `B.IOT local, mask=..., last`，随后 Shared consumer 直接读取同一个 `S<n>`。
- 边界与 Shared TLOAD 相同：函数内或内联 SSA 流程可用；非内联函数若通过
  caller-owned `SharedTile&` 写回，仍需要独立的 Shared cross-function ABI。

## 2026-08-13: FIXP 接口层分派与 Inline ASM 实现设计

### 目标与总体结论

PTO ISA 0.58 的 FIXP 已经统一为 Matrix CUBE 操作的 `B.FPATR`，不再使用独立的
`TMATMUL*_FIXP` 指令函数。因此，第一阶段不必新增 LLVM intrinsic：可以在
TileOP 接口层使用 `FixpAttr` 的编译期字段选择固定签名的重载，并由每个重载生成
固定操作数数量的 inline asm CUBE bundle。后端后续若需要优化调度、寄存器分配或
矩阵依赖，再把相同的 bundle 逐步下沉为 LLVM pseudo/intrinsic。

该方案的硬约束是：

1. `FixpAttr` 必须是 `constexpr` 模板参数，不能在运行时改变 inline asm 的操作数数量；
2. 每个 `if constexpr` 分支必须对应固定的 `B.IOT`/`B.IOR` 数量和固定顺序；
3. API 层必须在实例化期拒绝属性、Tile 类型和附加参数不匹配的组合；
4. inline asm 必须完整表达一个 CUBE bundle，不能只发出 `B.FPATR` 而遗漏参数流；
5. 所有输出应作为一个完整 bundle 提交，失败时不能留下部分可见输出。

### FIXP 字段到输入/输出数量的映射

固定数学输入由操作类型决定：

```text
TMATMUL       A, B
TMATMUL.BIAS  A, B
TMATMUL.ACC   C, A, B       （顺序必须是 C, A, B）
TMATMULMX     A, B
TMATMULMX.BIAS A, B
TMATMULMX.ACC C, A, B
TGEMV/TGEMVMX 按各自规范的数学 source 顺序
```

在数学输入之后，按以下顺序追加可选 Local Tile source：

```text
1. RowMaxIn       ：仅当 RowMaxInit=true
2. QuantParamTile ：仅当 PreQuant 是 vector 模式
3. PReLUParamTile ：仅当 Relu=PRelu
```

`B.IOR` 的 GPR source 必须按 dense 顺序追加：

```text
1. QuantParam     ：仅当 PreQuant 是 scalar 模式
2. LReLUParam     ：仅当 Relu=LRelu
```

输出顺序固定为：

```text
1. D              ：始终存在
2. RowMaxOut      ：仅当 RowMaxEn=true
3. GroupMaxOut    ：仅当 GroupMaxEn=true
```

因此，输入/输出数量确实由 `PreQuant`、`Relu`、`RowMaxInit`、`RowMaxEn` 和
`GroupMaxEn` 决定；`GroupNCode`、`MaxAbsEn` 主要影响语义合法性和 FPATR 编码，
不单独增加操作数。`RowMaxInit=true` 必须同时有 `RowMaxEn=true`，所以通常同时
存在 RowMaxIn 和 RowMaxOut。

### 建议的接口分层

保留一个统一入口，但把不同操作数形态分成编译期可判定的重载或 options 类型：

```cpp
template <FixpAttr Attr, typename D, typename A, typename B>
void TMATMUL(D &, A &, B &);

template <FixpAttr Attr, typename D, typename A, typename B, typename QTile>
void TMATMUL(D &, A &, B &, QTile &);

template <FixpAttr Attr, typename D, typename A, typename B,
          typename RowIn, typename QTile, typename PReluTile,
          typename RowOut, typename GroupOut>
void TMATMUL(D &, A &, B &, RowIn &, QTile &, PReluTile &,
             RowOut &, GroupOut &);
```

实际实现可以继续使用现有 `fixp::Options`/`TMATMUL_FIXP` 形式；不要求为每种属性
组合复制公开函数。推荐的内部结构是：

```cpp
constexpr bool HasVectorQuant = is_vector_fixp_pre_quant(Attr.PreQuant);
constexpr bool HasScalarQuant = is_scalar_fixp_pre_quant(Attr.PreQuant);
constexpr bool HasRowIn = Attr.RowMaxInit;
constexpr bool HasRowOut = Attr.RowMaxEn;
constexpr bool HasGroupOut = Attr.GroupMaxEn;
constexpr bool HasPRelu = Attr.Relu == FixpReluMode::PRelu;
constexpr bool HasLRelu = Attr.Relu == FixpReluMode::LRelu;
```

随后以 `if constexpr` 选择 binder 和 asm 模板。不能把可选参数先放进一个
“可能为空”的通用 asm 操作数列表，因为那会破坏 ISA 的 dense stream 顺序。

### Inline ASM Bundle 生成逻辑

每个固定形状实现应生成如下逻辑顺序，具体语法以现有 TileOP binder 和 PTO 0.58
规范为准：

```asm
BSTART.CUBE TMATMUL[.BIAS|.ACC|...]
B.DATR <DType>, <byte0>, Zero
B.FPATR <PreQuant>, <Relu>, <GroupNCode>, <RowMaxEn>,
        <GroupMaxEn>, <RowMaxInit>, <MaxAbsEn>
B.IOT <mathematical sources in order>
B.IOT <RowMaxIn if present>
B.IOT <QuantParamTile if present>
B.IOT <PReLUParamTile if present>
B.IOR <scalar sources in dense order>
B.IOT mask=15, last, -><D><tile-size>
B.IOT mask=15, last, -><RowMaxOut><tile-size>       # if present
B.IOT mask=15, last, -><GroupMaxOut><tile-size>     # if present
BEND.CUBE
```

注意：上面是实现顺序示意，不是允许随意拼接的字符串。应为每种 source/output
mask 组合提供明确 binder，或用宏生成 2^N 个固定模板。`B.IOR` 中若只有第二个
逻辑参数，不能跳过第一个位置；必须遵守规范的 dense order。Matrix `B.DATR` 的
padding union 必须使用规范要求的 canonical zero 形式，不能继续发出 `Null`。

### API 层必须实现的静态检查

每个入口实例化时至少检查：

```text
FixpAttr 合法：PreQuant 闭合集合、Relu<=3、GroupNCode<=9；
RowMaxInit -> RowMaxEn；
GroupMaxEn == (GroupNCode != 0)；
MaxAbsEn -> RowMaxEn 或 GroupMaxEn；
属性与 destination dtype 的映射正确；
PreQuant=None 只允许 FP32（禁止当前遗留的 S32 alias）；
通用 FP8 输出统一为 E4M3（禁止 E5M2 alias）；
vector PreQuant 必须且只能提供 QuantParam Tile；
scalar PreQuant 必须且只能提供 QuantParam GPR；
PRelu 必须且只能提供 PReLUParam Tile；
LRelu 必须且只能提供 LReLUParam GPR；
RowMaxInit 必须提供 RowMaxIn；RowMaxInit=false 禁止 RowMaxIn；
RowMaxEn 必须提供 RowMaxOut；RowMaxEn=false 禁止 RowMaxOut；
GroupMaxEn 必须提供 GroupMaxOut；GroupMaxEn=false 禁止 GroupMaxOut；
D、RowMaxOut、GroupMaxOut 必须两两不同；RowMaxIn==RowMaxOut 合法；
source 总数不超过 8，destination 总数不超过 3；
TMATMUL.ACC 的数学 source 顺序严格为 C,A,B；
```

旧的 `.FIXP` helper 可以保留兼容入口，但不得再生成已删除的
`TMATMUL*.FIXP` mnemonic/function。应统一转发到普通 Matrix operation + `B.FPATR`。

### 推荐实施步骤（交给实现 agent）

1. **盘点现有入口**：逐个检查 12 个 active Matrix operation 的 public wrapper、
   `fixp::Options`、`emit_fixp`、所有 B.IOT/B.IOR binder；标出仍只支持
   parameter-free 的入口。
2. **先修公共合法性**：修正 `is_fixp_output_type`（None 仅 FP32、FP8 仅 E4M3），
   补全属性组合和参数存在性 `static_assert`，修正 Matrix `B.DATR` canonical zero。
3. **抽取固定 bundle emitter**：将数学 source、可选 Tile source、可选 GPR source、
   destination 按上述规范顺序抽取为统一的 compile-time emitter；每个分支生成固定
   asm 模板，不允许运行时动态操作数。
4. **补全 12 个 wrapper**：让 TMATMUL/TMATMULMX/TGEMV/TGEMVMX 的普通、BIAS、ACC
   入口都能通过统一 `FixpAttr`/Options 路径表达完整动态参数；不要再要求用户选择
   已删除的 `.FIXP` API。
5. **处理输出别名和原子性**：加入 D/RowMaxOut/GroupMaxOut 互斥校验，允许
   RowMaxIn==RowMaxOut，并确保所有输出绑定在同一个 CUBE bundle 中提交。
6. **清理旧路径**：搜索并隔离所有会发出 `TMATMUL*.FIXP` 的 helper、错误信息和
   测试；保留兼容 API 时只允许转发到 active mnemonic。
7. **补回归测试**：至少覆盖 None/F16/BF16、scalar/vector quant、PRelu/LRelu、
   RowMax 输入输出、GroupMax 输出、ACC 的 C,A,B 顺序、非法组合和 dtype 拒绝。
8. **验证边界**：先运行 TileOP API compile tests 和生成汇编检查，再运行 LLVM
   `v5-matmul-fpatr.s`、相关 CodeGen tests；不要把 inline asm 方案宣称为 LLVM
   intrinsic 等价优化路径。

### 不应在本工作包中擅自实现

- 不要把运行时 `FixpAttr` 变量用于决定 asm 操作数数量；
- 不要恢复独立的 `TMATMUL*_FIXP` opcode/function；
- 不要让 LLVM CodeGen 继续默默把所有 FPATR 属性降成全零 None；
- 不要猜 TGEMV 的 C++ 参数 ABI，先对照 PTO 0.58 normative ASL；
- 不要把 Shared Tile 的跨非内联函数 ABI 问题混入 FIXP bundle 修复；
- 不要修改 unrelated TLSU/Shared TLOAD 工作或清理用户未提交文件。

### 实现 agent 的验收标准

完成后必须报告：

```text
修改的文件及每个文件的职责；
每种属性组合实际生成的 BSTART/B.DATR/B.FPATR/B.IOT/B.IOR/BEND 顺序；
12 个 active Matrix operation 的支持矩阵；
参数数量由哪些 constexpr 字段决定；
新增的正向/负向测试及测试命令；
是否仍存在任何会生成 TMATMUL*.FIXP 的路径；
LLVM CodeGen 是否仍默认合成全零 B.FPATR，以及这是暂时限制还是已解决。
```

## 2026-08-13: CmpMode / TCMP / TCMPS 对齐 PTO 0.58 的实现设计

### 规范基线

PTO 0.58 对 Tile 比较操作的比较模式定义为：

```text
EQ = 0
NE = 1
LT = 2
GT = 3
LE = 4
GE = 5
```

`B.DATR` 的 `CMode` 位域位于 `[31:29]`，宽度为 3 bit。规范解码关系位于：

```text
/tmp/pto-spec-audit-20260812/asl/block/attributes/B.DATR.asl
/tmp/pto-spec-audit-20260812/asl/block/model/dispatch/tile-schema.asl
```

规范 `TileComparison` 的语义顺序是：

```text
EQ, NE, LT, LE, GT, GE
```

TCMP/TCMPS 的规范 contract 均将 `comparison` 作为独立操作数，并允许 `B.DATR`
中的非零字段只有 `CMode`；其 padding union 必须为 zero：

```text
TCMP  : destination0, source0, source1, comparison
TCMPS : destination0, source0, scalar0, comparison
```

规范来源：

```text
/tmp/pto-spec-audit-20260812/asl/tile/elementwise-tile-tile/logical/TCMP.asl
/tmp/pto-spec-audit-20260812/asl/tile/tile-scalar-and-immediate/logical/TCMPS.asl
/tmp/pto-spec-audit-20260812/asl/tile/model/state/types.asl
/tmp/pto-spec-audit-20260812/asl/tile/model/execution/elementwise.asl
```

### 当前差异

#### TileOP `CmpMode` 枚举编码错误

当前文件：

```text
/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/include/common/pto_tile.hpp
```

现有枚举顺序是 `EQ, NE, GT, LT, GE, LE`，若枚举值直接用于硬件编码，会造成：

```text
GT 当前为 2，但 ISA 要求 3
LT 当前为 3，但 ISA 要求 2
GE 当前为 4，但 ISA 要求 5
LE 当前为 5，但 ISA 要求 4
```

必须改为显式值，避免依赖枚举声明顺序：

```cpp
enum class CmpMode : uint8_t {
  EQ = 0,
  NE = 1,
  LT = 2,
  GT = 3,
  LE = 4,
  GE = 5,
};
```

不得通过隐式 `static_cast` 掩盖非法值；建议增加：

```cpp
constexpr bool is_valid_cmp_mode(CmpMode Mode) {
  switch (Mode) {
  case CmpMode::EQ:
  case CmpMode::NE:
  case CmpMode::LT:
  case CmpMode::GT:
  case CmpMode::LE:
  case CmpMode::GE:
    return true;
  }
  return false;
}
```

#### TileOP 硬件 TCMP/TCMPS 缺少比较模式

当前文件：

```text
/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/include/jcore/template_asm.hpp
```

当前 `TCMP` 和 `TCMPS` wrapper 只发出固定的 `BSTART.TEPL` 和 `B.IOT/B.IOR`，没有：

1. `CmpMode` 的前端参数；
2. `B.DATR`；
3. `CMode[31:29]` 编码；
4. 针对六种比较模式的静态合法性检查。

因此当前硬件路径无法表达除默认比较之外的完整 TCMP/TCMPS 语义。不能只修正枚举值，必须同时修改 public API 和 inline asm Bundle。

### TileOP 前端 API 设计

比较模式应是编译期参数，推荐使用模板参数，不能使用运行时参数决定 `B.DATR`
的属性操作数布局：

```cpp
template <CmpMode Mode, is_tile_data_v tile_shape>
PTO_SHARED_INLINE void TCMP(tile_shape &dst, tile_shape &src0,
                            tile_shape &src1);

template <CmpMode Mode, is_tile_data_v tile_shape>
PTO_SHARED_INLINE void TCMPS(tile_shape &dst, tile_shape &src,
                             typename tile_shape::DType scalar);
```

建议保留旧无 mode 入口的方式取决于兼容性要求：

- 如果旧 API 约定默认 EQ，可以保留为 `TCMP<CmpMode::EQ>(...)` 的转发 wrapper；
- 如果无法判断旧 API 的默认语义，不能静默保留无 mode 入口；应标记 deprecated，
  并让用户显式选择 mode；
- 不建议增加 `TCMP(dst, src0, src1, CmpMode mode)` 作为唯一硬件实现，因为运行时
  mode 会让编译器无法把 CMode 作为稳定的 inline asm immediate；若确实需要运行时
  mode，必须显式实现六路分支，每一路调用一个编译期 mode wrapper。

为了兼容现有 CPU simulator 的调用方式，可以提供统一的 compile-time 核心实现：

```cpp
template <CmpMode Mode, typename D, typename A, typename B>
PTO_SHARED_INLINE void TCMP(D &dst, A &src0, B &src1) {
  static_assert(is_valid_cmp_mode(Mode), "invalid CmpMode");
  static_assert(tile_type_compatible<D, A, B>,
                "TCMP tile types are incompatible");
  emit_tcmp<Mode>(dst, src0, src1);
}
```

`TCMPS` 同理：

```cpp
template <CmpMode Mode, typename D, typename S>
PTO_SHARED_INLINE void TCMPS(D &dst, S &src,
                             typename S::DType scalar) {
  static_assert(is_valid_cmp_mode(Mode), "invalid CmpMode");
  emit_tcmps<Mode>(dst, src, scalar);
}
```

具体 concept 名称应适配当前库已有 trait，不能为了本修复引入重复的类型系统。

### Inline ASM 实现逻辑

`TCMP` 的 Bundle 必须表达：

```asm
BSTART.TEPL 13, <DataType>
B.DATR <layout/data type form>, <pad/byte form>, <CMode>, <RMode>, <Sat>
B.DIM <LB0 setup>
B.DIM <LB1 setup>
B.DIM <LB2 setup>
B.IOT <src0>, <src1>, mask=15, last, -><dst><tile-size>
BSTOP
```

`TCMPS` 的 Bundle 必须表达：

```asm
BSTART.TEPL 45, <DataType>
B.DATR <layout/data type form>, <pad/byte form>, <CMode>, <RMode>, <Sat>
B.DIM <LB0 setup>
B.DIM <LB1 setup>
B.DIM <LB2 setup>
B.IOT <src>, mask=15, last, -><dst><tile-size>
B.IOR [<scalar>], []
BSTOP
```

实际 B.DATR 文本形式必须复用当前 TileOP 的正确 binder/宏，不要手工复制多套
语法。关键要求是：

```text
CMode = static_cast<unsigned>(Mode)
padding = Zero/canonical zero
RMode/Sat = 按 TCMP/TCMPS 规范默认值，不能使用未初始化值
```

TCMP 的两个 Tile source 顺序必须是 `src0, src1`；TCMPS 的 scalar 必须通过规范
要求的 `B.IOR` 位置传递，不能把 scalar 当作 Tile source 或普通内存输入。

如果当前 LLVM 汇编器要求完整形式的 `B.DATR`，应先确认现有语法和 operand order，
再由 inline asm 使用完整 canonical 形式；不要继续使用没有 CMode 的简化字符串。

### CPU simulator 对齐

当前 CPU simulator：

```text
/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/include/cpu_sim/TCmp.hpp
```

已经按 mode 分派比较语义，但必须配合枚举值修正，并检查以下语义顺序：

```text
EQ : left == right
NE : left != right
LT : signed(left) < signed(right)
GT : signed(left) > signed(right)
LE : signed(left) <= signed(right)
GE : signed(left) >= signed(right)
```

对浮点类型的比较应遵守 PTO 0.58 `TileCompareValue`/MX profile 规则，不能简单用
整数比较逻辑替代；如果当前 CPU simulator 尚未覆盖浮点、NaN、signed zero，应在
handoff/测试中明确标记为未覆盖，不要宣称与硬件完全一致。

运行时 `TCMP_Impl(..., CmpMode mode)` 可以保留用于 simulator，但其 `switch` 必须
覆盖六个合法值并对 default 失败；硬件 inline asm 入口仍应优先使用 compile-time
mode。

### LLVM 侧需要检查和完成的内容

LLVM 当前 `CmpMode` 基础定义和 parser 映射已经与 ISA 一致：

```text
EQ=0, NE=1, LT=2, GT=3, LE=4, GE=5
B.DATR Inst[31:29] = CmpMode
```

相关文件：

```text
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5BaseInfo.h
llvm/lib/Target/LinxV5/AsmParser/LinxV5AsmParser.cpp
llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5InstPrinter.cpp
llvm/lib/Target/LinxV5/LinxV5InstrInfo.td
```

但仍需补充 MC 合法性测试：

1. `B.DATR ... EQ/NE/LT/GT/LE/GE` 分别检查 `[31:29]` 编码；
2. round-trip 反汇编名称检查；
3. 非法 CMode `6`、`7` 拒绝或按规范处理；
4. 检查简化 alias 与完整 B.DATR 形式编码一致；
5. 检查 TCMP/TCMPS 生成的 B.DATR padding 和 CMode 不被后端丢失。

LLVM 后端当前对部分 Tile operation 可能在展开阶段默认合成 `CmpMode::EQ` 的
`B.DATR`。这对没有显式比较属性的旧路径只能作为兼容默认，不能覆盖新的
`TCMP<Mode>` 语义。若 LLVM CodeGen 需要支持非 EQ 的 TCMP/TCMPS，必须把 mode
从 pseudo/inline asm operand 传到 `B.DATR`，不能在 emitter 中无条件写 EQ。

### 推荐实施顺序

1. **先修枚举和公共编码 helper**：显式设置六个 ISA 数值，增加合法性和编码 helper。
2. **补 TileOP compile-time API**：增加 `TCMP<Mode>` 和 `TCMPS<Mode>`，设计旧 API
   的兼容/deprecation 行为。
3. **抽取 B.DATR emitter**：统一生成 zero padding、CMode、默认 RMode/Sat 的形式，
   避免 TCMP/TCMPS 各自拼接错误语法。
4. **补硬件 inline asm**：把 `Mode` 作为 immediate 发入 B.DATR，保持 Tile source、
   scalar、destination 的规范顺序。
5. **修 CPU simulator**：同步枚举顺序，验证六种关系和类型语义；补运行时 mode 的
   default 错误处理。
6. **完善 LLVM MC/CodeGen**：补六种编码、非法值、round-trip 和 mode 传递测试。
7. **清理旧接口风险**：搜索所有 `TCMP(`、`TCMPS(`、`CmpMode` 使用点，避免旧调用
   继续依赖错误顺序或默认 EQ；同步更新 API 文档和示例。

### 验收测试

TileOP 至少需要：

```text
TCMP<EQ/NE/LT/GT/LE/GE> 编译并检查 B.DATR CMode=0..5；
TCMPS<EQ/NE/LT/GT/LE/GE> 编译并检查 B.DATR CMode=0..5；
检查 B.DATR padding 为 Zero/canonical zero；
检查 TCMP source0/source1 顺序；
检查 TCMPS scalar 位于 B.IOR 的正确槽位；
CPU simulator 六种 mode 的结果测试；
非法 mode 无法实例化；
```

LLVM 至少需要：

```text
llvm-mc 汇编六种 B.DATR CMode；
检查精确机器码 [31:29]；
反汇编 round-trip；
CMode=6/7 的负向测试；
TCMP/TCMPS CodeGen 或 inline-asm 输出保留 mode；
```

实现 agent 完成后必须报告：

```text
修改文件和职责；
CmpMode 的最终数值表；
TCMP/TCMPS 新旧 API 行为；
每种 mode 的实际 B.DATR 文本与编码；
CPU simulator 与硬件语义差异；
新增测试及命令；
是否还有任何路径默认丢失 CMode 或发出无 CMode 的 TCMP/TCMPS Bundle。
```

### 不应混入本工作包的内容

- 不要修改与比较模式无关的 TLSU、Shared TLOAD 或 FIXP 工作；
- 不要仅通过调整 `CmpMode` 枚举顺序而跳过硬件 Bundle 的 `B.DATR` 修复；
- 不要把运行时 mode 直接作为可变 inline asm operand 数量或 mnemonic；
- 不要猜测 PTO 0.58 未定义的浮点比较扩展语义；
- 不要在未验证现有用户调用点前删除旧 API；
- 不要把 CPU simulator 的整数行为直接宣称为完整 ISA 浮点一致性。

## 2026-08-13: Tile datatype reinterpret 前端接口设计

### 目标

在 TileOP C++ 前端增加一种零指令的 Tile datatype reinterpret 语义：底层 Tile
寄存器或存储位模式不变，只改变后续 Tile operation 使用的静态 `DType` 和对应
ISA datatype encoding。第一阶段通过 header-only Tile view 实现，不新增 LLVM
intrinsic，不生成 `TCVT`，也不执行任何数值转换。

推荐用户接口：

```cpp
auto as_i32 = reinterpret_tile<int32_t>(as_fp32);
auto as_bf16 = reinterpret_tile<__bf16>(as_fp16);
```

该接口的含义必须明确为：

```text
bits 不变
底层 Tile register/storage 不变
Location 不变
layout 不变
Rows/Cols 不变
ValidRow/ValidCol 不变
物理字节数和 TileSizeCode 不变
后续指令使用 NewDType 的 TypeCode
不发出 TCVT 或其他硬件指令
```

### 与 TCVT 的语义区别

必须在 API 文档和测试中明确区分：

```text
reinterpret_tile<NewDType>(src)
    位模式重解释；零指令；不舍入；不改变数值位模式。

TCVT(dst, src)
    数值格式转换；生成硬件 TCVT；可能舍入、饱和或改变位模式。
```

示例：

```cpp
auto bits = reinterpret_tile<int32_t>(fp32_tile); // FP32 bits 作为 S32 读取
TCVT(int_tile, fp32_tile);                        // FP32 数值转换为 S32
```

不能用 `reinterpret_tile` 替代 FIXP/TCVT 的数值转换，也不能让该接口静默生成
`TCVT`。

### 第一阶段范围

第一阶段只支持：

```text
Local Tile -> Local reinterpret view
源和目标 datatype 位宽相同
保持 shape、layout、valid shape 和 Location 完全不变
不创建新的独立 Tile storage
不支持跨 Local/Shared Location 转换
```

第一阶段可允许的典型组合：

```text
FP32 <-> S32/U32
FP16 <-> BF16/U16/S16
S8 <-> U8/FP8（仅当当前类型系统和 layout trait 明确支持）
```

第一阶段必须拒绝：

```text
FP32 -> FP16
FP16 -> FP32
S8 -> FP16
Local -> Shared
Shared -> Local
任何导致物理 Tile bytes 或 TileSizeCode 改变的组合
任何导致当前 boxed/fractal layout 非法的组合
```

不同位宽 reinterpret 会改变逻辑元素数量和 shape 解释，不属于本工作包。若后续
确实需要，应另行设计 `reshape_reinterpret_tile`，明确指定新 Rows/Cols/valid shape，
不能扩展当前简单接口的隐式行为。

### 推荐类型设计

不要构造一个新的独立 `Tile<NewDType,...>` 对象，因为这可能引入初始化、copy、
新的虚拟 Tile register 或 storage。推荐增加一个轻量、只持有源 Tile 引用的 view：

```cpp
template <typename NewDType, typename SourceTile>
class ReinterpretedTileView {
public:
  using DType = NewDType;
  using Source = SourceTile;
  using TileDType = /* 与 Source 相同 storage carrier 的 view 类型 */;

  static constexpr auto Location = /* Source 的 Location */;
  static constexpr int Rows = SourceTile::Rows;
  static constexpr int Cols = SourceTile::Cols;
  static constexpr int ValidRow = SourceTile::ValidRow;
  static constexpr int ValidCol = SourceTile::ValidCol;
  static constexpr auto Layout = /* Source 的 BLayout/SLayout */;

  explicit constexpr ReinterpretedTileView(SourceTile &Source)
      : SourceValue(Source) {}

  decltype(auto) data() { return SourceValue.data(); }
  decltype(auto) data() const { return SourceValue.data(); }

  auto GetValidRow() const { return SourceValue.GetValidRow(); }
  auto GetValidCol() const { return SourceValue.GetValidCol(); }

private:
  SourceTile &SourceValue;
};
```

以上只是结构示意，实际成员名必须复用当前 `Tile` 的公开静态属性和 trait，不要猜测
不存在的 `Location`/`Layout` 成员。实现 agent 应先阅读：

```text
/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/include/common/pto_tile.hpp
```

尤其是：

```text
Tile 模板定义
TileLeft/TileRight/TileAcc aliases
tile_type_traits
type_traits
is_tile_data_v
is_local_tile_v
is_shared_tile_v
tile_role_v
```

`ReinterpretedTileView` 必须能被现有 Local Tile API concept/trait 识别，但不能被识别
为 Shared Tile。

### 推荐公开接口

```cpp
template <typename NewDType, is_local_tile_v SourceTile>
PTO_SHARED_INLINE auto reinterpret_tile(SourceTile &Source) {
  using OldDType = typename SourceTile::DType;

  static_assert(is_supported_tile_dtype_v<NewDType>,
                "reinterpret_tile target dtype is unsupported");
  static_assert(type_traits<OldDType>::bits == type_traits<NewDType>::bits,
                "reinterpret_tile requires equal-width datatypes");
  static_assert(reinterpret_tile_layout_legal_v<SourceTile, NewDType>,
                "reinterpret_tile target dtype is incompatible with layout");
  static_assert(reinterpret_tile_storage_compatible_v<SourceTile, NewDType>,
                "reinterpret_tile must preserve physical Tile storage");

  return ReinterpretedTileView<NewDType, SourceTile>(Source);
}
```

如果当前代码库没有 `is_supported_tile_dtype_v`，应基于已有 `type_traits<T>` 和有效
`TypeCode` 设计最小 helper，不要引入第二套 datatype registry。

建议同时提供 const 版本：

```cpp
template <typename NewDType, is_local_tile_v SourceTile>
PTO_SHARED_INLINE auto reinterpret_tile(const SourceTile &Source);
```

const view 不得通过 `data()` 暴露可写访问。

不建议公开 ISA 整数编码接口：

```cpp
reinterpret_tile<5>(src); // 不推荐
```

用户接口应使用 C++ datatype，内部通过：

```cpp
type_traits<NewDType>::TypeCode
```

获得 ISA encoding。

### Storage carrier 与 `data()` 设计要求

这是实现中的关键点。后续 inline asm 必须：

```text
继续引用 Source 的同一个 Tile register/storage
使用与 Source 相同的 Tr/T 寄存器 constraint
不生成 copy
不创建第二个虚拟 Tile payload
```

因此 `data()` 不能把源 payload 做普通 C++ pointer reinterpret 后再创建新对象。它应
直接返回源 Tile 的 register carrier；新 dtype 只通过 view 的 `DType` 类型别名影响
后续接口中的：

```cpp
type_traits<typename TileView::DType>::TypeCode
```

如果现有 API 强制要求 `TileDType` 与 `DType` 绑定，应增加专门的 view trait，而不是
通过复制源数据构造一个新 Tile。

需要确认下面这些接口对 view 的使用方式：

```text
TMATMUL/TGEMV
TADD/TCMP 等 TEPL operation
TCVT
TSTORE
B.IOT Local Tile binder
TileSizeCode 查询
```

对于只读取 `DType`、shape、valid shape、`data()` 的 API，应直接兼容。对于依赖
`TileDType` 精确类型匹配的 API，应扩展 `tile_type_traits` 或增加
`tile_storage_traits<View>`，使 storage bytes/TileSizeCode 始终来自 Source。

### 必须实现的编译期约束

至少实现以下检查：

```text
Source 必须是 Local Tile；
NewDType 必须有合法 PTO type_traits/TypeCode；
OldDType bits == NewDType bits；
reinterpret 前后物理总字节数相同；
reinterpret 前后 TileSizeCode 相同；
Location/role 不变；
Rows/Cols 不变；
ValidRow/ValidCol 不变；
BLayout/SLayout/boxed/fractal 参数不变；
目标 dtype 在当前 layout 下合法；
禁止 view 指向临时 Tile，避免悬空引用；
```

若 Source 使用动态 valid shape，view 的 `GetValidRow()`/`GetValidCol()` 必须转发到
Source，不能缓存一份可能失效的值。

### Layout 合法性

仅检查 datatype 位宽相同可能仍不足。boxed/fractal layout 可能对 element type、
inner shape 或 packing 有额外约束。推荐 helper：

```cpp
template <typename SourceTile, typename NewDType>
constexpr bool reinterpret_tile_layout_legal_v = /* instantiate or query the
  existing layout/tile traits without allocating a new Tile */;
```

必须复用现有 layout 静态约束。如果无法安全验证某类 boxed/fractal layout，第一阶段
应明确拒绝，而不是放宽检查。

建议第一阶段支持矩阵 API 当前常用的等宽组合和普通 RowMajor/ColMajor；对未经测试的
特殊 boxed layout 使用 `static_assert` 拒绝，并在后续单独扩展。

### API 使用示例

```cpp
using F32Tile = Tile<Location::Vec, float, 32, 32, BLayout::RowMajor>;
F32Tile src;

auto s32_view = reinterpret_tile<int32_t>(src);
TSTORE(dst_s32, s32_view);
```

矩阵操作示例：

```cpp
TileLeft<float, M, K> a_fp32;
auto a_s32 = reinterpret_tile<int32_t>(a_fp32);

// 后续接口读取 a_s32 的 DType/TypeCode，但 B.IOT 仍绑定 a_fp32 的同一 Tile。
TMATMUL(d, a_s32, b_s32);
```

实现时需注意 `TileLeft/TileRight` role 必须由 Source 转发，不能因为 view 类型改变而
丢失 `Location::Left/Right`。

### Shared Tile 的后续阶段

第一阶段禁止 Shared Tile。后续若实现 Shared view，应使用独立设计：

```cpp
auto shared_s32 = reinterpret_shared_tile<int32_t>(shared_fp32);
```

或让同一个 `reinterpret_tile` 通过专门的 Shared view specialization 实现。必须保证：

```text
Shared handle 不变；
仍使用 Sr constraint；
仍通过 B.IOS 绑定；
Shared descriptor size 不变；
不能退化成 Local Tile 或 GPR；
不能产生 Shared -> Local copy；
```

在没有这些验证前，不要让 `is_shared_tile_v` 类型通过通用 Local view 路径。

### 不需要修改 LLVM 的条件

第一阶段如果满足以下条件，可以只修改 TileOP：

```text
view 不生成新 IR operation；
view 只保留源 Tile carrier 引用；
所有消费操作最终仍通过现有 inline asm；
inline asm constraint 与源 Tile 完全相同；
新 dtype 只影响立即数 TypeCode；
不要求 LLVM 理解或优化 reinterpret 语义；
```

此时 LLVM 只看到现有 Tile register operand 和最终 assembly immediate，不需要新增：

```text
intrinsic
builtin
MachineInstr opcode
pseudo
MC encoding
```

如果编译结果出现 Tile-to-Tile copy、普通 memory copy、mixed GPR copy，说明 view
实现错误，不能通过增加 LLVM copy 支持来掩盖。

### 推荐文件职责

预计修改范围：

```text
include/common/pto_tile.hpp
    ReinterpretedTileView、traits、reinterpret_tile API、静态约束。

include/common/pto_tileop.hpp
    如该文件负责导出公共接口，则加入必要 include/alias；避免重复定义。

include/jcore/template_asm.hpp
    原则上不应为 reinterpret 新增硬件指令；仅在现有 concept/trait 无法接受 view 时
    做最小兼容调整。

include/cpu_sim/*
    通常不需要生成操作；如 CPU simulator 需要访问 data，应保证相同 storage 的位模式
    view，不做数值转换。

test/tileop_api/src/TReinterpretTile.cpp
    正向编译和真实汇编检查入口。

test/tileop_api/src/TReinterpretTileInvalid*.cpp
    若测试框架支持 expected-fail，加入负向实例化测试。

docs/tileop-usage/reinterpret-tile.md
    说明 reinterpret 与 TCVT 的区别、允许组合和限制。
```

### 推荐实施步骤

1. 盘点 `Tile`、`TileDType`、`tile_type_traits` 和 inline asm constraint 对 storage carrier
   的真实依赖。
2. 定义不拥有 storage 的 `ReinterpretedTileView<NewDType, SourceTile>`。
3. 扩展现有 tile traits，使 view 保留 Source 的 Local/Left/Right/Acc role、shape、
   layout、physical bytes 和 TileSizeCode。
4. 实现 `reinterpret_tile<NewDType>(Source)` 与 const overload。
5. 增加等宽、layout、storage、Location 和临时对象约束。
6. 用简单 TEPL/TSTORE 操作验证 view 被现有接口接受且没有额外 copy。
7. 用 TMATMUL/TGEMV 验证 Left/Right role 和 TypeCode 已切换但 Tile binder 未改变。
8. 检查生成汇编不包含 `TCVT`，并确认消费指令的 datatype immediate 是 NewDType。
9. 增加不同位宽、Shared、非法 layout、临时对象等负向测试。
10. 更新文档并运行最小相关测试；不要顺手修改无关 FIXP/CmpMode/TLSU 工作。

### 必须增加的测试

正向测试至少覆盖：

```text
FP32 -> S32 Local RowMajor；
S32 -> FP32 Local RowMajor；
FP16 -> BF16；
Left Tile role 保留；
Right Tile role 保留；
动态 ValidRow/ValidCol 转发；
消费操作使用 NewDType TypeCode；
B.IOT 使用原 Tile storage；
生成汇编中没有 TCVT；
reinterpret view 本身不产生任何指令或 copy；
```

如当前 toolchain 支持，增加 object/objdump 检查：

```text
reinterpret 前后使用同一个 Tile register；
Tile size suffix/descriptor size 不变；
后续 BSTART/B.DATR 中 datatype encoding 使用 NewDType；
```

负向测试至少覆盖：

```text
FP32 -> FP16 拒绝；
S8 -> FP16 拒绝；
Shared Tile 通过第一阶段接口拒绝；
非法/无 TypeCode 的 C++ 类型拒绝；
导致 boxed/fractal layout 不合法的目标 dtype 拒绝；
对临时 Tile 调用导致悬空 view 的形式拒绝；
```

### 验收标准

实现 agent 完成后必须报告：

```text
修改的文件及职责；
最终公开 API 签名；
允许和禁止的 datatype 组合；
view 如何保持原 storage carrier 和 TileSizeCode；
哪些现有 Tile concepts/traits 被扩展；
Local Left/Right/Acc role 是否保留；
生成汇编中是否完全没有 TCVT；
至少一个消费指令使用 NewDType TypeCode 的证据；
是否产生额外 Tile copy、memory copy 或 GPR copy；
正向/负向测试及测试命令；
Shared Tile 是否仍明确拒绝。
```

### 不应做的事情

- 不要把 reinterpret 实现成 `TCVT`；
- 不要复制 Tile payload 到新的 Tile 对象；
- 不要只做普通 C++ pointer `reinterpret_cast` 后绕过 Tile traits；
- 不要允许不同位宽而保持原 Rows/Cols；
- 不要改变 Source 的 Location、layout 或 valid shape；
- 不要让 Shared Tile 落入 Local `Tr` constraint；
- 不要新增 LLVM intrinsic/pseudo，除非证明现有 inline asm carrier 无法表达零指令 view；
- 不要为了让测试通过而加入错误的 Tile-to-Tile copy；
- 不要混入 FIXP、CmpMode、TLSU 或 Shared ABI 的无关修改。

## 2026-08-13: reinterpret_tile 接口验证与提交状态

### 当前实现

TileOP 工作区已加入零指令 datatype reinterpret view：

```text
/home/zhuwei/linx-BLK-build/src/Linx-TileOP-API/include/common/pto_tile.hpp
```

公开入口：

```cpp
template <typename NewDType, is_tile_data_v SourceTile>
auto reinterpret_tile(SourceTile &Source);
```

当前实现通过 `ReinterpretedTileView<NewDType, SourceTile>` 保留源 Tile 的：

```text
TileDType/storage carrier
Location/role
Rows/Cols
ValidRow/ValidCol
layout/fractal 属性
physical bytes
TileSizeCode
```

目标 `NewDType` 只影响静态 `DType` 和后续 operation 查询的 PTO datatype encoding；
接口本身不发 `TCVT`，不复制 Tile payload，并且第一阶段只允许 Local Tile。

### 已执行验证

使用 PTO 仓库提供的 host type shim 进行了类型层正向验证：

```bash
cd /home/zhuwei/linx-BLK-build/src/Linx-TileOP-API
clang++ -std=c++20 -D__linx \
  -include test/linx_host_type_shim.hpp -Iinclude \
  -fsyntax-only /tmp/test_reinterpret_tile_type.cpp
```

验证内容包括：

```text
reinterpret_tile<int32_t>(Tile<float>) 可以实例化；
view.DType == int32_t；
view.TileDType 与源 Tile 相同；
Rows/Cols 不变；
Location 不变；
TileSizeCode 不变；
ValidRow 可以从源 Tile 转发。
```

结果：通过，退出码为 0。

已执行两个负向验证：

```text
FP32 -> FP16：被 equal-bit-width/storage static_assert 拒绝；
普通 C++ 未注册 datatype：被 no PTO TypeCode static_assert 拒绝。
```

### 当前限制与未验证项

系统宿主 clang 不支持 Linx 专用 `Tr` inline-asm constraint，因此以下命令不能作为
真实 Linx 汇编验证：

```text
普通 host clang + template_asm.hpp 的完整消费操作；
TMATMUL/TSTORE 经过 reinterpret view 的 Linx object/objdump 验证。
```

这不影响上述类型层验证，但实现 agent 后续仍需使用支持 `linx64` 和 `Tr` constraint
的目标工具链补充：

```text
消费操作没有产生 Tile copy/memory copy/GPR copy；
TMATMUL/TEPL/TSTORE 接受 view；
消费指令使用 NewDType TypeCode；
生成汇编中没有 TCVT；
Shared Tile 仍被明确拒绝。
```

当前接口只有命名 Local Tile 左值入口；尚未增加独立 const view overload。Shared
reinterpret 属于后续工作包，不应由当前通用 Local view 路径放行。

### 提交状态

实现提交到 TileOP `linx` 分支后，handoff 本身提交到 LLVM `dev-llvm15_56` 分支；
两者分别使用各自仓库的远端。不得把 TileOP 头文件误提交到 LLVM 仓库。
